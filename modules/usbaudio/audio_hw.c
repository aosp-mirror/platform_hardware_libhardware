/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "usb_audio_hw"
/*#define LOG_NDEBUG 0*/

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>

#include <log/log.h>
#include <cutils/str_parms.h>
#include <cutils/properties.h>

#include <hardware/hardware.h>
#include <system/audio.h>
#include <hardware/audio.h>

#include <tinyalsa/asoundlib.h>

/* This is the default configuration to hand to The Framework on the initial
 * adev_open_output_stream(). Actual device attributes will be used on the subsequent
 * adev_open_output_stream() after the card and device number have been set in out_set_parameters()
 */
#define OUT_PERIOD_SIZE 1024
#define OUT_PERIOD_COUNT 4
#define OUT_SAMPLING_RATE 44100

struct pcm_config default_alsa_out_config = {
    .channels = 2,
    .rate = OUT_SAMPLING_RATE,
    .period_size = OUT_PERIOD_SIZE,
    .period_count = OUT_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
};

/*
 * Input defaults.  See comment above.
 */
#define IN_PERIOD_SIZE 1024
#define IN_PERIOD_COUNT 4
#define IN_SAMPLING_RATE 44100

struct pcm_config default_alsa_in_config = {
    .channels = 2,
    .rate = IN_SAMPLING_RATE,
    .period_size = IN_PERIOD_SIZE,
    .period_count = IN_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
    .start_threshold = 1,
    .stop_threshold = (IN_PERIOD_SIZE * IN_PERIOD_COUNT),
};

struct audio_device {
    struct audio_hw_device hw_device;

    pthread_mutex_t lock; /* see note below on mutex acquisition order */

    /* output */
    int out_card;
    int out_device;

    /* input */
    int in_card;
    int in_device;

    bool standby;
};

struct stream_out {
    struct audio_stream_out stream;

    pthread_mutex_t lock;               /* see note below on mutex acquisition order */
    struct pcm *pcm;                    /* state of the stream */
    bool standby;

    struct audio_device *dev;           /* hardware information */

    void * conversion_buffer;           /* any conversions are put into here
                                         * they could come from here too if
                                         * there was a previous conversion */
    size_t conversion_buffer_size;      /* in bytes */
};

/*
 * Output Configuration Cache
 * FIXME(pmclean) This is not reentrant. Should probably be moved into the stream structure
 * but that will involve changes in The Framework.
 */
static struct pcm_config cached_output_hardware_config;
static bool output_hardware_config_is_cached = false;

struct stream_in {
    struct audio_stream_in stream;

    pthread_mutex_t lock; /* see note below on mutex acquisition order */
    struct pcm *pcm;
    bool standby;

    struct audio_device *dev;

    struct audio_config hal_pcm_config;

//    struct resampler_itfe *resampler;
//    struct resampler_buffer_provider buf_provider;

    int read_status;

    // We may need to read more data from the device in order to data reduce to 16bit, 4chan */
    void * conversion_buffer;           /* any conversions are put into here
                                         * they could come from here too if
                                         * there was a previous conversion */
    size_t conversion_buffer_size;      /* in bytes */
};

/*
 * Input Configuration Cache
 * FIXME(pmclean) This is not reentrant. Should probably be moved into the stream structure
 * but that will involve changes in The Framework.
 */
static struct pcm_config cached_input_hardware_config;
static bool input_hardware_config_is_cached = false;

/*
 * Utility
 */
/*
 * Translates from ALSA format ID to ANDROID_AUDIO_CORE format ID
 * (see master/system/core/include/core/audio.h)
 * TODO(pmclean) Replace with audio_format_from_pcm_format() (in hardware/audio_alsaops.h).
 *   post-integration.
 */
static audio_format_t alsa_to_fw_format_id(int alsa_fmt_id)
{
    switch (alsa_fmt_id) {
    case PCM_FORMAT_S8:
        return AUDIO_FORMAT_PCM_8_BIT;

    case PCM_FORMAT_S24_3LE:
        //TODO(pmclean) make sure this is the 'right' sort of 24-bit
        return AUDIO_FORMAT_PCM_8_24_BIT;

    case PCM_FORMAT_S32_LE:
    case PCM_FORMAT_S24_LE:
        return AUDIO_FORMAT_PCM_32_BIT;
    }

    return AUDIO_FORMAT_PCM_16_BIT;
}

/*
 * Data Conversions
 */
/*
 * Convert a buffer of PCM16LE samples to packed (3-byte) PCM24LE samples.
 *   in_buff points to the buffer of PCM16 samples
 *   num_in_samples size of input buffer in SAMPLES
 *   out_buff points to the buffer to receive converted PCM24 LE samples.
 * returns
 *   the number of BYTES of output data.
 * We are doing this since we *always* present to The Framework as A PCM16LE device, but need to
 * support PCM24_3LE (24-bit, packed).
 * NOTE:
 *   We're just filling the low-order byte of the PCM24LE samples with 0.
 *   This conversion is safe to do in-place (in_buff == out_buff).
 * TODO(pmclean, hung) Move this to a utilities module.
 */
static size_t convert_16_to_24_3(const short * in_buff, size_t num_in_samples, unsigned char * out_buff) {
    /*
     * Move from back to front so that the conversion can be done in-place
     * i.e. in_buff == out_buff
     */
    int in_buff_size_in_bytes = num_in_samples * 2;
    /* we need 3 bytes in the output for every 2 bytes in the input */
    int out_buff_size_in_bytes = ((3 * in_buff_size_in_bytes) / 2);
    unsigned char* dst_ptr = out_buff + out_buff_size_in_bytes - 1;
    size_t src_smpl_index;
    const unsigned char* src_ptr = ((const unsigned char *)in_buff) + in_buff_size_in_bytes - 1;
    for (src_smpl_index = 0; src_smpl_index < num_in_samples; src_smpl_index++) {
        *dst_ptr-- = *src_ptr--; /* hi-byte */
        *dst_ptr-- = *src_ptr--; /* low-byte */
        /*TODO(pmclean) - we might want to consider dithering the lowest byte. */
        *dst_ptr-- = 0;          /* zero-byte */
    }

    /* return number of *bytes* generated */
    return out_buff_size_in_bytes;
}

