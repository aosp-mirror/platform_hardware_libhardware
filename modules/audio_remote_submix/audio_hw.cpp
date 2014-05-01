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

#define LOG_TAG "r_submix"
//#define LOG_NDEBUG 0

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/time.h>

#include <cutils/log.h>
#include <cutils/properties.h>
#include <cutils/str_parms.h>

#include <hardware/audio.h>
#include <hardware/hardware.h>
#include <system/audio.h>

#include <media/AudioParameter.h>
#include <media/AudioBufferProvider.h>
#include <media/nbaio/MonoPipe.h>
#include <media/nbaio/MonoPipeReader.h>

#include <utils/String8.h>

extern "C" {

namespace android {

// Set to 1 to enable extremely verbose logging in this module.
#define SUBMIX_VERBOSE_LOGGING 0
#if SUBMIX_VERBOSE_LOGGING
#define SUBMIX_ALOGV(...) ALOGV(__VA_ARGS__)
#define SUBMIX_ALOGE(...) ALOGE(__VA_ARGS__)
#else
#define SUBMIX_ALOGV(...)
#define SUBMIX_ALOGE(...)
#endif // SUBMIX_VERBOSE_LOGGING

// Size of the pipe's buffer in frames.
#define MAX_PIPE_DEPTH_IN_FRAMES     (1024*8)
// Maximum number of frames users of the input and output stream should buffer.
#define PERIOD_SIZE_IN_FRAMES         1024
// The duration of MAX_READ_ATTEMPTS * READ_ATTEMPT_SLEEP_MS must be stricly inferior to
//   the duration of a record buffer at the current record sample rate (of the device, not of
//   the recording itself). Here we have:
//      3 * 5ms = 15ms < 1024 frames * 1000 / 48000 = 21.333ms
#define MAX_READ_ATTEMPTS            3
#define READ_ATTEMPT_SLEEP_MS        5 // 5ms between two read attempts when pipe is empty
#define DEFAULT_SAMPLE_RATE_HZ       48000 // default sample rate
// See NBAIO_Format frameworks/av/include/media/nbaio/NBAIO.h.
#define DEFAULT_FORMAT               AUDIO_FORMAT_PCM_16_BIT

// Set *result_variable_ptr to true if value_to_find is present in the array array_to_search,
// otherwise set *result_variable_ptr to false.
#define SUBMIX_VALUE_IN_SET(value_to_find, array_to_search, result_variable_ptr) \
    { \
        size_t i; \
        *(result_variable_ptr) = false; \
        for (i = 0; i < sizeof(array_to_search) / sizeof((array_to_search)[0]); i++) { \
          if ((value_to_find) == (array_to_search)[i]) { \
                *(result_variable_ptr) = true; \
                break; \
            } \
        } \
    }

// Configuration of the submix pipe.
struct submix_config {
    // Channel mask field in this data structure is set to either input_channel_mask or
    // output_channel_mask depending upon the last stream to be opened on this device.
    struct audio_config common;
    // Input stream and output stream channel masks.  This is required since input and output
    // channel bitfields are not equivalent.
    audio_channel_mask_t input_channel_mask;
    audio_channel_mask_t output_channel_mask;
    size_t period_size; // Size of the audio pipe is period_size * period_count in frames.
};

struct submix_audio_device {
    struct audio_hw_device device;
    bool input_standby;
    bool output_standby;
    submix_config config;
    // Pipe variables: they handle the ring buffer that "pipes" audio:
    //  - from the submix virtual audio output == what needs to be played
    //    remotely, seen as an output for AudioFlinger
    //  - to the virtual audio source == what is captured by the component
    //    which "records" the submix / virtual audio source, and handles it as needed.
    // A usecase example is one where the component capturing the audio is then sending it over
    // Wifi for presentation on a remote Wifi Display device (e.g. a dongle attached to a TV, or a
    // TV with Wifi Display capabilities), or to a wireless audio player.
    sp<MonoPipe> rsxSink;
    sp<MonoPipeReader> rsxSource;

    // Device lock, also used to protect access to submix_audio_device from the input and output
    // streams.
    pthread_mutex_t lock;
};

struct submix_stream_out {
    struct audio_stream_out stream;
    struct submix_audio_device *dev;
};

struct submix_stream_in {
    struct audio_stream_in stream;
    struct submix_audio_device *dev;
    bool output_standby; // output standby state as seen from record thread

    // wall clock when recording starts
    struct timespec record_start_time;
    // how many frames have been requested to be read
    int64_t read_counter_frames;
};

// Determine whether the specified sample rate is supported by the submix module.
static bool sample_rate_supported(const uint32_t sample_rate)
{
    // Set of sample rates supported by Format_from_SR_C() frameworks/av/media/libnbaio/NAIO.cpp.
    static const unsigned int supported_sample_rates[] = {
        8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000,
    };
    bool return_value;
    SUBMIX_VALUE_IN_SET(sample_rate, supported_sample_rates, &return_value);
    return return_value;
}

// Determine whether the specified sample rate is supported, if it is return the specified sample
// rate, otherwise return the default sample rate for the submix module.
static uint32_t get_supported_sample_rate(uint32_t sample_rate)
{
  return sample_rate_supported(sample_rate) ? sample_rate : DEFAULT_SAMPLE_RATE_HZ;
}

// Determine whether the specified channel in mask is supported by the submix module.
static bool channel_in_mask_supported(const audio_channel_mask_t channel_in_mask)
{
    // Set of channel in masks supported by Format_from_SR_C()
    // frameworks/av/media/libnbaio/NAIO.cpp.
    static const audio_channel_mask_t supported_channel_in_masks[] = {
        AUDIO_CHANNEL_IN_MONO, AUDIO_CHANNEL_IN_STEREO,
    };
    bool return_value;
    SUBMIX_VALUE_IN_SET(channel_in_mask, supported_channel_in_masks, &return_value);
    return return_value;
}

// Determine whether the specified channel in mask is supported, if it is return the specified
// channel in mask, otherwise return the default channel in mask for the submix module.
static audio_channel_mask_t get_supported_channel_in_mask(
        const audio_channel_mask_t channel_in_mask)
{
    return channel_in_mask_supported(channel_in_mask) ? channel_in_mask :
            static_cast<audio_channel_mask_t>(AUDIO_CHANNEL_IN_STEREO);
}

// Determine whether the specified channel out mask is supported by the submix module.
static bool channel_out_mask_supported(const audio_channel_mask_t channel_out_mask)
{
    // Set of channel out masks supported by Format_from_SR_C()
    // frameworks/av/media/libnbaio/NAIO.cpp.
    static const audio_channel_mask_t supported_channel_out_masks[] = {
        AUDIO_CHANNEL_OUT_MONO, AUDIO_CHANNEL_OUT_STEREO,
    };
    bool return_value;
    SUBMIX_VALUE_IN_SET(channel_out_mask, supported_channel_out_masks, &return_value);
    return return_value;
}

// Determine whether the specified channel out mask is supported, if it is return the specified
// channel out mask, otherwise return the default channel out mask for the submix module.
static audio_channel_mask_t get_supported_channel_out_mask(
        const audio_channel_mask_t channel_out_mask)
{
    return channel_out_mask_supported(channel_out_mask) ? channel_out_mask :
        static_cast<audio_channel_mask_t>(AUDIO_CHANNEL_OUT_STEREO);
}

// Get a pointer to submix_stream_out given an audio_stream_out that is embedded within the
// structure.
static struct submix_stream_out * audio_stream_out_get_submix_stream_out(
        struct audio_stream_out * const stream)
{
    ALOG_ASSERT(stream);
    return reinterpret_cast<struct submix_stream_out *>(reinterpret_cast<uint8_t *>(stream) -
                offsetof(struct submix_stream_out, stream));
}

// Get a pointer to submix_stream_out given an audio_stream that is embedded within the structure.
static struct submix_stream_out * audio_stream_get_submix_stream_out(
        struct audio_stream * const stream)
{
    ALOG_ASSERT(stream);
    return audio_stream_out_get_submix_stream_out(
            reinterpret_cast<struct audio_stream_out *>(stream));
}

// Get a pointer to submix_stream_in given an audio_stream_in that is embedded within the
// structure.
static struct submix_stream_in * audio_stream_in_get_submix_stream_in(
        struct audio_stream_in * const stream)
{
    ALOG_ASSERT(stream);
    return reinterpret_cast<struct submix_stream_in *>(reinterpret_cast<uint8_t *>(stream) -
            offsetof(struct submix_stream_in, stream));
}

// Get a pointer to submix_stream_in given an audio_stream that is embedded within the structure.
static struct submix_stream_in * audio_stream_get_submix_stream_in(
        struct audio_stream * const stream)
{
    ALOG_ASSERT(stream);
    return audio_stream_in_get_submix_stream_in(
            reinterpret_cast<struct audio_stream_in *>(stream));
}

// Get a pointer to submix_audio_device given a pointer to an audio_device that is embedded within
// the structure.
static struct submix_audio_device * audio_hw_device_get_submix_audio_device(
        struct audio_hw_device *device)
{
    ALOG_ASSERT(device);
    return reinterpret_cast<struct submix_audio_device *>(reinterpret_cast<uint8_t *>(device) -
        offsetof(struct submix_audio_device, device));
}

// Get the number of channels referenced by the specified channel_mask.  The channel_mask can
// reference either input or output channels.
uint32_t get_channel_count_from_mask(const audio_channel_mask_t channel_mask) {
    if (audio_is_input_channel(channel_mask)) {
        return popcount(channel_mask & AUDIO_CHANNEL_IN_ALL);
    } else if (audio_is_output_channel(channel_mask)) {
        return popcount(channel_mask & AUDIO_CHANNEL_OUT_ALL);
    }
    ALOGE("get_channel_count(): No channels specified in channel mask %x", channel_mask);
    return 0;
}

// Convert a input channel mask to output channel mask where a mapping is available, returns 0
// otherwise.
static audio_channel_mask_t audio_channel_in_mask_to_out(
        const audio_channel_mask_t channel_in_mask)
{
  switch (channel_in_mask) {
      case AUDIO_CHANNEL_IN_MONO:
          return AUDIO_CHANNEL_OUT_MONO;
      case AUDIO_CHANNEL_IN_STEREO:
          return AUDIO_CHANNEL_OUT_STEREO;
      default:
          return 0;
  }
}

// Compare an audio_config with input channel mask and an audio_config with output channel mask
// returning false if they do *not* match, true otherwise.
static bool audio_config_compare(const audio_config * const input_config,
        const audio_config * const output_config)
{
    audio_channel_mask_t channel_mask = audio_channel_in_mask_to_out(input_config->channel_mask);
    if (channel_mask != output_config->channel_mask) {
        ALOGE("audio_config_compare() channel mask mismatch %x (%x) vs. %x",
              channel_mask, input_config->channel_mask, output_config->channel_mask);
        return false;
    }
    if (input_config->sample_rate != output_config->sample_rate) {
        ALOGE("audio_config_compare() sample rate mismatch %ul vs. %ul",
              input_config->sample_rate, output_config->sample_rate);
        return false;
    }
    if (input_config->format != output_config->format) {
        ALOGE("audio_config_compare() format mismatch %x vs. %x",
              input_config->format, output_config->format);
        return false;
    }
    // This purposely ignores offload_info as it's not required for the submix device.
    return true;
}

// Sanitize the user specified audio config for a submix input / output stream.
static void submix_sanitize_config(struct audio_config * const config, const bool is_input_format)
{
    config->channel_mask = is_input_format ? get_supported_channel_in_mask(config->channel_mask) :
            get_supported_channel_out_mask(config->channel_mask);
    config->sample_rate = get_supported_sample_rate(config->sample_rate);
    config->format = DEFAULT_FORMAT;
}

// Verify a submix input or output stream can be opened.
static bool submix_open_validate(const struct submix_audio_device * const rsxadev,
                                 pthread_mutex_t * const lock,
                                 const struct audio_config * const config,
                                 const bool opening_input)
{
    bool pipe_open;
    audio_config pipe_config;

    // Query the device for the current audio config and whether input and output streams are open.
    pthread_mutex_lock(lock);
    pipe_open = rsxadev->rsxSink.get() != NULL || rsxadev->rsxSource.get() != NULL;
    memcpy(&pipe_config, &rsxadev->config.common, sizeof(pipe_config));
    pthread_mutex_unlock(lock);

    // If the pipe is open, verify the existing audio config the pipe matches the user
    // specified config.
    if (pipe_open) {
        const audio_config * const input_config = opening_input ? config : &pipe_config;
        const audio_config * const output_config = opening_input ? &pipe_config : config;
        // Get the channel mask of the open device.
        pipe_config.channel_mask =
            opening_input ? rsxadev->config.output_channel_mask :
                rsxadev->config.input_channel_mask;
        if (!audio_config_compare(input_config, output_config)) {
            ALOGE("submix_open_validate(): Unsupported format.");
            return -EINVAL;
        }
    }
    return true;
}

/* audio HAL functions */

static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    const struct submix_stream_out * const out = audio_stream_get_submix_stream_out(
            const_cast<struct audio_stream *>(stream));
    const uint32_t out_rate = out->dev->config.common.sample_rate;
    SUBMIX_ALOGV("out_get_sample_rate() returns %u", out_rate);
    return out_rate;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    if (!sample_rate_supported(rate)) {
        ALOGE("out_set_sample_rate(rate=%u) rate unsupported", rate);
        return -ENOSYS;
    }
    struct submix_stream_out * const out = audio_stream_get_submix_stream_out(stream);
    SUBMIX_ALOGV("out_set_sample_rate(rate=%u)", rate);
    out->dev->config.common.sample_rate = rate;
    return 0;
}

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    const struct submix_stream_out * const out = audio_stream_get_submix_stream_out(
            const_cast<struct audio_stream *>(stream));
    const struct submix_config * const config = &out->dev->config;
    const size_t buffer_size = config->period_size * audio_stream_frame_size(stream);
    SUBMIX_ALOGV("out_get_buffer_size() returns %zu bytes, %zu frames",
                 buffer_size, config->period_size);
    return buffer_size;
}

