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
/*#define LOG_PCM_PARAMS 0*/

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>

#include <log/log.h>
#include <cutils/str_parms.h>
#include <cutils/properties.h>

#include <hardware/audio.h>
#include <hardware/audio_alsaops.h>
#include <hardware/hardware.h>

#include <system/audio.h>

#include <tinyalsa/asoundlib.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

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

struct audio_device_profile {
    int card;
    int device;
    int direction; /* PCM_OUT or PCM_IN */
};

struct audio_device {
    struct audio_hw_device hw_device;

    pthread_mutex_t lock; /* see note below on mutex acquisition order */

    /* output */
    struct audio_device_profile out_profile;

    /* input */
    struct audio_device_profile in_profile;

    bool standby;
};

struct stream_out {
    struct audio_stream_out stream;

    pthread_mutex_t lock;               /* see note below on mutex acquisition order */
    struct pcm *pcm;                    /* state of the stream */
    bool standby;

    struct audio_device *dev;           /* hardware information */
    struct audio_device_profile * profile;

    void * conversion_buffer;           /* any conversions are put into here
                                         * they could come from here too if
                                         * there was a previous conversion */
    size_t conversion_buffer_size;      /* in bytes */
};

/*
 * Output Configuration Cache
 * FIXME(pmclean) This is not reentrant. Should probably be moved into the stream structure.
 */
static struct pcm_config cached_output_hardware_config;
static bool output_hardware_config_is_cached = false;

struct stream_in {
    struct audio_stream_in stream;

    pthread_mutex_t lock; /* see note below on mutex acquisition order */
    struct pcm *pcm;
    bool standby;

    struct audio_device *dev;

    struct audio_device_profile * profile;

    struct audio_config hal_pcm_config;