/*
 * Convert a buffer of packed (3-byte) PCM24LE samples to PCM16LE samples.
 *   in_buff points to the buffer of PCM24LE samples
 *   num_in_samples size of input buffer in SAMPLES
 *   out_buff points to the buffer to receive converted PCM16LE LE samples.
 * returns
 *   the number of BYTES of output data.
 * We are doing this since we *always* present to The Framework as A PCM16LE device, but need to
 * support PCM24_3LE (24-bit, packed).
 * NOTE:
 *   We're just filling the low-order byte of the PCM24LE samples with 0.
 *   This conversion is safe to do in-place (in_buff == out_buff).
 * TODO(pmclean, hung) Move this to a utilities module.
 */
static size_t convert_24_3_to_16(const unsigned char * in_buff, size_t num_in_samples, short * out_buff) {
    /*
     * Move from front to back so that the conversion can be done in-place
     * i.e. in_buff == out_buff
     */
    /* we need 2 bytes in the output for every 3 bytes in the input */
    unsigned char* dst_ptr = (unsigned char*)out_buff;
    const unsigned char* src_ptr = in_buff;
    size_t src_smpl_index;
    for (src_smpl_index = 0; src_smpl_index < num_in_samples; src_smpl_index++) {
        src_ptr++;               /* lowest-(skip)-byte */
        *dst_ptr++ = *src_ptr++; /* low-byte */
        *dst_ptr++ = *src_ptr++; /* high-byte */
    }

    /* return number of *bytes* generated: */
    return num_in_samples * 2;
}

/*
 * Convert a buffer of N-channel, interleaved PCM16 samples to M-channel PCM16 channels
 * (where N < M).
 *   in_buff points to the buffer of PCM16 samples
 *   in_buff_channels Specifies the number of channels in the input buffer.
 *   out_buff points to the buffer to receive converted PCM16 samples.
 *   out_buff_channels Specifies the number of channels in the output buffer.
 *   num_in_samples size of input buffer in SAMPLES
 * returns
 *   the number of BYTES of output data.
 * NOTE
 *   channels > N are filled with silence.
 *   This conversion is safe to do in-place (in_buff == out_buff)
 * We are doing this since we *always* present to The Framework as STEREO device, but need to
 * support 4-channel devices.
 * TODO(pmclean, hung) Move this to a utilities module.
 */
static size_t expand_channels_16(const short* in_buff, int in_buff_chans,
                                 short* out_buff, int out_buff_chans,
                                 size_t num_in_samples) {
    /*
     * Move from back to front so that the conversion can be done in-place
     * i.e. in_buff == out_buff
     * NOTE: num_in_samples * out_buff_channels must be an even multiple of in_buff_chans
     */
    int num_out_samples = (num_in_samples * out_buff_chans)/in_buff_chans;

    short* dst_ptr = out_buff + num_out_samples - 1;
    size_t src_index;
    const short* src_ptr = in_buff + num_in_samples - 1;
    int num_zero_chans = out_buff_chans - in_buff_chans;
    for (src_index = 0; src_index < num_in_samples; src_index += in_buff_chans) {
        int dst_offset;
        for(dst_offset = 0; dst_offset < num_zero_chans; dst_offset++) {
            *dst_ptr-- = 0;
        }
        for(; dst_offset < out_buff_chans; dst_offset++) {
            *dst_ptr-- = *src_ptr--;
        }
    }

    /* return number of *bytes* generated */
    return num_out_samples * sizeof(short);
}

/*
 * Convert a buffer of N-channel, interleaved PCM16 samples to M-channel PCM16 channels
 * (where N > M).
 *   in_buff points to the buffer of PCM16 samples
 *   in_buff_channels Specifies the number of channels in the input buffer.
 *   out_buff points to the buffer to receive converted PCM16 samples.
 *   out_buff_channels Specifies the number of channels in the output buffer.
 *   num_in_samples size of input buffer in SAMPLES
 * returns
 *   the number of BYTES of output data.
 * NOTE
 *   channels > N are thrown away.
 *   This conversion is safe to do in-place (in_buff == out_buff)
 * We are doing this since we *always* present to The Framework as STEREO device, but need to
 * support 4-channel devices.
 * TODO(pmclean, hung) Move this to a utilities module.
 */
static size_t contract_channels_16(short* in_buff, int in_buff_chans,
                                   short* out_buff, int out_buff_chans,
                                   size_t num_in_samples) {
    /*
     * Move from front to back so that the conversion can be done in-place
     * i.e. in_buff == out_buff
     * NOTE: num_in_samples * out_buff_channels must be an even multiple of in_buff_chans
     */
    int num_out_samples = (num_in_samples * out_buff_chans)/in_buff_chans;

    int num_skip_samples = in_buff_chans - out_buff_chans;

    short* dst_ptr = out_buff;
    short* src_ptr = in_buff;
    size_t src_index;
    for (src_index = 0; src_index < num_in_samples; src_index += in_buff_chans) {
        int dst_offset;
        for(dst_offset = 0; dst_offset < out_buff_chans; dst_offset++) {
            *dst_ptr++ = *src_ptr++;
        }
        src_ptr += num_skip_samples;
    }

    /* return number of *bytes* generated */
    return num_out_samples * sizeof(short);
}