static audio_channel_mask_t out_get_channels(const struct audio_stream *stream)
{
    const struct submix_stream_out * const out = audio_stream_get_submix_stream_out(
            const_cast<struct audio_stream *>(stream));
    uint32_t channel_mask = out->dev->config.output_channel_mask;
    SUBMIX_ALOGV("out_get_channels() returns %08x", channel_mask);
    return channel_mask;
}

static audio_format_t out_get_format(const struct audio_stream *stream)
{
    const struct submix_stream_out * const out = audio_stream_get_submix_stream_out(
            const_cast<struct audio_stream *>(stream));
    const audio_format_t format = out->dev->config.common.format;
    SUBMIX_ALOGV("out_get_format() returns %x", format);
    return format;
}

static int out_set_format(struct audio_stream *stream, audio_format_t format)
{
    const struct submix_stream_out * const out = audio_stream_get_submix_stream_out(stream);
    if (format != out->dev->config.common.format) {
        ALOGE("out_set_format(format=%x) format unsupported", format);
        return -ENOSYS;
    }
    SUBMIX_ALOGV("out_set_format(format=%x)", format);
    return 0;
}

static int out_standby(struct audio_stream *stream)
{
    struct submix_audio_device * const rsxadev = audio_stream_get_submix_stream_out(stream)->dev;
    ALOGI("out_standby()");

    pthread_mutex_lock(&rsxadev->lock);

    rsxadev->output_standby = true;

    pthread_mutex_unlock(&rsxadev->lock);

    return 0;
}