    /* this is the format the framework thinks it's using. We may need to convert from the actual
     * (24-bit, 32-bit?) format to this theoretical (framework, probably 16-bit)
     * format in in_read() */
    enum pcm_format input_framework_format;

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
 * Data Conversions
 */
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
 *   This conversion is safe to do in-place (in_buff == out_buff).
 * TODO Move this to a utilities module.
 */
static size_t convert_24_3_to_16(const unsigned char * in_buff, size_t num_in_samples,
                                 short * out_buff)
{
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
 * Convert a buffer of packed (3-byte) PCM32 samples to PCM16LE samples.
 *   in_buff points to the buffer of PCM32 samples
 *   num_in_samples size of input buffer in SAMPLES
 *   out_buff points to the buffer to receive converted PCM16LE LE samples.
 * returns
 *   the number of BYTES of output data.
 * We are doing this since we *always* present to The Framework as A PCM16LE device, but need to
 * support PCM_FORMAT_S32_LE (32-bit).
 * NOTE:
 *   This conversion is safe to do in-place (in_buff == out_buff).
 * TODO Move this to a utilities module.
 */
static size_t convert_32_to_16(const int32_t * in_buff, size_t num_in_samples, short * out_buff)
{
    /*
     * Move from front to back so that the conversion can be done in-place
     * i.e. in_buff == out_buff
     */

    short * dst_ptr = out_buff;
    const int32_t* src_ptr = in_buff;
    size_t src_smpl_index;
    for (src_smpl_index = 0; src_smpl_index < num_in_samples; src_smpl_index++) {
        *dst_ptr++ = *src_ptr++ >> 16;
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
 *   num_in_bytes size of input buffer in BYTES
 * returns
 *   the number of BYTES of output data.
 * NOTE
 *   channels > N are filled with silence.
 *   This conversion is safe to do in-place (in_buff == out_buff)
 * We are doing this since we *always* present to The Framework as STEREO device, but need to
 * support 4-channel devices.
 * TODO Move this to a utilities module.
 */
static size_t expand_channels_16(const int16_t* in_buff, int in_buff_chans,
                                 int16_t* out_buff, int out_buff_chans,
                                 size_t num_in_bytes)
{
    /*
     * Move from back to front so that the conversion can be done in-place
     * i.e. in_buff == out_buff
     * NOTE: num_in_samples * out_buff_channels must be an even multiple of in_buff_chans
     */
    size_t num_in_samples = num_in_bytes / sizeof(int16_t);

    size_t num_out_samples = (num_in_samples * out_buff_chans) / in_buff_chans;

    short* dst_ptr = out_buff + num_out_samples - 1;
    size_t src_index;
    const short* src_ptr = in_buff + num_in_samples - 1;
    int num_zero_chans = out_buff_chans - in_buff_chans;
    for (src_index = 0; src_index < num_in_samples; src_index += in_buff_chans) {
        int dst_offset;
        for (dst_offset = 0; dst_offset < num_zero_chans; dst_offset++) {
            *dst_ptr-- = 0;
        }
        for (; dst_offset < out_buff_chans; dst_offset++) {
            *dst_ptr-- = *src_ptr--;
        }
    }

    /* return number of *bytes* generated */
    return num_out_samples * sizeof(int16_t);
}

/*
 * Convert a buffer of N-channel, interleaved PCM16 samples to M-channel PCM16 channels
 * (where N > M).
 *   in_buff points to the buffer of PCM16 samples
 *   in_buff_channels Specifies the number of channels in the input buffer.
 *   out_buff points to the buffer to receive converted PCM16 samples.
 *   out_buff_channels Specifies the number of channels in the output buffer.
 *   num_in_bytes size of input buffer in BYTES
 * returns
 *   the number of BYTES of output data.
 * NOTE
 *   channels > N are thrown away.
 *   This conversion is safe to do in-place (in_buff == out_buff)
 * We are doing this since we *always* present to The Framework as STEREO device, but need to
 * support 4-channel devices.
 * TODO Move this to a utilities module.
 */
static size_t contract_channels_16(const int16_t* in_buff, size_t in_buff_chans,
                                   int16_t* out_buff, size_t out_buff_chans,
                                   size_t num_in_bytes)
{
    /*
     * Move from front to back so that the conversion can be done in-place
     * i.e. in_buff == out_buff
     * NOTE: num_in_samples * out_buff_channels must be an even multiple of in_buff_chans
     */
    size_t num_in_samples = num_in_bytes / sizeof(int16_t);

    size_t num_out_samples = (num_in_samples * out_buff_chans) / in_buff_chans;

    size_t num_skip_samples = in_buff_chans - out_buff_chans;

    int16_t* dst_ptr = out_buff;
    const int16_t* src_ptr = in_buff;
    size_t src_index;
    for (src_index = 0; src_index < num_in_samples; src_index += in_buff_chans) {
        size_t dst_offset;
        for (dst_offset = 0; dst_offset < out_buff_chans; dst_offset++) {
            *dst_ptr++ = *src_ptr++;
        }
        src_ptr += num_skip_samples;
    }

    /* return number of *bytes* generated */
    return num_out_samples * sizeof(int16_t);
}

/*
 * Convert a buffer of N-channel, interleaved PCM32 samples to M-channel PCM32 channels
 * (where N < M).
 *   in_buff points to the buffer of PCM32 samples
 *   in_buff_channels Specifies the number of channels in the input buffer.
 *   out_buff points to the buffer to receive converted PCM32 samples.
 *   out_buff_channels Specifies the number of channels in the output buffer.
 *   num_in_bytes size of input buffer in BYTES
 * returns
 *   the number of BYTES of output data.
 * NOTE
 *   channels > N are filled with silence.
 *   This conversion is safe to do in-place (in_buff == out_buff)
 * We are doing this since we *always* present to The Framework as STEREO device, but need to
 * support 4-channel devices.
 * TODO Move this to a utilities module.
 */
static size_t expand_channels_32(const int32_t* in_buff, size_t in_buff_chans,
                                 int32_t* out_buff, size_t out_buff_chans,
                                 size_t num_in_bytes)
{
    /*
     * Move from back to front so that the conversion can be done in-place
     * i.e. in_buff == out_buff
     * NOTE: num_in_samples * out_buff_channels must be an even multiple of in_buff_chans
     */
    size_t num_in_samples = num_in_bytes / sizeof(int32_t);

    size_t num_out_samples = (num_in_samples * out_buff_chans) / in_buff_chans;

    int32_t* dst_ptr = out_buff + num_out_samples - 1;
    const int32_t* src_ptr = in_buff + num_in_samples - 1;
    size_t num_zero_chans = out_buff_chans - in_buff_chans;
    size_t src_index;
    for (src_index = 0; src_index < num_in_samples; src_index += in_buff_chans) {
        size_t dst_offset;
        for (dst_offset = 0; dst_offset < num_zero_chans; dst_offset++) {
            *dst_ptr-- = 0;
        }
        for (; dst_offset < out_buff_chans; dst_offset++) {
            *dst_ptr-- = *src_ptr--;
        }
    }

    /* return number of *bytes* generated */
    return num_out_samples * sizeof(int32_t);
}

/*
 * Convert a buffer of N-channel, interleaved PCM32 samples to M-channel PCM16 channels
 * (where N > M).
 *   in_buff points to the buffer of PCM32 samples
 *   in_buff_channels Specifies the number of channels in the input buffer.
 *   out_buff points to the buffer to receive converted PCM16 samples.
 *   out_buff_channels Specifies the number of channels in the output buffer.
 *   num_in_bytes size of input buffer in BYTES
 * returns
 *   the number of BYTES of output data.
 * NOTE
 *   channels > N are thrown away.
 *   This conversion is safe to do in-place (in_buff == out_buff)
 * We are doing this since we *always* present to The Framework as STEREO device, but need to
 * support 4-channel devices.
 * TODO Move this to a utilities module.
 */
static size_t contract_channels_32(const int32_t* in_buff, size_t in_buff_chans,
                                   int32_t* out_buff, size_t out_buff_chans,
                                   size_t num_in_bytes)
{
    /*
     * Move from front to back so that the conversion can be done in-place
     * i.e. in_buff == out_buff
     * NOTE: num_in_samples * out_buff_channels must be an even multiple of in_buff_chans
     */
    size_t num_in_samples = num_in_bytes / sizeof(int32_t);

    size_t num_out_samples = (num_in_samples * out_buff_chans) / in_buff_chans;

    size_t num_skip_samples = in_buff_chans - out_buff_chans;

    int32_t* dst_ptr = out_buff;
    const int32_t* src_ptr = in_buff;
    size_t src_index;
    for (src_index = 0; src_index < num_in_samples; src_index += in_buff_chans) {
        size_t dst_offset;
        for (dst_offset = 0; dst_offset < out_buff_chans; dst_offset++) {
            *dst_ptr++ = *src_ptr++;
        }
        src_ptr += num_skip_samples;
    }

    /* return number of *bytes* generated */
    return num_out_samples * sizeof(int32_t);
}

static size_t contract_channels(const void* in_buff, size_t in_buff_chans,
                                void* out_buff, size_t out_buff_chans,
                                unsigned sample_size_in_bytes, size_t num_in_bytes)
{
    switch (sample_size_in_bytes) {
    case 2:
        return contract_channels_16((const int16_t*)in_buff, in_buff_chans,
                                    (int16_t*)out_buff, out_buff_chans,
                                    num_in_bytes);

    /* TODO - do this conversion when we have a device to test it with */
    case 3:
        ALOGE("24-bit channel contraction not supported.");
        return 0;

    case 4:
        return contract_channels_32((const int32_t*)in_buff, in_buff_chans,
                                    (int32_t*)out_buff, out_buff_chans,
                                    num_in_bytes);

    default:
        return 0;
    }
}

static size_t expand_channels(const void* in_buff, size_t in_buff_chans,
                                void* out_buff, size_t out_buff_chans,
                                unsigned sample_size_in_bytes, size_t num_in_bytes)
{
    switch (sample_size_in_bytes) {
    case 2:
        return expand_channels_16((const int16_t*)in_buff, in_buff_chans,
                                  (int16_t*)out_buff, out_buff_chans,
                                  num_in_bytes);

    /* TODO - do this conversion when we have a device to test it with */
    case 3:
        ALOGE("24-bit channel expansion not supported.");
        return 0;

    case 4:
        return expand_channels_32((const int32_t*)in_buff, in_buff_chans,
                                  (int32_t*)out_buff, out_buff_chans,
                                  num_in_bytes);

    default:
        return 0;
    }
}

static size_t adjust_channels(const void* in_buff, size_t in_buff_chans,
                              void* out_buff, size_t out_buff_chans,
                              unsigned sample_size_in_bytes, size_t num_in_bytes)
{
    if (out_buff_chans > in_buff_chans) {
        return expand_channels(in_buff, in_buff_chans, out_buff,  out_buff_chans,
                               sample_size_in_bytes, num_in_bytes);
    } else if (out_buff_chans < in_buff_chans) {
        return contract_channels(in_buff, in_buff_chans, out_buff,  out_buff_chans,
                                 sample_size_in_bytes, num_in_bytes);
    } else if (in_buff != out_buff) {
        memcpy(out_buff, in_buff, num_in_bytes);
    }

    return num_in_bytes;
}

/*
 * ALSA Utilities
 */
/*TODO This table and the function that uses it should be moved to a utilities module (probably) */
/*
 * Maps bit-positions in a pcm_mask to the corresponding AUDIO_ format string.
 */
static const char * const format_string_map[] = {
    "AUDIO_FORMAT_PCM_8_BIT",           /* 00 - SNDRV_PCM_FORMAT_S8 */
    "AUDIO_FORMAT_PCM_8_BIT",           /* 01 - SNDRV_PCM_FORMAT_U8 */
    "AUDIO_FORMAT_PCM_16_BIT",          /* 02 - SNDRV_PCM_FORMAT_S16_LE */
    NULL,                               /* 03 - SNDRV_PCM_FORMAT_S16_BE */
    NULL,                               /* 04 - SNDRV_PCM_FORMAT_U16_LE */
    NULL,                               /* 05 - SNDRV_PCM_FORMAT_U16_BE */
    "AUDIO_FORMAT_PCM_24_BIT_PACKED",   /* 06 - SNDRV_PCM_FORMAT_S24_LE */
    NULL,                               /* 07 - SNDRV_PCM_FORMAT_S24_BE */
    NULL,                               /* 08 - SNDRV_PCM_FORMAT_U24_LE */
    NULL,                               /* 09 - SNDRV_PCM_FORMAT_U24_BE */
    "AUDIO_FORMAT_PCM_32_BIT",          /* 10 - SNDRV_PCM_FORMAT_S32_LE */
    NULL,                               /* 11 - SNDRV_PCM_FORMAT_S32_BE */
    NULL,                               /* 12 - SNDRV_PCM_FORMAT_U32_LE */
    NULL,                               /* 13 - SNDRV_PCM_FORMAT_U32_BE */
    "AUDIO_FORMAT_PCM_FLOAT",           /* 14 - SNDRV_PCM_FORMAT_FLOAT_LE */
    NULL,                               /* 15 - SNDRV_PCM_FORMAT_FLOAT_BE */
    NULL,                               /* 16 - SNDRV_PCM_FORMAT_FLOAT64_LE */
    NULL,                               /* 17 - SNDRV_PCM_FORMAT_FLOAT64_BE */
    NULL,                               /* 18 - SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE */
    NULL,                               /* 19 - SNDRV_PCM_FORMAT_IEC958_SUBFRAME_BE */
    NULL,                               /* 20 - SNDRV_PCM_FORMAT_MU_LAW */
    NULL,                               /* 21 - SNDRV_PCM_FORMAT_A_LAW */
    NULL,                               /* 22 - SNDRV_PCM_FORMAT_IMA_ADPCM */
    NULL,                               /* 23 - SNDRV_PCM_FORMAT_MPEG */
    NULL,                               /* 24 - SNDRV_PCM_FORMAT_GSM */
    NULL, NULL, NULL, NULL, NULL, NULL, /* 25 -> 30 (not assigned) */
    NULL,                               /* 31 - SNDRV_PCM_FORMAT_SPECIAL */
    "AUDIO_FORMAT_PCM_24_BIT_PACKED",   /* 32 - SNDRV_PCM_FORMAT_S24_3LE */ /* ??? */
    NULL,                               /* 33 - SNDRV_PCM_FORMAT_S24_3BE */
    NULL,                               /* 34 - SNDRV_PCM_FORMAT_U24_3LE */
    NULL,                               /* 35 - SNDRV_PCM_FORMAT_U24_3BE */
    NULL,                               /* 36 - SNDRV_PCM_FORMAT_S20_3LE */
    NULL,                               /* 37 - SNDRV_PCM_FORMAT_S20_3BE */
    NULL,                               /* 38 - SNDRV_PCM_FORMAT_U20_3LE */
    NULL,                               /* 39 - SNDRV_PCM_FORMAT_U20_3BE */
    NULL,                               /* 40 - SNDRV_PCM_FORMAT_S18_3LE */
    NULL,                               /* 41 - SNDRV_PCM_FORMAT_S18_3BE */
    NULL,                               /* 42 - SNDRV_PCM_FORMAT_U18_3LE */
    NULL,                               /* 43 - SNDRV_PCM_FORMAT_U18_3BE */
    NULL,                               /* 44 - SNDRV_PCM_FORMAT_G723_24 */
    NULL,                               /* 45 - SNDRV_PCM_FORMAT_G723_24_1B */
    NULL,                               /* 46 - SNDRV_PCM_FORMAT_G723_40 */
    NULL,                               /* 47 - SNDRV_PCM_FORMAT_G723_40_1B */
    NULL,                               /* 48 - SNDRV_PCM_FORMAT_DSD_U8 */
    NULL                                /* 49 - SNDRV_PCM_FORMAT_DSD_U16_LE */
};

/*
 * Generate string containing a bar ("|") delimited list of AUDIO_ formats specified in
 * the mask parameter.
 *
 */
static char* get_format_str_for_mask(struct pcm_mask* mask)
{
    char buffer[256];
    int buffer_size = sizeof(buffer) / sizeof(buffer[0]);
    buffer[0] = '\0';

    int num_slots = sizeof(mask->bits) / sizeof(mask->bits[0]);
    int bits_per_slot = sizeof(mask->bits[0]) * 8;

    const char* format_str = NULL;
    int table_size = sizeof(format_string_map)/sizeof(format_string_map[0]);

    int slot_index, bit_index, table_index;
    table_index = 0;
    int num_written = 0;
    for (slot_index = 0; slot_index < num_slots; slot_index++) {
        unsigned bit_mask = 1;
        for (bit_index = 0; bit_index < bits_per_slot; bit_index++) {
            if ((mask->bits[slot_index] & bit_mask) != 0) {
                format_str = table_index < table_size
                                ? format_string_map[table_index]
                                : NULL;
                if (format_str != NULL) {
                    if (num_written != 0) {
                        num_written += snprintf(buffer + num_written,
                                                buffer_size - num_written, "|");
                    }
                    num_written += snprintf(buffer + num_written, buffer_size - num_written,
                                            "%s", format_str);
                }
            }
            bit_mask <<= 1;
            table_index++;
        }
    }

    return strdup(buffer);
}

/*
 * Maps from bit position in pcm_mask to AUDIO_ format constants.
 */
static audio_format_t const format_value_map[] = {
    AUDIO_FORMAT_PCM_8_BIT,           /* 00 - SNDRV_PCM_FORMAT_S8 */
    AUDIO_FORMAT_PCM_8_BIT,           /* 01 - SNDRV_PCM_FORMAT_U8 */
    AUDIO_FORMAT_PCM_16_BIT,          /* 02 - SNDRV_PCM_FORMAT_S16_LE */
    AUDIO_FORMAT_INVALID,             /* 03 - SNDRV_PCM_FORMAT_S16_BE */
    AUDIO_FORMAT_INVALID,             /* 04 - SNDRV_PCM_FORMAT_U16_LE */
    AUDIO_FORMAT_INVALID,             /* 05 - SNDRV_PCM_FORMAT_U16_BE */
    AUDIO_FORMAT_INVALID,             /* 06 - SNDRV_PCM_FORMAT_S24_LE */
    AUDIO_FORMAT_INVALID,             /* 07 - SNDRV_PCM_FORMAT_S24_BE */
    AUDIO_FORMAT_INVALID,             /* 08 - SNDRV_PCM_FORMAT_U24_LE */
    AUDIO_FORMAT_INVALID,             /* 09 - SNDRV_PCM_FORMAT_U24_BE */
    AUDIO_FORMAT_PCM_32_BIT,          /* 10 - SNDRV_PCM_FORMAT_S32_LE */
    AUDIO_FORMAT_INVALID,             /* 11 - SNDRV_PCM_FORMAT_S32_BE */
    AUDIO_FORMAT_INVALID,             /* 12 - SNDRV_PCM_FORMAT_U32_LE */
    AUDIO_FORMAT_INVALID,             /* 13 - SNDRV_PCM_FORMAT_U32_BE */
    AUDIO_FORMAT_PCM_FLOAT,           /* 14 - SNDRV_PCM_FORMAT_FLOAT_LE */
    AUDIO_FORMAT_INVALID,             /* 15 - SNDRV_PCM_FORMAT_FLOAT_BE */
    AUDIO_FORMAT_INVALID,             /* 16 - SNDRV_PCM_FORMAT_FLOAT64_LE */
    AUDIO_FORMAT_INVALID,             /* 17 - SNDRV_PCM_FORMAT_FLOAT64_BE */
    AUDIO_FORMAT_INVALID,             /* 18 - SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE */
    AUDIO_FORMAT_INVALID,             /* 19 - SNDRV_PCM_FORMAT_IEC958_SUBFRAME_BE */
    AUDIO_FORMAT_INVALID,             /* 20 - SNDRV_PCM_FORMAT_MU_LAW */
    AUDIO_FORMAT_INVALID,             /* 21 - SNDRV_PCM_FORMAT_A_LAW */
    AUDIO_FORMAT_INVALID,             /* 22 - SNDRV_PCM_FORMAT_IMA_ADPCM */
    AUDIO_FORMAT_INVALID,             /* 23 - SNDRV_PCM_FORMAT_MPEG */
    AUDIO_FORMAT_INVALID,             /* 24 - SNDRV_PCM_FORMAT_GSM */
    AUDIO_FORMAT_INVALID,             /* 25 -> 30 (not assigned) */
    AUDIO_FORMAT_INVALID,
    AUDIO_FORMAT_INVALID,
    AUDIO_FORMAT_INVALID,
    AUDIO_FORMAT_INVALID,
    AUDIO_FORMAT_INVALID,
    AUDIO_FORMAT_INVALID,             /* 31 - SNDRV_PCM_FORMAT_SPECIAL */
    AUDIO_FORMAT_PCM_24_BIT_PACKED,   /* 32 - SNDRV_PCM_FORMAT_S24_3LE */
    AUDIO_FORMAT_INVALID,             /* 33 - SNDRV_PCM_FORMAT_S24_3BE */
    AUDIO_FORMAT_INVALID,             /* 34 - SNDRV_PCM_FORMAT_U24_3LE */
    AUDIO_FORMAT_INVALID,             /* 35 - SNDRV_PCM_FORMAT_U24_3BE */
    AUDIO_FORMAT_INVALID,             /* 36 - SNDRV_PCM_FORMAT_S20_3LE */
    AUDIO_FORMAT_INVALID,             /* 37 - SNDRV_PCM_FORMAT_S20_3BE */
    AUDIO_FORMAT_INVALID,             /* 38 - SNDRV_PCM_FORMAT_U20_3LE */
    AUDIO_FORMAT_INVALID,             /* 39 - SNDRV_PCM_FORMAT_U20_3BE */
    AUDIO_FORMAT_INVALID,             /* 40 - SNDRV_PCM_FORMAT_S18_3LE */
    AUDIO_FORMAT_INVALID,             /* 41 - SNDRV_PCM_FORMAT_S18_3BE */
    AUDIO_FORMAT_INVALID,             /* 42 - SNDRV_PCM_FORMAT_U18_3LE */
    AUDIO_FORMAT_INVALID,             /* 43 - SNDRV_PCM_FORMAT_U18_3BE */
    AUDIO_FORMAT_INVALID,             /* 44 - SNDRV_PCM_FORMAT_G723_24 */
    AUDIO_FORMAT_INVALID,             /* 45 - SNDRV_PCM_FORMAT_G723_24_1B */
    AUDIO_FORMAT_INVALID,             /* 46 - SNDRV_PCM_FORMAT_G723_40 */
    AUDIO_FORMAT_INVALID,             /* 47 - SNDRV_PCM_FORMAT_G723_40_1B */
    AUDIO_FORMAT_INVALID,             /* 48 - SNDRV_PCM_FORMAT_DSD_U8 */
    AUDIO_FORMAT_INVALID              /* 49 - SNDRV_PCM_FORMAT_DSD_U16_LE */
};

/*
 * Returns true if mask indicates support for PCM_16.
 */
static bool mask_has_pcm_16(struct pcm_mask* mask) {
    return (mask->bits[0] & 0x0004) != 0;
}

static audio_format_t get_format_for_mask(struct pcm_mask* mask)
{
    int num_slots = sizeof(mask->bits)/ sizeof(mask->bits[0]);
    int bits_per_slot = sizeof(mask->bits[0]) * 8;

    int table_size = sizeof(format_value_map) / sizeof(format_value_map[0]);

    int slot_index, bit_index, table_index;
    table_index = 0;
    int num_written = 0;
    for (slot_index = 0; slot_index < num_slots; slot_index++) {
        unsigned bit_mask = 1;
        for (bit_index = 0; bit_index < bits_per_slot; bit_index++) {
            if ((mask->bits[slot_index] & bit_mask) != 0) {
                /* just return the first one */
                return table_index < table_size
                           ? format_value_map[table_index]
                           : AUDIO_FORMAT_INVALID;
            }
            bit_mask <<= 1;
            table_index++;
        }
    }

    return AUDIO_FORMAT_INVALID;
}

/*
 * Maps from bit position in pcm_mask to PCM_ format constants.
 */
static int8_t const pcm_format_value_map[] = {
    PCM_FORMAT_S8,          /* 00 - SNDRV_PCM_FORMAT_S8 */
    PCM_FORMAT_INVALID,     /* 01 - SNDRV_PCM_FORMAT_U8 */
    PCM_FORMAT_S16_LE,      /* 02 - SNDRV_PCM_FORMAT_S16_LE */
    PCM_FORMAT_INVALID,     /* 03 - SNDRV_PCM_FORMAT_S16_BE */
    PCM_FORMAT_INVALID,     /* 04 - SNDRV_PCM_FORMAT_U16_LE */
    PCM_FORMAT_INVALID,     /* 05 - SNDRV_PCM_FORMAT_U16_BE */
    PCM_FORMAT_S24_3LE,     /* 06 - SNDRV_PCM_FORMAT_S24_LE */
    PCM_FORMAT_INVALID,     /* 07 - SNDRV_PCM_FORMAT_S24_BE */
    PCM_FORMAT_INVALID,     /* 08 - SNDRV_PCM_FORMAT_U24_LE */
    PCM_FORMAT_INVALID,     /* 09 - SNDRV_PCM_FORMAT_U24_BE */
    PCM_FORMAT_S32_LE,      /* 10 - SNDRV_PCM_FORMAT_S32_LE */
    PCM_FORMAT_INVALID,     /* 11 - SNDRV_PCM_FORMAT_S32_BE */
    PCM_FORMAT_INVALID,     /* 12 - SNDRV_PCM_FORMAT_U32_LE */
    PCM_FORMAT_INVALID,     /* 13 - SNDRV_PCM_FORMAT_U32_BE */
    PCM_FORMAT_INVALID,     /* 14 - SNDRV_PCM_FORMAT_FLOAT_LE */
    PCM_FORMAT_INVALID,     /* 15 - SNDRV_PCM_FORMAT_FLOAT_BE */
    PCM_FORMAT_INVALID,     /* 16 - SNDRV_PCM_FORMAT_FLOAT64_LE */
    PCM_FORMAT_INVALID,     /* 17 - SNDRV_PCM_FORMAT_FLOAT64_BE */
    PCM_FORMAT_INVALID,     /* 18 - SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE */
    PCM_FORMAT_INVALID,     /* 19 - SNDRV_PCM_FORMAT_IEC958_SUBFRAME_BE */
    PCM_FORMAT_INVALID,     /* 20 - SNDRV_PCM_FORMAT_MU_LAW */
    PCM_FORMAT_INVALID,     /* 21 - SNDRV_PCM_FORMAT_A_LAW */
    PCM_FORMAT_INVALID,     /* 22 - SNDRV_PCM_FORMAT_IMA_ADPCM */
    PCM_FORMAT_INVALID,     /* 23 - SNDRV_PCM_FORMAT_MPEG */
    PCM_FORMAT_INVALID,     /* 24 - SNDRV_PCM_FORMAT_GSM */
    PCM_FORMAT_INVALID,     /* 25 -> 30 (not assigned) */
    PCM_FORMAT_INVALID,
    PCM_FORMAT_INVALID,
    PCM_FORMAT_INVALID,
    PCM_FORMAT_INVALID,
    PCM_FORMAT_INVALID,
    PCM_FORMAT_INVALID,     /* 31 - SNDRV_PCM_FORMAT_SPECIAL */
    PCM_FORMAT_S24_3LE,     /* 32 - SNDRV_PCM_FORMAT_S24_3LE */ /* ??? */
    PCM_FORMAT_INVALID,     /* 33 - SNDRV_PCM_FORMAT_S24_3BE */
    PCM_FORMAT_INVALID,     /* 34 - SNDRV_PCM_FORMAT_U24_3LE */
    PCM_FORMAT_INVALID,     /* 35 - SNDRV_PCM_FORMAT_U24_3BE */
    PCM_FORMAT_INVALID,     /* 36 - SNDRV_PCM_FORMAT_S20_3LE */
    PCM_FORMAT_INVALID,     /* 37 - SNDRV_PCM_FORMAT_S20_3BE */
    PCM_FORMAT_INVALID,     /* 38 - SNDRV_PCM_FORMAT_U20_3LE */
    PCM_FORMAT_INVALID,     /* 39 - SNDRV_PCM_FORMAT_U20_3BE */
    PCM_FORMAT_INVALID,     /* 40 - SNDRV_PCM_FORMAT_S18_3LE */
    PCM_FORMAT_INVALID,     /* 41 - SNDRV_PCM_FORMAT_S18_3BE */
    PCM_FORMAT_INVALID,     /* 42 - SNDRV_PCM_FORMAT_U18_3LE */
    PCM_FORMAT_INVALID,     /* 43 - SNDRV_PCM_FORMAT_U18_3BE */
    PCM_FORMAT_INVALID,     /* 44 - SNDRV_PCM_FORMAT_G723_24 */
    PCM_FORMAT_INVALID,     /* 45 - SNDRV_PCM_FORMAT_G723_24_1B */
    PCM_FORMAT_INVALID,     /* 46 - SNDRV_PCM_FORMAT_G723_40 */
    PCM_FORMAT_INVALID,     /* 47 - SNDRV_PCM_FORMAT_G723_40_1B */
    PCM_FORMAT_INVALID,     /* 48 - SNDRV_PCM_FORMAT_DSD_U8 */
    PCM_FORMAT_INVALID      /* 49 - SNDRV_PCM_FORMAT_DSD_U16_LE */
};

/*
 * Scans the provided format mask and returns the first non-8 bit sample
 * format supported by the devices.
 */
static int get_pcm_format_for_mask(struct pcm_mask* mask) {
    int num_slots = sizeof(mask->bits)/ sizeof(mask->bits[0]);
    int bits_per_slot = sizeof(mask->bits[0]) * 8;

    int table_size = ARRAY_SIZE(pcm_format_value_map);

    int slot_index, bit_index, table_index;
    table_index = 0;
    int num_written = 0;
    for (slot_index = 0; slot_index < num_slots && table_index < table_size; slot_index++) {
        unsigned bit_mask = 1;
        for (bit_index = 0; bit_index < bits_per_slot  && table_index < table_size; bit_index++) {
            if ((mask->bits[slot_index] & bit_mask) != 0) {
                /* TODO - we don't want a low-level function to be making this decision */
                if (table_index != 0) { /* Don't pick 8-bit */
                    /* just return the first one */
                    return (int)pcm_format_value_map[table_index];
                }
            }
            bit_mask <<= 1;
            table_index++;
        }
    }

    return PCM_FORMAT_INVALID;
}

static bool test_out_sample_rate(struct audio_device_profile* dev_profile, unsigned rate) {
    struct pcm_config local_config = cached_output_hardware_config;
    local_config.rate = rate;

    bool works = false; /* let's be pessimistic */
    struct pcm * pcm =
        pcm_open(dev_profile->card, dev_profile->device, dev_profile->direction, &local_config);

    if (pcm != NULL) {
        works = pcm_is_ready(pcm);
        pcm_close(pcm);
    }

    return works;
}

/* sort these highest -> lowest */
static const unsigned std_sample_rates[] =
    {48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000};

static char* enum_std_sample_rates(struct audio_device_profile* dev_profile,
                                   unsigned min, unsigned max)
{
    char buffer[128];
    buffer[0] = '\0';
    int buffSize = ARRAY_SIZE(buffer);

    char numBuffer[32];

    int numEntries = 0;
    unsigned index;
    for(index = 0; index < ARRAY_SIZE(std_sample_rates); index++) {
        if (std_sample_rates[index] >= min && std_sample_rates[index] <= max &&
            test_out_sample_rate(dev_profile, std_sample_rates[index])) {
            if (numEntries++ != 0) {
                strncat(buffer, "|", buffSize);
            }
            snprintf(numBuffer, sizeof(numBuffer), "%u", std_sample_rates[index]);
            strncat(buffer, numBuffer, buffSize);
        }
    }

    return strdup(buffer);
}

/*
 * Logging
 */
static void log_pcm_mask(const char* mask_name, struct pcm_mask* mask) {
    char buff[512];
    char bit_buff[32];
    int buffSize = sizeof(buff)/sizeof(buff[0]);

    buff[0] = '\0';

    int num_slots = sizeof(mask->bits) / sizeof(mask->bits[0]);
    int bits_per_slot = sizeof(mask->bits[0]) * 8;

    int slot_index, bit_index;
    strcat(buff, "[");
    for (slot_index = 0; slot_index < num_slots; slot_index++) {
        unsigned bit_mask = 1;
        for (bit_index = 0; bit_index < bits_per_slot; bit_index++) {
            strcat(buff, (mask->bits[slot_index] & bit_mask) != 0 ? "1" : "0");
            bit_mask <<= 1;
        }
        if (slot_index < num_slots - 1) {
            strcat(buff, ",");
        }
    }
    strcat(buff, "]");

    ALOGV("usb:audio_hw - %s mask:%s", mask_name, buff);
}

static void log_pcm_params(struct pcm_params * alsa_hw_params) {
    ALOGV("usb:audio_hw - PCM_PARAM_SAMPLE_BITS min:%u, max:%u",
          pcm_params_get_min(alsa_hw_params, PCM_PARAM_SAMPLE_BITS),
          pcm_params_get_max(alsa_hw_params, PCM_PARAM_SAMPLE_BITS));
    ALOGV("usb:audio_hw - PCM_PARAM_FRAME_BITS min:%u, max:%u",
          pcm_params_get_min(alsa_hw_params, PCM_PARAM_FRAME_BITS),
          pcm_params_get_max(alsa_hw_params, PCM_PARAM_FRAME_BITS));
    log_pcm_mask("PCM_PARAM_FORMAT", pcm_params_get_mask(alsa_hw_params, PCM_PARAM_FORMAT));
    log_pcm_mask("PCM_PARAM_SUBFORMAT", pcm_params_get_mask(alsa_hw_params, PCM_PARAM_SUBFORMAT));
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
 * Returns the supplied value rounded up to the next even multiple of 16
 */
static unsigned int round_to_16_mult(unsigned int size) {
    return (size + 15) & 0xFFFFFFF0;
}

/*TODO - Evaluate if this value should/can be retrieved from a device-specific property */
#define MIN_BUFF_TIME   5   /* milliseconds */

/*
 * Returns the system defined minimum period size based on the supplied sample rate
 */
static unsigned int calc_min_period_size(unsigned int sample_rate) {
    unsigned int period_size = (sample_rate * MIN_BUFF_TIME) / 1000;
    return round_to_16_mult(period_size);
}

/*
 * Reads and decodes configuration info from the specified ALSA card/device
 */
static int read_alsa_device_config(struct audio_device_profile * dev_profile,
                                   struct pcm_config * config)
{
    ALOGV("usb:audio_hw - read_alsa_device_config(c:%d d:%d t:0x%X)",
          dev_profile->card, dev_profile->device, dev_profile->direction);

    if (dev_profile->card < 0 || dev_profile->device < 0) {
        return -EINVAL;
    }

    struct pcm_params * alsa_hw_params =
        pcm_params_get(dev_profile->card, dev_profile->device, dev_profile->direction);
    if (alsa_hw_params == NULL) {
        return -EINVAL;
    }

    int ret = 0;

    /*
     * This Logging will be useful when testing new USB devices.
     */
#ifdef LOG_PCM_PARAMS
    log_pcm_params(alsa_hw_params);
#endif

    config->channels = pcm_params_get_min(alsa_hw_params, PCM_PARAM_CHANNELS);
    config->rate = pcm_params_get_min(alsa_hw_params, PCM_PARAM_RATE);
    config->period_size = pcm_params_get_min(alsa_hw_params, PCM_PARAM_BUFFER_SIZE);
    /* round this up to a multiple of 16 */
    config->period_size = round_to_16_mult(config->period_size);
    /* make sure it is above a minimum value to minimize jitter */
    unsigned int min_period_size = calc_min_period_size(config->rate);
    if (config->period_size < min_period_size) {
        config->period_size = min_period_size;
    }
    config->period_count = pcm_params_get_min(alsa_hw_params, PCM_PARAM_PERIODS);

    int format = get_pcm_format_for_mask(pcm_params_get_mask(alsa_hw_params, PCM_PARAM_FORMAT));
    if (format == PCM_FORMAT_INVALID) {
        ret = -EINVAL;
    } else {
        config->format = format;
    }

    pcm_params_free(alsa_hw_params);
    return ret;
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
    return cached_output_hardware_config.period_size *
                audio_stream_out_frame_size((const struct audio_stream_out *)stream);
}

static uint32_t out_get_channels(const struct audio_stream *stream)
{
    // Always Stero for now. We will do *some* conversions in this HAL.
    /* TODO When AudioPolicyManager & AudioFlinger supports arbitrary channels
       rewrite this to return the ACTUAL channel format */
    return AUDIO_CHANNEL_OUT_STEREO;
}

static audio_format_t out_get_format(const struct audio_stream *stream)
{
    return audio_format_from_pcm_format(cached_output_hardware_config.format);
}

static int out_set_format(struct audio_stream *stream, audio_format_t format)
{
    cached_output_hardware_config.format = pcm_format_from_audio_format(format);
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
    struct str_parms *parms;
    char value[32];
    int param_val;
    int routing = 0;
    int ret_value = 0;

    parms = str_parms_create_str(kvpairs);
    pthread_mutex_lock(&out->dev->lock);
    pthread_mutex_lock(&out->lock);

    bool recache_device_params = false;
    param_val = str_parms_get_str(parms, "card", value, sizeof(value));
    if (param_val >= 0) {
        out->profile->card = atoi(value);
        recache_device_params = true;
    }

    param_val = str_parms_get_str(parms, "device", value, sizeof(value));
    if (param_val >= 0) {
        out->profile->device = atoi(value);
        recache_device_params = true;
    }

    if (recache_device_params && out->profile->card >= 0 && out->profile->device >= 0) {
        ret_value = read_alsa_device_config(out->profile, &cached_output_hardware_config);
        output_hardware_config_is_cached = (ret_value == 0);
    }

    pthread_mutex_unlock(&out->lock);
    pthread_mutex_unlock(&out->dev->lock);
    str_parms_destroy(parms);

    return ret_value;
}

static char * device_get_parameters(struct audio_device_profile * dev_profile, const char *keys)
{
    ALOGV("usb:audio_hw::device_get_parameters() keys:%s", keys);

    if (dev_profile->card < 0 || dev_profile->device < 0) {
        return strdup("");
    }

    unsigned min, max;

    struct str_parms *query = str_parms_create_str(keys);
    struct str_parms *result = str_parms_create();

    int num_written = 0;
    char buffer[256];
    int buffer_size = sizeof(buffer) / sizeof(buffer[0]);
    char* result_str = NULL;

    struct pcm_params * alsa_hw_params =
        pcm_params_get(dev_profile->card, dev_profile->device, dev_profile->direction);

    // These keys are from hardware/libhardware/include/audio.h
    // supported sample rates
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
        min = pcm_params_get_min(alsa_hw_params, PCM_PARAM_RATE);
        max = pcm_params_get_max(alsa_hw_params, PCM_PARAM_RATE);

        char* rates_list = enum_std_sample_rates(dev_profile, min, max);
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES, rates_list);
        free(rates_list);
    }  // AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES

    // supported channel counts
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)) {
        // TODO remove this hack when it is superceeded by proper multi-channel support
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_CHANNELS,
                          dev_profile->direction == PCM_OUT
                                          ? "AUDIO_CHANNEL_OUT_STEREO"
                                          : "AUDIO_CHANNEL_IN_STEREO");
    }  // AUDIO_PARAMETER_STREAM_SUP_CHANNELS