/*
 * ALSA Utilities
 */
/*
 * gets the ALSA bit-format flag from a bits-per-sample value.
 * TODO(pmclean, hung) Move this to a utilities module.
 */
static int bits_to_alsa_format(unsigned int bits_per_sample, int default_format)
{
    enum pcm_format format;
    for (format = PCM_FORMAT_S16_LE; format < PCM_FORMAT_MAX; format++) {
        if (pcm_format_to_bits(format) == bits_per_sample) {
            return format;
        }
    }
    return default_format;
}

static void log_pcm_params(struct pcm_params * alsa_hw_params) {
    ALOGV("usb:audio_hw - PCM_PARAM_SAMPLE_BITS min:%u, max:%u",
          pcm_params_get_min(alsa_hw_params, PCM_PARAM_SAMPLE_BITS),
          pcm_params_get_max(alsa_hw_params, PCM_PARAM_SAMPLE_BITS));
    ALOGV("usb:audio_hw - PCM_PARAM_FRAME_BITS min:%u, max:%u",
          pcm_params_get_min(alsa_hw_params, PCM_PARAM_FRAME_BITS),
          pcm_params_get_max(alsa_hw_params, PCM_PARAM_FRAME_BITS));
    ALOGV("usb:audio_hw - PCM_PARAM_CHANNELS min:%u, max:%u",
          pcm_params_get_min(alsa_hw_params, PCM_PARAM_CHANNELS),
          pcm_params_get_max(alsa_hw_params, PCM_PARAM_CHANNELS));
    ALOGV("usb:audio_hw - PCM_PARAM_RATE min:%u, max:%u",
          pcm_params_get_min(alsa_hw_params, PCM_PARAM_RATE),
          pcm_params_get_max(alsa_hw_params, PCM_PARAM_RATE));
    ALOGV("usb:audio_hw - PCM_PARAM_PERIOD_TIME min:%u, max:%u",
          pcm_params_get_min(alsa_hw_params, PCM_PARAM_PERIOD_TIME),
          pcm_params_get_max(alsa_hw_params, PCM_PARAM_PERIOD_TIME));
    ALOGV("usb:audio_hw - PCM_PARAM_PERIOD_SIZE min:%u, max:%u",
          pcm_params_get_min(alsa_hw_params, PCM_PARAM_PERIOD_SIZE),
          pcm_params_get_max(alsa_hw_params, PCM_PARAM_PERIOD_SIZE));
    ALOGV("usb:audio_hw - PCM_PARAM_PERIOD_BYTES min:%u, max:%u",
          pcm_params_get_min(alsa_hw_params, PCM_PARAM_PERIOD_BYTES),
          pcm_params_get_max(alsa_hw_params, PCM_PARAM_PERIOD_BYTES));
    ALOGV("usb:audio_hw - PCM_PARAM_PERIODS min:%u, max:%u",
          pcm_params_get_min(alsa_hw_params, PCM_PARAM_PERIODS),
          pcm_params_get_max(alsa_hw_params, PCM_PARAM_PERIODS));
    ALOGV("usb:audio_hw - PCM_PARAM_BUFFER_TIME min:%u, max:%u",
          pcm_params_get_min(alsa_hw_params, PCM_PARAM_BUFFER_TIME),
          pcm_params_get_max(alsa_hw_params, PCM_PARAM_BUFFER_TIME));
    ALOGV("usb:audio_hw - PCM_PARAM_BUFFER_SIZE min:%u, max:%u",
          pcm_params_get_min(alsa_hw_params, PCM_PARAM_BUFFER_SIZE),
          pcm_params_get_max(alsa_hw_params, PCM_PARAM_BUFFER_SIZE));
    ALOGV("usb:audio_hw - PCM_PARAM_BUFFER_BYTES min:%u, max:%u",
          pcm_params_get_min(alsa_hw_params, PCM_PARAM_BUFFER_BYTES),
          pcm_params_get_max(alsa_hw_params, PCM_PARAM_BUFFER_BYTES));
    ALOGV("usb:audio_hw - PCM_PARAM_TICK_TIME min:%u, max:%u",
          pcm_params_get_min(alsa_hw_params, PCM_PARAM_TICK_TIME),
          pcm_params_get_max(alsa_hw_params, PCM_PARAM_TICK_TIME));
}

/*
 * Reads and decodes configuration info from the specified ALSA card/device
 */
static int read_alsa_device_config(int card, int device, int io_type, struct pcm_config * config)
{
    ALOGV("usb:audio_hw - read_alsa_device_config(c:%d d:%d t:0x%X)",card, device, io_type);

    if (card < 0 || device < 0) {
        return -EINVAL;
    }

    struct pcm_params * alsa_hw_params = pcm_params_get(card, device, io_type);
    if (alsa_hw_params == NULL) {
        return -EINVAL;
    }

    /*
     * This Logging will be useful when testing new USB devices.
     */
    /* log_pcm_params(alsa_hw_params); */

    config->channels = pcm_params_get_min(alsa_hw_params, PCM_PARAM_CHANNELS);
    config->rate = pcm_params_get_min(alsa_hw_params, PCM_PARAM_RATE);
    config->period_size = pcm_params_get_max(alsa_hw_params, PCM_PARAM_PERIODS);
    config->period_count = pcm_params_get_min(alsa_hw_params, PCM_PARAM_PERIODS);

    unsigned int bits_per_sample = pcm_params_get_min(alsa_hw_params, PCM_PARAM_SAMPLE_BITS);
    config->format = bits_to_alsa_format(bits_per_sample, PCM_FORMAT_S16_LE);

    return 0;
}

/*
 * HAl Functions
 */