static int out_dump(const struct audio_stream *stream, int fd)
{
    (void)stream;
    (void)fd;
    return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    int exiting = -1;
    AudioParameter parms = AudioParameter(String8(kvpairs));
    SUBMIX_ALOGV("out_set_parameters() kvpairs='%s'", kvpairs);
    // FIXME this is using hard-coded strings but in the future, this functionality will be
    //       converted to use audio HAL extensions required to support tunneling
    if ((parms.getInt(String8("exiting"), exiting) == NO_ERROR) && (exiting > 0)) {
        struct submix_audio_device * const rsxadev =
                audio_stream_get_submix_stream_out(stream)->dev;
        pthread_mutex_lock(&rsxadev->lock);
        { // using the sink
            sp<MonoPipe> sink = rsxadev->rsxSink.get();
            if (sink == NULL) {
                pthread_mutex_unlock(&rsxadev->lock);
                return 0;
            }

            ALOGI("out_set_parameters(): shutdown");
            sink->shutdown(true);
        } // done using the sink
        pthread_mutex_unlock(&rsxadev->lock);
    }
    return 0;
}

static char * out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    (void)stream;
    (void)keys;
    return strdup("");
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    const struct submix_stream_out * const out = audio_stream_out_get_submix_stream_out(
            const_cast<struct audio_stream_out *>(stream));
    const struct submix_config * const config = &out->dev->config;
    const uint32_t latency_ms = (MAX_PIPE_DEPTH_IN_FRAMES * 1000) / config->common.sample_rate;
    SUBMIX_ALOGV("out_get_latency() returns %u ms, size in frames %zu, sample rate %u", latency_ms,
          MAX_PIPE_DEPTH_IN_FRAMES, config->common.sample_rate);
    return latency_ms;
}