    // supported sample formats
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
        // TODO remove this hack when we have support for input in non PCM16 formats
        if (dev_profile->direction == PCM_IN) {
            str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_FORMATS, "AUDIO_FORMAT_PCM_16_BIT");
        } else {
            struct pcm_mask * format_mask = pcm_params_get_mask(alsa_hw_params, PCM_PARAM_FORMAT);
            char * format_params = get_format_str_for_mask(format_mask);
            str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_FORMATS, format_params);
            free(format_params);
        }
    }  // AUDIO_PARAMETER_STREAM_SUP_FORMATS

    pcm_params_free(alsa_hw_params);

    result_str = str_parms_to_str(result);

    // done with these...
    str_parms_destroy(query);
    str_parms_destroy(result);

    ALOGV("usb:audio_hw::device_get_parameters = %s", result_str);

    return result_str;
}

static char * out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    ALOGV("usb:audio_hw::out out_get_parameters() keys:%s", keys);

    struct stream_out *out = (struct stream_out *) stream;
    pthread_mutex_lock(&out->dev->lock);
    pthread_mutex_lock(&out->lock);

    char * params_str =  device_get_parameters(out->profile, keys);

    pthread_mutex_unlock(&out->lock);
    pthread_mutex_unlock(&out->dev->lock);

    return params_str;
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    // struct stream_out *out = (struct stream_out *) stream;

    /*TODO Do we need a term here for the USB latency (as reported in the USB descriptors)? */
    uint32_t latency = (cached_output_hardware_config.period_size
                    * cached_output_hardware_config.period_count * 1000)
                    / out_get_sample_rate(&stream->common);
    return latency;
}