/**
 * NOTE: when multiple mutexes have to be acquired, always respect the
 * following order: hw device > out stream
 */

/* Helper functions */
static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    return cached_output_hardware_config.rate;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    return 0;
}

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    return cached_output_hardware_config.period_size * audio_stream_frame_size(stream);
}

static uint32_t out_get_channels(const struct audio_stream *stream)
{
    // Always Stero for now. We will do *some* conversions in this HAL.
    // TODO(pmclean) When AudioPolicyManager & AudioFlinger supports arbitrary channels
    // rewrite this to return the ACTUAL channel format
    return AUDIO_CHANNEL_OUT_STEREO;
}

static audio_format_t out_get_format(const struct audio_stream *stream)
{
    // Always return 16-bit PCM. We will do *some* conversions in this HAL.
    // TODO(pmclean) When AudioPolicyManager & AudioFlinger supports arbitrary PCM formats
    // rewrite this to return the ACTUAL data format
    return AUDIO_FORMAT_PCM_16_BIT;
}

static int out_set_format(struct audio_stream *stream, audio_format_t format)
{
    return 0;
}

static int out_standby(struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    pthread_mutex_lock(&out->dev->lock);
    pthread_mutex_lock(&out->lock);

    if (!out->standby) {
        pcm_close(out->pcm);
        out->pcm = NULL;
        out->standby = true;
    }

    pthread_mutex_unlock(&out->lock);
    pthread_mutex_unlock(&out->dev->lock);

    return 0;
}

static int out_dump(const struct audio_stream *stream, int fd)
{
    return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    ALOGV("usb:audio_hw::out out_set_parameters() keys:%s", kvpairs);

    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;
    struct str_parms *parms;
    char value[32];
    int param_val;
    int routing = 0;
    int ret_value = 0;

    parms = str_parms_create_str(kvpairs);
    pthread_mutex_lock(&adev->lock);

    bool recache_device_params = false;
    param_val = str_parms_get_str(parms, "card", value, sizeof(value));
    if (param_val >= 0) {
        adev->out_card = atoi(value);
        recache_device_params = true;
    }

    param_val = str_parms_get_str(parms, "device", value, sizeof(value));
    if (param_val >= 0) {
        adev->out_device = atoi(value);
        recache_device_params = true;
    }

    if (recache_device_params && adev->out_card >= 0 && adev->out_device >= 0) {
        ret_value = read_alsa_device_config(adev->out_card, adev->out_device, PCM_OUT,
                                            &cached_output_hardware_config);
        output_hardware_config_is_cached = (ret_value == 0);
    }

    pthread_mutex_unlock(&adev->lock);
    str_parms_destroy(parms);

    return ret_value;
}

//TODO(pmclean) it seems like both out_get_parameters() and in_get_parameters()
// could be written in terms of a get_device_parameters(io_type)

static char * out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    ALOGV("usb:audio_hw::out out_get_parameters() keys:%s", keys);

    struct stream_out *out = (struct stream_out *) stream;
    struct audio_device *adev = out->dev;

    if (adev->out_card < 0 || adev->out_device < 0)
        return strdup("");

    unsigned min, max;

    struct str_parms *query = str_parms_create_str(keys);
    struct str_parms *result = str_parms_create();

    int num_written = 0;
    char buffer[256];
    int buffer_size = sizeof(buffer) / sizeof(buffer[0]);
    char* result_str = NULL;

    struct pcm_params * alsa_hw_params = pcm_params_get(adev->out_card, adev->out_device, PCM_OUT);

    // These keys are from hardware/libhardware/include/audio.h
    // supported sample rates
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
        // pcm_hw_params doesn't have a list of supported samples rates, just a min and a max, so
        // if they are different, return a list containing those two values, otherwise just the one.
        min = pcm_params_get_min(alsa_hw_params, PCM_PARAM_RATE);
        max = pcm_params_get_max(alsa_hw_params, PCM_PARAM_RATE);
        num_written = snprintf(buffer, buffer_size, "%u", min);
        if (min != max) {
            snprintf(buffer + num_written, buffer_size - num_written, "|%u", max);
        }
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES,
                          buffer);
    }  // AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES

    // supported channel counts
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)) {
        // Similarly for output channels count
        min = pcm_params_get_min(alsa_hw_params, PCM_PARAM_CHANNELS);
        max = pcm_params_get_max(alsa_hw_params, PCM_PARAM_CHANNELS);
        num_written = snprintf(buffer, buffer_size, "%u", min);
        if (min != max) {
            snprintf(buffer + num_written, buffer_size - num_written, "|%u", max);
        }
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_CHANNELS, buffer);
    }  // AUDIO_PARAMETER_STREAM_SUP_CHANNELS

    // supported sample formats
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
        // Similarly for output channels count
        //TODO(pmclean): this is wrong.
        min = pcm_params_get_min(alsa_hw_params, PCM_PARAM_SAMPLE_BITS);
        max = pcm_params_get_max(alsa_hw_params, PCM_PARAM_SAMPLE_BITS);
        num_written = snprintf(buffer, buffer_size, "%u", min);
        if (min != max) {
            snprintf(buffer + num_written, buffer_size - num_written, "|%u", max);
        }
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_FORMATS, buffer);
    }  // AUDIO_PARAMETER_STREAM_SUP_FORMATS

    result_str = str_parms_to_str(result);

    // done with these...
    str_parms_destroy(query);
    str_parms_destroy(result);

    return result_str;
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    struct stream_out *out = (struct stream_out *) stream;

    //TODO(pmclean): Do we need a term here for the USB latency
    // (as reported in the USB descriptors)?
    uint32_t latency = (cached_output_hardware_config.period_size
            * cached_output_hardware_config.period_count * 1000) / out_get_sample_rate(&stream->common);
    return latency;
}