static int out_set_volume(struct audio_stream_out *stream, float left,
                          float right)
{
    (void)stream;
    (void)left;
    (void)right;
    return -ENOSYS;
}

static ssize_t out_write(struct audio_stream_out *stream, const void* buffer,
                         size_t bytes)
{
    SUBMIX_ALOGV("out_write(bytes=%zd)", bytes);
    ssize_t written_frames = 0;
    const size_t frame_size = audio_stream_frame_size(&stream->common);
    struct submix_audio_device * const rsxadev =
            audio_stream_out_get_submix_stream_out(stream)->dev;
    const size_t frames = bytes / frame_size;

    pthread_mutex_lock(&rsxadev->lock);

    rsxadev->output_standby = false;

    sp<MonoPipe> sink = rsxadev->rsxSink.get();
    if (sink != NULL) {
        if (sink->isShutdown()) {
            sink.clear();
            pthread_mutex_unlock(&rsxadev->lock);
            SUBMIX_ALOGV("out_write(): pipe shutdown, ignoring the write.");
            // the pipe has already been shutdown, this buffer will be lost but we must
            //   simulate timing so we don't drain the output faster than realtime
            usleep(frames * 1000000 / out_get_sample_rate(&stream->common));
            return bytes;
        }
    } else {
        pthread_mutex_unlock(&rsxadev->lock);
        ALOGE("out_write without a pipe!");
        ALOG_ASSERT("out_write without a pipe!");
        return 0;
    }

    pthread_mutex_unlock(&rsxadev->lock);

    written_frames = sink->write(buffer, frames);

    if (written_frames < 0) {
        if (written_frames == (ssize_t)NEGOTIATE) {
            ALOGE("out_write() write to pipe returned NEGOTIATE");

            pthread_mutex_lock(&rsxadev->lock);
            sink.clear();
            pthread_mutex_unlock(&rsxadev->lock);

            written_frames = 0;
            return 0;
        } else {
            // write() returned UNDERRUN or WOULD_BLOCK, retry
            ALOGE("out_write() write to pipe returned unexpected %zd", written_frames);
            written_frames = sink->write(buffer, frames);
        }
    }

    pthread_mutex_lock(&rsxadev->lock);
    sink.clear();
    pthread_mutex_unlock(&rsxadev->lock);

    if (written_frames < 0) {
        ALOGE("out_write() failed writing to pipe with %zd", written_frames);
        return 0;
    }
    const ssize_t written_bytes = written_frames * frame_size;
    SUBMIX_ALOGV("out_write() wrote %zd bytes %zd frames)", written_bytes, written_frames);
    return written_bytes;
}