static int out_set_volume(struct audio_stream_out *stream, float left, float right)
{
    return -ENOSYS;
}

/* must be called with hw device and output stream mutexes locked */
static int start_output_stream(struct stream_out *out)
{
    int return_val = 0;

    ALOGV("usb:audio_hw::out start_output_stream(card:%d device:%d)",
          out->profile->card, out->profile->device);

    out->pcm = pcm_open(out->profile->card, out->profile->device, PCM_OUT,
                        &cached_output_hardware_config);

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
        /* TODO Remove this when AudioPolicyManger/AudioFlinger support arbitrary formats
           (and do these conversions themselves) */
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
        audio_format_t audio_format = out_get_format(&(out->stream.common));
        unsigned sample_size_in_bytes = audio_bytes_per_sample(audio_format);
        num_write_buff_bytes =
             adjust_channels(write_buff, num_req_channels,
                             out->conversion_buffer, num_device_channels,
                             sample_size_in_bytes, num_write_buff_bytes);
        write_buff = out->conversion_buffer;
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
        usleep(bytes * 1000000 / audio_stream_out_frame_size(stream) /
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

    out->profile = &(adev->out_profile);
    out->profile->direction = PCM_OUT;

    if (output_hardware_config_is_cached) {
        config->sample_rate = cached_output_hardware_config.rate;

        config->format = audio_format_from_pcm_format(cached_output_hardware_config.format);

        config->channel_mask =
                audio_channel_out_mask_from_count(cached_output_hardware_config.channels);
        if (config->channel_mask != AUDIO_CHANNEL_OUT_STEREO) {
            // Always report STEREO for now.  AudioPolicyManagerBase/AudioFlinger dont' understand
            // formats with more channels, so we won't get chosen (say with a 4-channel DAC).
            /*TODO remove this when the above restriction is removed. */
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

    // Close the pcm device
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
    size_t buffer_size = cached_input_hardware_config.period_size *
                            audio_stream_in_frame_size((const struct audio_stream_in *)stream);
    ALOGV("usb: in_get_buffer_size() = %zu", buffer_size);
    return buffer_size;
}

static uint32_t in_get_channels(const struct audio_stream *stream)
{
    // just report stereo for now
    return AUDIO_CHANNEL_IN_STEREO;
}

static audio_format_t in_get_format(const struct audio_stream *stream)
{
    const struct stream_in * in_stream = (const struct stream_in *)stream;

    ALOGV("in_get_format() = %d -> %d", in_stream->input_framework_format,
          audio_format_from_pcm_format(in_stream->input_framework_format));
    /* return audio_format_from_pcm_format(cached_input_hardware_config.format); */
    return audio_format_from_pcm_format(in_stream->input_framework_format);
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
    struct str_parms *parms;
    char value[32];
    int param_val;
    int routing = 0;
    int ret_value = 0;

    parms = str_parms_create_str(kvpairs);
    pthread_mutex_lock(&in->dev->lock);
    pthread_mutex_lock(&in->lock);

    bool recache_device_params = false;

    // Card/Device
    param_val = str_parms_get_str(parms, "card", value, sizeof(value));
    if (param_val >= 0) {
        in->profile->card = atoi(value);
        recache_device_params = true;
    }

    param_val = str_parms_get_str(parms, "device", value, sizeof(value));
    if (param_val >= 0) {
        in->profile->device = atoi(value);
        recache_device_params = true;
    }

    if (recache_device_params && in->profile->card >= 0 && in->profile->device >= 0) {
        ret_value = read_alsa_device_config(in->profile, &cached_input_hardware_config);
        input_hardware_config_is_cached = (ret_value == 0);
    }

    pthread_mutex_unlock(&in->lock);
    pthread_mutex_unlock(&in->dev->lock);
    str_parms_destroy(parms);

    return ret_value;
}

static char * in_get_parameters(const struct audio_stream *stream, const char *keys) {
    ALOGV("usb:audio_hw::in in_get_parameters() keys:%s", keys);

    struct stream_in *in = (struct stream_in *)stream;
    pthread_mutex_lock(&in->dev->lock);
    pthread_mutex_lock(&in->lock);

    char * params_str  = device_get_parameters(in->profile, keys);

    pthread_mutex_unlock(&in->lock);
    pthread_mutex_unlock(&in->dev->lock);

    return params_str;
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
    int return_val = 0;

    ALOGV("usb:audio_hw::start_input_stream(card:%d device:%d)",
          in->profile->card, in->profile->device);

    in->pcm = pcm_open(in->profile->card, in->profile->device, PCM_IN,
                       &cached_input_hardware_config);
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

/* TODO mutex stuff here (see out_write) */
static ssize_t in_read(struct audio_stream_in *stream, void* buffer, size_t bytes)
{
    size_t num_read_buff_bytes = 0;
    void * read_buff = buffer;
    void * out_buff = buffer;

    struct stream_in * in = (struct stream_in *) stream;

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

    /* Assume (for now) that in->input_framework_format == PCM_FORMAT_S16_LE */
    if (cached_input_hardware_config.format == PCM_FORMAT_S24_3LE) {
        /* 24-bit USB device */
        num_read_buff_bytes = (3 * num_read_buff_bytes) / 2;
    } else if (cached_input_hardware_config.format == PCM_FORMAT_S32_LE) {
        /* 32-bit USB device */
        num_read_buff_bytes = num_read_buff_bytes * 2;
    }

    // Setup/Realloc the conversion buffer (if necessary).
    if (num_read_buff_bytes != bytes) {
        if (num_read_buff_bytes > in->conversion_buffer_size) {
            /*TODO Remove this when AudioPolicyManger/AudioFlinger support arbitrary formats
              (and do these conversions themselves) */
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
        if (cached_input_hardware_config.format != PCM_FORMAT_S16_LE) {
            // we need to convert
            if (num_device_channels != num_req_channels) {
                out_buff = read_buff;
            }

            if (cached_input_hardware_config.format == PCM_FORMAT_S24_3LE) {
                num_read_buff_bytes =
                    convert_24_3_to_16(read_buff, num_read_buff_bytes / 3, out_buff);
            } else if (cached_input_hardware_config.format == PCM_FORMAT_S32_LE) {
                num_read_buff_bytes =
                    convert_32_to_16(read_buff, num_read_buff_bytes / 4, out_buff);
            }
            else {
                goto err;
            }
        }

        if (num_device_channels != num_req_channels) {
            out_buff = buffer;
            /* Num Channels conversion */
            if (num_device_channels != num_req_channels) {
                audio_format_t audio_format = in_get_format(&(in->stream.common));
                unsigned sample_size_in_bytes = audio_bytes_per_sample(audio_format);

                num_read_buff_bytes =
                    adjust_channels(read_buff, num_device_channels,
                                    out_buff, num_req_channels,
                                    sample_size_in_bytes, num_read_buff_bytes);
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
                                  struct audio_stream_in **stream_in,
                                  audio_input_flags_t flags __unused)
{
    ALOGV("usb: in adev_open_input_stream() rate:%" PRIu32 ", chanMask:0x%" PRIX32 ", fmt:%" PRIu8,
          config->sample_rate, config->channel_mask, config->format);

    struct stream_in *in = (struct stream_in *)calloc(1, sizeof(struct stream_in));
    int ret = 0;

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

    in->input_framework_format = PCM_FORMAT_S16_LE;

    in->dev = (struct audio_device *)dev;

    in->profile = &(in->dev->in_profile);
    in->profile->direction = PCM_IN;

    if (!input_hardware_config_is_cached) {
        // just return defaults until we can actually query the device.
        cached_input_hardware_config = default_alsa_in_config;
    }

    /* Rate */
    /* TODO Check that the requested rate is valid for the connected device */
    if (config->sample_rate == 0) {
        config->sample_rate = cached_input_hardware_config.rate;
    } else {
        cached_input_hardware_config.rate = config->sample_rate;
    }

    /* Format */
    /* until the framework supports format conversion, just take what it asks for
     * i.e. AUDIO_FORMAT_PCM_16_BIT */
    /* config->format = audio_format_from_pcm_format(cached_input_hardware_config.format); */
    if (config->format == AUDIO_FORMAT_DEFAULT) {
        /* just return AUDIO_FORMAT_PCM_16_BIT until the framework supports other input
         * formats */
        config->format = AUDIO_FORMAT_PCM_16_BIT;
    } else if (config->format == AUDIO_FORMAT_PCM_16_BIT) {
        /* Always accept AUDIO_FORMAT_PCM_16_BIT until the framework supports other input
         * formats */
    } else {
        /* When the framework support other formats, validate here */
        config->format = AUDIO_FORMAT_PCM_16_BIT;
        ret = -EINVAL;
    }

    /* don't change the cached_input_hardware_config, we will open it as what it is and
     * convert as necessary */
    if (config->channel_mask == AUDIO_CHANNEL_NONE) {
        /* just return AUDIO_CHANNEL_IN_STEREO until the framework supports other input
         * formats */
        config->channel_mask = AUDIO_CHANNEL_IN_STEREO;
    } else if (config->channel_mask != AUDIO_CHANNEL_IN_STEREO) {
        /* allow only stereo capture for now */
        config->channel_mask = AUDIO_CHANNEL_IN_STEREO;
        ret = -EINVAL;
    }

    in->standby = true;

    in->conversion_buffer = NULL;
    in->conversion_buffer_size = 0;

    *stream_in = &in->stream;

    return ret;
}

static void adev_close_input_stream(struct audio_hw_device *dev, struct audio_stream_in *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

    // Close the pcm device
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