static int out_set_volume(struct audio_stream_out *stream, float left, float right)
{
    return -ENOSYS;
}

/* must be called with hw device and output stream mutexes locked */
static int start_output_stream(struct stream_out *out)
{
    struct audio_device *adev = out->dev;
    int return_val = 0;

    ALOGV("usb:audio_hw::out start_output_stream(card:%d device:%d)",
          adev->out_card, adev->out_device);

    out->pcm = pcm_open(adev->out_card, adev->out_device, PCM_OUT, &cached_output_hardware_config);
    if (out->pcm == NULL) {
        return -ENOMEM;
    }

    if (out->pcm && !pcm_is_ready(out->pcm)) {
        ALOGE("audio_hw audio_hw pcm_open() failed: %s", pcm_get_error(out->pcm));
        pcm_close(out->pcm);
        return -ENOMEM;
    }

    return 0;
}

static ssize_t out_write(struct audio_stream_out *stream, const void* buffer, size_t bytes)
{
    int ret;
    struct stream_out *out = (struct stream_out *)stream;

    pthread_mutex_lock(&out->dev->lock);
    pthread_mutex_lock(&out->lock);
    if (out->standby) {
        ret = start_output_stream(out);
        if (ret != 0) {
            goto err;
        }
        out->standby = false;
    }

    // Setup conversion buffer
    // compute maximum potential buffer size.
    // * 2 for stereo -> quad conversion
    // * 3/2 for 16bit -> 24 bit conversion
    size_t required_conversion_buffer_size = (bytes * 3 * 2) / 2;
    if (required_conversion_buffer_size > out->conversion_buffer_size) {
        //TODO(pmclean) - remove this when AudioPolicyManger/AudioFlinger support arbitrary formats
        // (and do these conversions themselves)
        out->conversion_buffer_size = required_conversion_buffer_size;
        out->conversion_buffer = realloc(out->conversion_buffer, out->conversion_buffer_size);
    }

    const void * write_buff = buffer;
    int num_write_buff_bytes = bytes;

    /*
     * Num Channels conversion
     */
    int num_device_channels = cached_output_hardware_config.channels;
    int num_req_channels = 2; /* always, for now */
    if (num_device_channels != num_req_channels) {
        num_write_buff_bytes =
                expand_channels_16(write_buff, num_req_channels,
                                   out->conversion_buffer, num_device_channels,
                                   num_write_buff_bytes / sizeof(short));
        write_buff = out->conversion_buffer;
    }

    /*
     *  16 vs 24-bit logic here
     */
    switch (cached_output_hardware_config.format) {
    case PCM_FORMAT_S16_LE:
        // the output format is the same as the input format, so just write it out
        break;

    case PCM_FORMAT_S24_3LE:
        // 16-bit LE2 - 24-bit LE3
        num_write_buff_bytes = convert_16_to_24_3(write_buff,
                                                  num_write_buff_bytes / sizeof(short),
                                                  out->conversion_buffer);
        write_buff = out->conversion_buffer;
        break;

    default:
        // hmmmmm.....
        ALOGV("usb:Unknown Format!!!");
        break;
    }

    if (write_buff != NULL && num_write_buff_bytes != 0) {
        pcm_write(out->pcm, write_buff, num_write_buff_bytes);
    }

    pthread_mutex_unlock(&out->lock);
    pthread_mutex_unlock(&out->dev->lock);

    return bytes;

err:
    pthread_mutex_unlock(&out->lock);
    pthread_mutex_unlock(&out->dev->lock);
    if (ret != 0) {
        usleep(bytes * 1000000 / audio_stream_frame_size(&stream->common) /
               out_get_sample_rate(&stream->common));
    }

    return bytes;
}

static int out_get_render_position(const struct audio_stream_out *stream, uint32_t *dsp_frames)
{
    return -EINVAL;
}

static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static int out_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static int out_get_next_write_timestamp(const struct audio_stream_out *stream, int64_t *timestamp)
{
    return -EINVAL;
}

static int adev_open_output_stream(struct audio_hw_device *dev,
                                   audio_io_handle_t handle,
                                   audio_devices_t devices,
                                   audio_output_flags_t flags,
                                   struct audio_config *config,
                                   struct audio_stream_out **stream_out)
{
    ALOGV("usb:audio_hw::out adev_open_output_stream() handle:0x%X, device:0x%X, flags:0x%X",
          handle, devices, flags);

    struct audio_device *adev = (struct audio_device *)dev;

    struct stream_out *out;

    out = (struct stream_out *)calloc(1, sizeof(struct stream_out));
    if (!out)
        return -ENOMEM;

    // setup function pointers
    out->stream.common.get_sample_rate = out_get_sample_rate;
    out->stream.common.set_sample_rate = out_set_sample_rate;
    out->stream.common.get_buffer_size = out_get_buffer_size;
    out->stream.common.get_channels = out_get_channels;
    out->stream.common.get_format = out_get_format;
    out->stream.common.set_format = out_set_format;
    out->stream.common.standby = out_standby;
    out->stream.common.dump = out_dump;
    out->stream.common.set_parameters = out_set_parameters;
    out->stream.common.get_parameters = out_get_parameters;
    out->stream.common.add_audio_effect = out_add_audio_effect;
    out->stream.common.remove_audio_effect = out_remove_audio_effect;
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.write = out_write;
    out->stream.get_render_position = out_get_render_position;
    out->stream.get_next_write_timestamp = out_get_next_write_timestamp;

    out->dev = adev;