static int out_get_render_position(const struct audio_stream_out *stream,
                                   uint32_t *dsp_frames)
{
    (void)stream;
    (void)dsp_frames;
    return -EINVAL;
}

static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    (void)stream;
    (void)effect;
    return 0;
}

static int out_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    (void)stream;
    (void)effect;
    return 0;
}

static int out_get_next_write_timestamp(const struct audio_stream_out *stream,
                                        int64_t *timestamp)
{
    (void)stream;
    (void)timestamp;
    return -EINVAL;
}

/** audio_stream_in implementation **/
static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
    const struct submix_stream_in * const in = audio_stream_get_submix_stream_in(
        const_cast<struct audio_stream*>(stream));
    SUBMIX_ALOGV("in_get_sample_rate() returns %u", in->dev->config.common.sample_rate);
    return in->dev->config.common.sample_rate;
}

static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    const struct submix_stream_in * const in = audio_stream_get_submix_stream_in(stream);
    if (!sample_rate_supported(rate)) {
        ALOGE("in_set_sample_rate(rate=%u) rate unsupported", rate);
        return -ENOSYS;
    }
    in->dev->config.common.sample_rate = rate;
    SUBMIX_ALOGV("in_set_sample_rate() set %u", rate);
    return 0;
}

static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    const struct submix_stream_in * const in = audio_stream_get_submix_stream_in(
            const_cast<struct audio_stream*>(stream));
    const size_t buffer_size = in->dev->config.period_size * audio_stream_frame_size(stream);
    SUBMIX_ALOGV("in_get_buffer_size() returns %zu", buffer_size);
    return buffer_size;
}

static audio_channel_mask_t in_get_channels(const struct audio_stream *stream)
{
    const struct submix_stream_in * const in = audio_stream_get_submix_stream_in(
            const_cast<struct audio_stream*>(stream));
    const audio_channel_mask_t channel_mask = in->dev->config.input_channel_mask;
    SUBMIX_ALOGV("in_get_channels() returns %x", channel_mask);
    return channel_mask;
}

static audio_format_t in_get_format(const struct audio_stream *stream)
{
    const struct submix_stream_in * const in = audio_stream_get_submix_stream_in(
            const_cast<struct audio_stream*>(stream));
    const audio_format_t format = in->dev->config.common.format;
    SUBMIX_ALOGV("in_get_format() returns %x", format);
    return format;
}

static int in_set_format(struct audio_stream *stream, audio_format_t format)
{
    const struct submix_stream_in * const in = audio_stream_get_submix_stream_in(stream);
    if (format != in->dev->config.common.format) {
        ALOGE("in_set_format(format=%x) format unsupported", format);
        return -ENOSYS;
    }
    SUBMIX_ALOGV("in_set_format(format=%x)", format);
    return 0;
}

static int in_standby(struct audio_stream *stream)
{
    struct submix_audio_device * const rsxadev = audio_stream_get_submix_stream_in(stream)->dev;
    ALOGI("in_standby()");

    pthread_mutex_lock(&rsxadev->lock);

    rsxadev->input_standby = true;

    pthread_mutex_unlock(&rsxadev->lock);

    return 0;
}

static int in_dump(const struct audio_stream *stream, int fd)
{
    (void)stream;
    (void)fd;
    return 0;
}

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    (void)stream;
    (void)kvpairs;
    return 0;
}

static char * in_get_parameters(const struct audio_stream *stream,
                                const char *keys)
{
    (void)stream;
    (void)keys;
    return strdup("");
}

static int in_set_gain(struct audio_stream_in *stream, float gain)
{
    (void)stream;
    (void)gain;
    return 0;
}

static ssize_t in_read(struct audio_stream_in *stream, void* buffer,
                       size_t bytes)
{
    ssize_t frames_read = -1977;
    struct submix_stream_in * const in = audio_stream_in_get_submix_stream_in(stream);
    struct submix_audio_device * const rsxadev = in->dev;
    const size_t frame_size = audio_stream_frame_size(&stream->common);
    const size_t frames_to_read = bytes / frame_size;

    SUBMIX_ALOGV("in_read bytes=%zu", bytes);
    pthread_mutex_lock(&rsxadev->lock);

    const bool output_standby_transition = (in->output_standby != in->dev->output_standby);
    in->output_standby = rsxadev->output_standby;

    if (rsxadev->input_standby || output_standby_transition) {
        rsxadev->input_standby = false;
        // keep track of when we exit input standby (== first read == start "real recording")
        // or when we start recording silence, and reset projected time
        int rc = clock_gettime(CLOCK_MONOTONIC, &in->record_start_time);
        if (rc == 0) {
            in->read_counter_frames = 0;
        }
    }

    in->read_counter_frames += frames_to_read;
    size_t remaining_frames = frames_to_read;

    {
        // about to read from audio source
        sp<MonoPipeReader> source = rsxadev->rsxSource;
        if (source == NULL) {
            ALOGE("no audio pipe yet we're trying to read!");
            pthread_mutex_unlock(&rsxadev->lock);
            usleep((bytes / frame_size) * 1000000 / in_get_sample_rate(&stream->common));
            memset(buffer, 0, bytes);
            return bytes;
        }

        pthread_mutex_unlock(&rsxadev->lock);

        // read the data from the pipe (it's non blocking)
        int attempts = 0;
        char* buff = (char*)buffer;
        while ((remaining_frames > 0) && (attempts < MAX_READ_ATTEMPTS)) {
            attempts++;
            frames_read = source->read(buff, remaining_frames, AudioBufferProvider::kInvalidPTS);
            if (frames_read > 0) {
                remaining_frames -= frames_read;
                buff += frames_read * frame_size;
                SUBMIX_ALOGV("  in_read (att=%d) got %zd frames, remaining=%zu",
                             attempts, frames_read, remaining_frames);
            } else {
                SUBMIX_ALOGE("  in_read read returned %zd", frames_read);
                usleep(READ_ATTEMPT_SLEEP_MS * 1000);
            }
        }
        // done using the source
        pthread_mutex_lock(&rsxadev->lock);
        source.clear();
        pthread_mutex_unlock(&rsxadev->lock);
    }

    if (remaining_frames > 0) {
        SUBMIX_ALOGV("  remaining_frames = %zu", remaining_frames);
        memset(((char*)buffer)+ bytes - (remaining_frames * frame_size), 0,
                remaining_frames * frame_size);
    }

    // compute how much we need to sleep after reading the data by comparing the wall clock with
    //   the projected time at which we should return.
    struct timespec time_after_read;// wall clock after reading from the pipe
    struct timespec record_duration;// observed record duration
    int rc = clock_gettime(CLOCK_MONOTONIC, &time_after_read);
    const uint32_t sample_rate = in_get_sample_rate(&stream->common);
    if (rc == 0) {
        // for how long have we been recording?
        record_duration.tv_sec  = time_after_read.tv_sec - in->record_start_time.tv_sec;
        record_duration.tv_nsec = time_after_read.tv_nsec - in->record_start_time.tv_nsec;
        if (record_duration.tv_nsec < 0) {
            record_duration.tv_sec--;
            record_duration.tv_nsec += 1000000000;
        }

        // read_counter_frames contains the number of frames that have been read since the
        // beginning of recording (including this call): it's converted to usec and compared to
        // how long we've been recording for, which gives us how long we must wait to sync the
        // projected recording time, and the observed recording time.
        long projected_vs_observed_offset_us =
                ((int64_t)(in->read_counter_frames
                            - (record_duration.tv_sec*sample_rate)))
                        * 1000000 / sample_rate
                - (record_duration.tv_nsec / 1000);

        SUBMIX_ALOGV("  record duration %5lds %3ldms, will wait: %7ldus",
                record_duration.tv_sec, record_duration.tv_nsec/1000000,
                projected_vs_observed_offset_us);
        if (projected_vs_observed_offset_us > 0) {
            usleep(projected_vs_observed_offset_us);
        }
    }

    SUBMIX_ALOGV("in_read returns %zu", bytes);
    return bytes;

}

static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream)
{
    (void)stream;
    return 0;
}

static int in_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    (void)stream;
    (void)effect;
    return 0;
}

static int in_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    (void)stream;
    (void)effect;
    return 0;
}

static int adev_open_output_stream(struct audio_hw_device *dev,
                                   audio_io_handle_t handle,
                                   audio_devices_t devices,
                                   audio_output_flags_t flags,
                                   struct audio_config *config,
                                   struct audio_stream_out **stream_out)
{
    struct submix_audio_device * const rsxadev = audio_hw_device_get_submix_audio_device(dev);
    ALOGV("adev_open_output_stream()");
    struct submix_stream_out *out;
    int ret;
    (void)handle;
    (void)devices;
    (void)flags;