    if (output_hardware_config_is_cached) {
        config->sample_rate = cached_output_hardware_config.rate;

        config->format = alsa_to_fw_format_id(cached_output_hardware_config.format);
        if (config->format != AUDIO_FORMAT_PCM_16_BIT) {
            // Always report PCM16 for now. AudioPolicyManagerBase/AudioFlinger dont' understand
            // formats with more other format, so we won't get chosen (say with a 24bit DAC).
            //TODO(pmclean) remove this when the above restriction is removed.
            config->format = AUDIO_FORMAT_PCM_16_BIT;
        }

        config->channel_mask =
                audio_channel_out_mask_from_count(cached_output_hardware_config.channels);
        if (config->channel_mask != AUDIO_CHANNEL_OUT_STEREO) {
            // Always report STEREO for now.  AudioPolicyManagerBase/AudioFlinger dont' understand
            // formats with more channels, so we won't get chosen (say with a 4-channel DAC).
            //TODO(pmclean) remove this when the above restriction is removed.
            config->channel_mask = AUDIO_CHANNEL_OUT_STEREO;
        }
    } else {
        cached_output_hardware_config = default_alsa_out_config;

        config->format = out_get_format(&out->stream.common);
        config->channel_mask = out_get_channels(&out->stream.common);
        config->sample_rate = out_get_sample_rate(&out->stream.common);
    }

    out->conversion_buffer = NULL;
    out->conversion_buffer_size = 0;

    out->standby = true;

    *stream_out = &out->stream;
    return 0;

err_open:
    free(out);
    *stream_out = NULL;
    return -ENOSYS;
}

static void adev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream)
{
    ALOGV("usb:audio_hw::out adev_close_output_stream()");
    struct stream_out *out = (struct stream_out *)stream;

    //TODO(pmclean) why are we doing this when stream get's freed at the end
    // because it closes the pcm device
    out_standby(&stream->common);

    free(out->conversion_buffer);
    out->conversion_buffer = NULL;
    out->conversion_buffer_size = 0;

    free(stream);
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
    return 0;
}

static char * adev_get_parameters(const struct audio_hw_device *dev, const char *keys)
{
    return strdup("");
}

static int adev_init_check(const struct audio_hw_device *dev)
{
    return 0;
}

static int adev_set_voice_volume(struct audio_hw_device *dev, float volume)
{
    return -ENOSYS;
}

static int adev_set_master_volume(struct audio_hw_device *dev, float volume)
{
    return -ENOSYS;
}

static int adev_set_mode(struct audio_hw_device *dev, audio_mode_t mode)
{
    return 0;
}

static int adev_set_mic_mute(struct audio_hw_device *dev, bool state)
{
    return -ENOSYS;
}

static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state)
{
    return -ENOSYS;
}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
                                         const struct audio_config *config)
{
    return 0;
}

/* Helper functions */
static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
    return cached_input_hardware_config.rate;
}

static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    return -ENOSYS;
}

static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    ALOGV("usb: in_get_buffer_size() = %zu",
          cached_input_hardware_config.period_size * audio_stream_frame_size(stream));
    return cached_input_hardware_config.period_size * audio_stream_frame_size(stream);

}

static uint32_t in_get_channels(const struct audio_stream *stream)
{
    // just report stereo for now
    return AUDIO_CHANNEL_IN_STEREO;
}

static audio_format_t in_get_format(const struct audio_stream *stream)
{
    // just report 16-bit, pcm for now.
    return AUDIO_FORMAT_PCM_16_BIT;
}

static int in_set_format(struct audio_stream *stream, audio_format_t format)
{
    return -ENOSYS;
}

static int in_standby(struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *) stream;

    pthread_mutex_lock(&in->dev->lock);
    pthread_mutex_lock(&in->lock);

    if (!in->standby) {
        pcm_close(in->pcm);
        in->pcm = NULL;
        in->standby = true;
    }

    pthread_mutex_unlock(&in->lock);
    pthread_mutex_unlock(&in->dev->lock);

    return 0;
}

static int in_dump(const struct audio_stream *stream, int fd)
{
    return 0;
}

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    ALOGV("usb: audio_hw::in in_set_parameters() keys:%s", kvpairs);

    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in->dev;
    struct str_parms *parms;
    char value[32];
    int param_val;
    int routing = 0;
    int ret_value = 0;

    parms = str_parms_create_str(kvpairs);
    pthread_mutex_lock(&adev->lock);

    bool recache_device_params = false;

    // Card/Device
    param_val = str_parms_get_str(parms, "card", value, sizeof(value));
    if (param_val >= 0) {
        adev->in_card = atoi(value);
        recache_device_params = true;
    }

    param_val = str_parms_get_str(parms, "device", value, sizeof(value));
    if (param_val >= 0) {
        adev->in_device = atoi(value);
        recache_device_params = true;
    }

    if (recache_device_params && adev->in_card >= 0 && adev->in_device >= 0) {
        ret_value = read_alsa_device_config(adev->in_card, adev->in_device,
                                            PCM_IN, &(cached_input_hardware_config));
        input_hardware_config_is_cached = (ret_value == 0);
    }

    pthread_mutex_unlock(&adev->lock);
    str_parms_destroy(parms);

    return ret_value;
}

//TODO(pmclean) it seems like both out_get_parameters() and in_get_parameters()
// could be written in terms of a get_device_parameters(io_type)