    // Make sure it's possible to open the device given the current audio config.
    submix_sanitize_config(config, false);
    if (!submix_open_validate(rsxadev, &rsxadev->lock, config, false)) {
        ALOGE("adev_open_output_stream(): Unable to open output stream.");
        return -EINVAL;
    }

    out = (struct submix_stream_out *)calloc(1, sizeof(struct submix_stream_out));
    if (!out) {
        ret = -ENOMEM;
        goto err_open;
    }

    pthread_mutex_lock(&rsxadev->lock);

    // Initialize the function pointer tables (v-tables).
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

    memcpy(&rsxadev->config.common, config, sizeof(rsxadev->config.common));
    rsxadev->config.output_channel_mask = config->channel_mask;

    rsxadev->config.period_size = PERIOD_SIZE_IN_FRAMES;

    // Store a pointer to the device from the output stream.
    out->dev = rsxadev;
    // Return the output stream.
    *stream_out = &out->stream;


    // initialize pipe
    {
        ALOGV("  initializing pipe");
        const NBAIO_Format format = Format_from_SR_C(rsxadev->config.common.sample_rate,
                get_channel_count_from_mask(rsxadev->config.common.channel_mask),
                rsxadev->config.common.format);
        const NBAIO_Format offers[1] = {format};
        size_t numCounterOffers = 0;
        // creating a MonoPipe with optional blocking set to true.
        MonoPipe* sink = new MonoPipe(MAX_PIPE_DEPTH_IN_FRAMES, format, true/*writeCanBlock*/);
        ssize_t index = sink->negotiate(offers, 1, NULL, numCounterOffers);
        ALOG_ASSERT(index == 0);
        MonoPipeReader* source = new MonoPipeReader(sink);
        numCounterOffers = 0;
        index = source->negotiate(offers, 1, NULL, numCounterOffers);
        ALOG_ASSERT(index == 0);
        rsxadev->rsxSink = sink;
        rsxadev->rsxSource = source;
    }

    pthread_mutex_unlock(&rsxadev->lock);

    return 0;

err_open:
    *stream_out = NULL;
    return ret;
}

static void adev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream)
{
    struct submix_audio_device *rsxadev = audio_hw_device_get_submix_audio_device(dev);
    ALOGV("adev_close_output_stream()");

    pthread_mutex_lock(&rsxadev->lock);

    rsxadev->rsxSink.clear();
    rsxadev->rsxSource.clear();
    free(stream);

    pthread_mutex_unlock(&rsxadev->lock);
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
    (void)dev;
    (void)kvpairs;
    return -ENOSYS;
}

static char * adev_get_parameters(const struct audio_hw_device *dev,
                                  const char *keys)
{
    (void)dev;
    (void)keys;
    return strdup("");;
}

static int adev_init_check(const struct audio_hw_device *dev)
{
    ALOGI("adev_init_check()");
    (void)dev;
    return 0;
}

static int adev_set_voice_volume(struct audio_hw_device *dev, float volume)
{
    (void)dev;
    (void)volume;
    return -ENOSYS;
}

static int adev_set_master_volume(struct audio_hw_device *dev, float volume)
{
    (void)dev;
    (void)volume;
    return -ENOSYS;
}

static int adev_get_master_volume(struct audio_hw_device *dev, float *volume)
{
    (void)dev;
    (void)volume;
    return -ENOSYS;
}

static int adev_set_master_mute(struct audio_hw_device *dev, bool muted)
{
    (void)dev;
    (void)muted;
    return -ENOSYS;
}

static int adev_get_master_mute(struct audio_hw_device *dev, bool *muted)
{
    (void)dev;
    (void)muted;
    return -ENOSYS;
}

static int adev_set_mode(struct audio_hw_device *dev, audio_mode_t mode)
{
    (void)dev;
    (void)mode;
    return 0;
}

static int adev_set_mic_mute(struct audio_hw_device *dev, bool state)
{
    (void)dev;
    (void)state;
    return -ENOSYS;
}

static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state)
{
    (void)dev;
    (void)state;
    return -ENOSYS;
}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
                                         const struct audio_config *config)
{
    if (audio_is_linear_pcm(config->format)) {
        const size_t buffer_period_size_frames =
            audio_hw_device_get_submix_audio_device(const_cast<struct audio_hw_device*>(dev))->
                config.period_size;
        const size_t frame_size_in_bytes = get_channel_count_from_mask(config->channel_mask) *
                audio_bytes_per_sample(config->format);
        const size_t buffer_size = buffer_period_size_frames * frame_size_in_bytes;
        SUBMIX_ALOGV("out_get_buffer_size() returns %zu bytes, %zu frames",
                 buffer_size, buffer_period_size_frames);
        return buffer_size;
    }
    return 0;
}