static char * in_get_parameters(const struct audio_stream *stream, const char *keys) {
    ALOGV("usb:audio_hw::in in_get_parameters() keys:%s", keys);

    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in->dev;

    if (adev->in_card < 0 || adev->in_device < 0)
        return strdup("");

    struct pcm_params * alsa_hw_params = pcm_params_get(adev->in_card, adev->in_device, PCM_IN);
    if (alsa_hw_params == NULL)
        return strdup("");

    struct str_parms *query = str_parms_create_str(keys);
    struct str_parms *result = str_parms_create();

    int num_written = 0;
    char buffer[256];
    int buffer_size = sizeof(buffer) / sizeof(buffer[0]);
    char* result_str = NULL;

    unsigned min, max;

    // These keys are from hardware/libhardware/include/audio.h
    // supported sample rates
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
        // pcm_hw_params doesn't have a list of supported samples rates, just a min and a max, so
        // if they are different, return a list containing those two values, otherwise just the one.
        min = pcm_params_get_min(alsa_hw_params, PCM_PARAM_RATE);
        max = pcm_params_get_max(alsa_hw_params, PCM_PARAM_RATE);
        num_written = snprintf(buffer, buffer_size, "%u", min);
        if (min != max) {
            snprintf(buffer + num_written, buffer_size - num_written, "|%u", max);
        }
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SAMPLING_RATE, buffer);
    }  // AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES

    // supported channel counts
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)) {
        // Similarly for output channels count
        min = pcm_params_get_min(alsa_hw_params, PCM_PARAM_CHANNELS);
        max = pcm_params_get_max(alsa_hw_params, PCM_PARAM_CHANNELS);
        num_written = snprintf(buffer, buffer_size, "%u", min);
        if (min != max) {
            snprintf(buffer + num_written, buffer_size - num_written, "|%u", max);
        }
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_CHANNELS, buffer);
    }  // AUDIO_PARAMETER_STREAM_SUP_CHANNELS

    // supported sample formats
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
        //TODO(pmclean): this is wrong.
        min = pcm_params_get_min(alsa_hw_params, PCM_PARAM_SAMPLE_BITS);
        max = pcm_params_get_max(alsa_hw_params, PCM_PARAM_SAMPLE_BITS);
        num_written = snprintf(buffer, buffer_size, "%u", min);
        if (min != max) {
            snprintf(buffer + num_written, buffer_size - num_written, "|%u", max);
        }
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_FORMATS, buffer);
    }  // AUDIO_PARAMETER_STREAM_SUP_FORMATS

    result_str = str_parms_to_str(result);

    // done with these...
    str_parms_destroy(query);
    str_parms_destroy(result);

    return result_str;
}

static int in_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static int in_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static int in_set_gain(struct audio_stream_in *stream, float gain)
{
    return 0;
}

/* must be called with hw device and output stream mutexes locked */
static int start_input_stream(struct stream_in *in) {
    struct audio_device *adev = in->dev;
    int return_val = 0;

    ALOGV("usb:audio_hw::start_input_stream(card:%d device:%d)",
          adev->in_card, adev->in_device);

    in->pcm = pcm_open(adev->in_card, adev->in_device, PCM_IN, &cached_input_hardware_config);
    if (in->pcm == NULL) {
        ALOGE("usb:audio_hw pcm_open() in->pcm == NULL");
        return -ENOMEM;
    }

    if (in->pcm && !pcm_is_ready(in->pcm)) {
        ALOGE("usb:audio_hw audio_hw pcm_open() failed: %s", pcm_get_error(in->pcm));
        pcm_close(in->pcm);
        return -ENOMEM;
    }

    return 0;
}

//TODO(pmclean) mutex stuff here (see out_write)
static ssize_t in_read(struct audio_stream_in *stream, void* buffer, size_t bytes)
{
    size_t num_read_buff_bytes = 0;
    void * read_buff = buffer;
    void * out_buff = buffer;

    struct stream_in * in = (struct stream_in *) stream;

    ALOGV("usb: in_read(%d)", bytes);

    pthread_mutex_lock(&in->dev->lock);
    pthread_mutex_lock(&in->lock);

    if (in->standby) {
        if (start_input_stream(in) != 0) {
            goto err;
        }
        in->standby = false;
    }

    // OK, we need to figure out how much data to read to be able to output the requested
    // number of bytes in the HAL format (16-bit, stereo).
    num_read_buff_bytes = bytes;
    int num_device_channels = cached_input_hardware_config.channels;
    int num_req_channels = 2; /* always, for now */

    if (num_device_channels != num_req_channels) {
        num_read_buff_bytes = (num_device_channels * num_read_buff_bytes) / num_req_channels;
    }

    if (cached_output_hardware_config.format == PCM_FORMAT_S24_3LE) {
        num_read_buff_bytes = (3 * num_read_buff_bytes) / 2;
    }

    // Setup/Realloc the conversion buffer (if necessary).
    if (num_read_buff_bytes != bytes) {
        if (num_read_buff_bytes > in->conversion_buffer_size) {
            //TODO(pmclean) - remove this when AudioPolicyManger/AudioFlinger support arbitrary formats
            // (and do these conversions themselves)
            in->conversion_buffer_size = num_read_buff_bytes;
            in->conversion_buffer = realloc(in->conversion_buffer, in->conversion_buffer_size);
        }
        read_buff = in->conversion_buffer;
    }

    if (pcm_read(in->pcm, read_buff, num_read_buff_bytes) == 0) {
        /*
         * Do any conversions necessary to send the data in the format specified to/by the HAL
         * (but different from the ALSA format), such as 24bit ->16bit, or 4chan -> 2chan.
         */
        if (cached_output_hardware_config.format == PCM_FORMAT_S24_3LE) {
            if (num_device_channels != num_req_channels) {
                out_buff = read_buff;
            }

            /* Bit Format Conversion */
            num_read_buff_bytes =
                convert_24_3_to_16(read_buff, num_read_buff_bytes / 3, out_buff);
        }

        if (num_device_channels != num_req_channels) {
            out_buff = buffer;
            /* Num Channels conversion */
            if (num_device_channels < num_req_channels) {
                num_read_buff_bytes =
                    contract_channels_16(read_buff, num_device_channels,
                                         out_buff, num_req_channels,
                                         num_read_buff_bytes / sizeof(short));
            } else {
                num_read_buff_bytes =
                    expand_channels_16(read_buff, num_device_channels,
                                       out_buff, num_req_channels,
                                       num_read_buff_bytes / sizeof(short));
            }
        }
    }

err:
    pthread_mutex_unlock(&in->lock);
    pthread_mutex_unlock(&in->dev->lock);

    return num_read_buff_bytes;
}