static int adev_open_input_stream(struct audio_hw_device *dev,
                                  audio_io_handle_t handle,
                                  audio_devices_t devices,
                                  struct audio_config *config,
                                  struct audio_stream_in **stream_in)
{
    struct submix_audio_device *rsxadev = audio_hw_device_get_submix_audio_device(dev);
    struct submix_stream_in *in;
    int ret;
    ALOGI("adev_open_input_stream()");
    (void)handle;
    (void)devices;

    // Make sure it's possible to open the device given the current audio config.
    submix_sanitize_config(config, true);
    if (!submix_open_validate(rsxadev, &rsxadev->lock, config, true)) {
        ALOGE("adev_open_input_stream(): Unable to open input stream.");
        return -EINVAL;
    }

    in = (struct submix_stream_in *)calloc(1, sizeof(struct submix_stream_in));
    if (!in) {
        ret = -ENOMEM;
        goto err_open;
    }

    pthread_mutex_lock(&rsxadev->lock);

    // Initialize the function pointer tables (v-tables).
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

    memcpy(&rsxadev->config.common, config, sizeof(rsxadev->config.common));
    rsxadev->config.input_channel_mask = config->channel_mask;

    rsxadev->config.period_size = PERIOD_SIZE_IN_FRAMES;

    *stream_in = &in->stream;

    in->dev = rsxadev;

    // Initialize the input stream.
    in->read_counter_frames = 0;
    in->output_standby = rsxadev->output_standby;

    pthread_mutex_unlock(&rsxadev->lock);

    return 0;

err_open:
    *stream_in = NULL;
    return ret;
}

static void adev_close_input_stream(struct audio_hw_device *dev,
                                    struct audio_stream_in *stream)
{
    struct submix_audio_device *rsxadev = audio_hw_device_get_submix_audio_device(dev);
    ALOGV("adev_close_input_stream()");

    pthread_mutex_lock(&rsxadev->lock);

    MonoPipe* sink = rsxadev->rsxSink.get();
    if (sink != NULL) {
        ALOGI("shutdown");
        sink->shutdown(true);
    }

    free(stream);

    pthread_mutex_unlock(&rsxadev->lock);
}

static int adev_dump(const audio_hw_device_t *device, int fd)
{
    (void)device;
    (void)fd;
    return 0;
}

static int adev_close(hw_device_t *device)
{
    ALOGI("adev_close()");
    free(device);
    return 0;
}

static int adev_open(const hw_module_t* module, const char* name,
                     hw_device_t** device)
{
    ALOGI("adev_open(name=%s)", name);
    struct submix_audio_device *rsxadev;

    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0)
        return -EINVAL;

    rsxadev = (submix_audio_device*) calloc(1, sizeof(struct submix_audio_device));
    if (!rsxadev)
        return -ENOMEM;

    rsxadev->device.common.tag = HARDWARE_DEVICE_TAG;
    rsxadev->device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
    rsxadev->device.common.module = (struct hw_module_t *) module;
    rsxadev->device.common.close = adev_close;

    rsxadev->device.init_check = adev_init_check;
    rsxadev->device.set_voice_volume = adev_set_voice_volume;
    rsxadev->device.set_master_volume = adev_set_master_volume;
    rsxadev->device.get_master_volume = adev_get_master_volume;
    rsxadev->device.set_master_mute = adev_set_master_mute;
    rsxadev->device.get_master_mute = adev_get_master_mute;
    rsxadev->device.set_mode = adev_set_mode;
    rsxadev->device.set_mic_mute = adev_set_mic_mute;
    rsxadev->device.get_mic_mute = adev_get_mic_mute;
    rsxadev->device.set_parameters = adev_set_parameters;
    rsxadev->device.get_parameters = adev_get_parameters;
    rsxadev->device.get_input_buffer_size = adev_get_input_buffer_size;
    rsxadev->device.open_output_stream = adev_open_output_stream;
    rsxadev->device.close_output_stream = adev_close_output_stream;
    rsxadev->device.open_input_stream = adev_open_input_stream;
    rsxadev->device.close_input_stream = adev_close_input_stream;
    rsxadev->device.dump = adev_dump;

    rsxadev->input_standby = true;
    rsxadev->output_standby = true;

    *device = &rsxadev->device.common;

    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    /* open */ adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    /* common */ {
        /* tag */                HARDWARE_MODULE_TAG,
        /* module_api_version */ AUDIO_MODULE_API_VERSION_0_1,
        /* hal_api_version */    HARDWARE_HAL_API_VERSION,
        /* id */                 AUDIO_HARDWARE_MODULE_ID,
        /* name */               "Wifi Display audio HAL",
        /* author */             "The Android Open Source Project",
        /* methods */            &hal_module_methods,
        /* dso */                NULL,
        /* reserved */           { 0 },
    },
};

} //namespace android

} //extern "C"