static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream)
{
    return 0;
}

static int adev_open_input_stream(struct audio_hw_device *dev,
                                  audio_io_handle_t handle,
                                  audio_devices_t devices,
                                  struct audio_config *config,
                                  struct audio_stream_in **stream_in)
{
    ALOGV("usb: in adev_open_input_stream() rate:%" PRIu32 ", chanMask:0x%" PRIX32 ", fmt:%" PRIu8,
          config->sample_rate, config->channel_mask, config->format);

    struct stream_in *in = (struct stream_in *)calloc(1, sizeof(struct stream_in));
    if (in == NULL)
        return -ENOMEM;

    // setup function pointers
    in->stream.common.get_sample_rate = in_get_sample_rate;
    in->stream.common.set_sample_rate = in_set_sample_rate;
    in->stream.common.get_buffer_size = in_get_buffer_size;
    in->stream.common.get_channels = in_get_channels;
    in->stream.common.get_format = in_get_format;
    in->stream.common.set_format = in_set_format;
    in->stream.common.standby = in_standby;
    in->stream.common.dump = in_dump;
    in->stream.common.set_parameters = in_set_parameters;
    in->stream.common.get_parameters = in_get_parameters;
    in->stream.common.add_audio_effect = in_add_audio_effect;
    in->stream.common.remove_audio_effect = in_remove_audio_effect;

    in->stream.set_gain = in_set_gain;
    in->stream.read = in_read;
    in->stream.get_input_frames_lost = in_get_input_frames_lost;

    in->dev = (struct audio_device *)dev;

    if (output_hardware_config_is_cached) {
        config->sample_rate = cached_output_hardware_config.rate;

        config->format = alsa_to_fw_format_id(cached_output_hardware_config.format);
        if (config->format != AUDIO_FORMAT_PCM_16_BIT) {
            // Always report PCM16 for now. AudioPolicyManagerBase/AudioFlinger dont' understand
            // formats with more other format, so we won't get chosen (say with a 24bit DAC).
            //TODO(pmclean) remove this when the above restriction is removed.
            config->format = AUDIO_FORMAT_PCM_16_BIT;
        }

        config->channel_mask = audio_channel_out_mask_from_count(
                cached_output_hardware_config.channels);
        if (config->channel_mask != AUDIO_CHANNEL_OUT_STEREO) {
            // Always report STEREO for now.  AudioPolicyManagerBase/AudioFlinger dont' understand
            // formats with more channels, so we won't get chosen (say with a 4-channel DAC).
            //TODO(pmclean) remove this when the above restriction is removed.
            config->channel_mask = AUDIO_CHANNEL_OUT_STEREO;
        }
    } else {
        cached_input_hardware_config = default_alsa_in_config;

        config->format = out_get_format(&in->stream.common);
        config->channel_mask = out_get_channels(&in->stream.common);
        config->sample_rate = out_get_sample_rate(&in->stream.common);
    }

    in->standby = true;

    in->conversion_buffer = NULL;
    in->conversion_buffer_size = 0;

    *stream_in = &in->stream;

    return 0;
}

static void adev_close_input_stream(struct audio_hw_device *dev, struct audio_stream_in *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

    //TODO(pmclean) why are we doing this when stream get's freed at the end
    // because it closes the pcm device
    in_standby(&stream->common);

    free(in->conversion_buffer);

    free(stream);
}

static int adev_dump(const audio_hw_device_t *device, int fd)
{
    return 0;
}

static int adev_close(hw_device_t *device)
{
    struct audio_device *adev = (struct audio_device *)device;
    free(device);

    output_hardware_config_is_cached = false;
    input_hardware_config_is_cached = false;

    return 0;
}

static int adev_open(const hw_module_t* module, const char* name, hw_device_t** device)
{
    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0)
        return -EINVAL;

    struct audio_device *adev = calloc(1, sizeof(struct audio_device));
    if (!adev)
        return -ENOMEM;

    adev->hw_device.common.tag = HARDWARE_DEVICE_TAG;
    adev->hw_device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
    adev->hw_device.common.module = (struct hw_module_t *) module;
    adev->hw_device.common.close = adev_close;

    adev->hw_device.init_check = adev_init_check;
    adev->hw_device.set_voice_volume = adev_set_voice_volume;
    adev->hw_device.set_master_volume = adev_set_master_volume;
    adev->hw_device.set_mode = adev_set_mode;
    adev->hw_device.set_mic_mute = adev_set_mic_mute;
    adev->hw_device.get_mic_mute = adev_get_mic_mute;
    adev->hw_device.set_parameters = adev_set_parameters;
    adev->hw_device.get_parameters = adev_get_parameters;
    adev->hw_device.get_input_buffer_size = adev_get_input_buffer_size;
    adev->hw_device.open_output_stream = adev_open_output_stream;
    adev->hw_device.close_output_stream = adev_close_output_stream;
    adev->hw_device.open_input_stream = adev_open_input_stream;
    adev->hw_device.close_input_stream = adev_close_input_stream;
    adev->hw_device.dump = adev_dump;

    *device = &adev->hw_device.common;

    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "USB audio HW HAL",
        .author = "The Android Open Source Project",
        .methods = &hal_module_methods,
    },
};
