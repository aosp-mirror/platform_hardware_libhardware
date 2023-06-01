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

#define LOG_TAG "modules.usbaudio.audio_hal"
/* #define LOG_NDEBUG 0 */

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <log/log.h>
#include <cutils/list.h>
#include <cutils/str_parms.h>
#include <cutils/properties.h>

#include <hardware/audio.h>
#include <hardware/audio_alsaops.h>
#include <hardware/hardware.h>

#include <system/audio.h>

#include <tinyalsa/asoundlib.h>

#include <audio_utils/channels.h>

#include "alsa_device_profile.h"
#include "alsa_device_proxy.h"
#include "alsa_logging.h"

/* Lock play & record samples rates at or above this threshold */
#define RATELOCK_THRESHOLD 96000

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

struct audio_device {
    struct audio_hw_device hw_device;

    pthread_mutex_t lock; /* see note below on mutex acquisition order */

    /* output */
    struct listnode output_stream_list;

    /* input */
    struct listnode input_stream_list;

    /* lock input & output sample rates */
    /*FIXME - How do we address multiple output streams? */
    uint32_t device_sample_rate;    // this should be a rate that is common to both input & output

    bool mic_muted;

    int32_t inputs_open; /* number of input streams currently open. */

    audio_patch_handle_t next_patch_handle; // Increase 1 when create audio patch
};

struct stream_lock {
    pthread_mutex_t lock;               /* see note below on mutex acquisition order */
    pthread_mutex_t pre_lock;           /* acquire before lock to avoid DOS by playback thread */
};

struct alsa_device_info {
    alsa_device_profile profile;        /* The profile of the ALSA device */
    alsa_device_proxy proxy;            /* The state */
    struct listnode list_node;
};

struct stream_out {
    struct audio_stream_out stream;

    struct stream_lock lock;

    bool standby;

    struct audio_device *adev;           /* hardware information - only using this for the lock */

    struct listnode alsa_devices;       /* The ALSA devices connected to the stream. */

    unsigned hal_channel_count;         /* channel count exposed to AudioFlinger.
                                         * This may differ from the device channel count when
                                         * the device is not compatible with AudioFlinger
                                         * capabilities, e.g. exposes too many channels or
                                         * too few channels. */
    audio_channel_mask_t hal_channel_mask;  /* USB devices deal in channel counts, not masks
                                             * so the proxy doesn't have a channel_mask, but
                                             * audio HALs need to talk about channel masks
                                             * so expose the one calculated by
                                             * adev_open_output_stream */

    struct listnode list_node;

    void * conversion_buffer;           /* any conversions are put into here
                                         * they could come from here too if
                                         * there was a previous conversion */
    size_t conversion_buffer_size;      /* in bytes */

    struct pcm_config config;

    audio_io_handle_t handle; // Unique constant for a stream

    audio_patch_handle_t patch_handle; // Patch handle for this stream

    bool is_bit_perfect; // True if the stream is open with bit-perfect output flag

    // Mixer information used for volume handling
    struct mixer* mixer;
    struct mixer_ctl* volume_ctl;
    int volume_ctl_num_values;
    int max_volume_level;
    int min_volume_level;
};

struct stream_in {
    struct audio_stream_in stream;

    struct stream_lock  lock;

    bool standby;

    struct audio_device *adev;           /* hardware information - only using this for the lock */

    struct listnode alsa_devices;       /* The ALSA devices connected to the stream. */

    unsigned hal_channel_count;         /* channel count exposed to AudioFlinger.
                                         * This may differ from the device channel count when
                                         * the device is not compatible with AudioFlinger
                                         * capabilities, e.g. exposes too many channels or
                                         * too few channels. */
    audio_channel_mask_t hal_channel_mask;  /* USB devices deal in channel counts, not masks
                                             * so the proxy doesn't have a channel_mask, but
                                             * audio HALs need to talk about channel masks
                                             * so expose the one calculated by
                                             * adev_open_input_stream */

    struct listnode list_node;

    /* We may need to read more data from the device in order to data reduce to 16bit, 4chan */
    void * conversion_buffer;           /* any conversions are put into here
                                         * they could come from here too if
                                         * there was a previous conversion */
    size_t conversion_buffer_size;      /* in bytes */

    struct pcm_config config;

    audio_io_handle_t handle; // Unique identifier for a stream

    audio_patch_handle_t patch_handle; // Patch handle for this stream
};

// Map channel count to output channel mask
static const audio_channel_mask_t OUT_CHANNEL_MASKS_MAP[FCC_24 + 1] = {
    [0] = AUDIO_CHANNEL_NONE,  // == 0 (so this line is optional and could be omitted)
                               // != AUDIO_CHANNEL_INVALID == 0xC0000000u

    [1] = AUDIO_CHANNEL_OUT_MONO,
    [2] = AUDIO_CHANNEL_OUT_STEREO,
    [3] = AUDIO_CHANNEL_OUT_2POINT1,
    [4] = AUDIO_CHANNEL_OUT_QUAD,
    [5] = AUDIO_CHANNEL_OUT_PENTA,
    [6] = AUDIO_CHANNEL_OUT_5POINT1,
    [7] = AUDIO_CHANNEL_OUT_6POINT1,
    [8] = AUDIO_CHANNEL_OUT_7POINT1,

    [9 ... 11] = AUDIO_CHANNEL_NONE,  // == 0 (so this line is optional and could be omitted).

    [12] = AUDIO_CHANNEL_OUT_7POINT1POINT4,

    [13 ... 23] = AUDIO_CHANNEL_NONE,  //  == 0 (so this line is optional and could be omitted).

    [24] = AUDIO_CHANNEL_OUT_22POINT2,
};
static const int OUT_CHANNEL_MASKS_SIZE = AUDIO_ARRAY_SIZE(OUT_CHANNEL_MASKS_MAP);

// Map channel count to input channel mask
static const audio_channel_mask_t IN_CHANNEL_MASKS_MAP[] = {
    AUDIO_CHANNEL_NONE,       /* 0 */
    AUDIO_CHANNEL_IN_MONO,    /* 1 */
    AUDIO_CHANNEL_IN_STEREO,  /* 2 */
    /* channel counts greater than this are not considered */
};
static const int IN_CHANNEL_MASKS_SIZE = AUDIO_ARRAY_SIZE(IN_CHANNEL_MASKS_MAP);

// Map channel count to index mask
static const audio_channel_mask_t CHANNEL_INDEX_MASKS_MAP[FCC_24 + 1] = {
    [0] = AUDIO_CHANNEL_NONE,  // == 0 (so this line is optional and could be omitted).

    [1] = AUDIO_CHANNEL_INDEX_MASK_1,
    [2] = AUDIO_CHANNEL_INDEX_MASK_2,
    [3] = AUDIO_CHANNEL_INDEX_MASK_3,
    [4] = AUDIO_CHANNEL_INDEX_MASK_4,
    [5] = AUDIO_CHANNEL_INDEX_MASK_5,
    [6] = AUDIO_CHANNEL_INDEX_MASK_6,
    [7] = AUDIO_CHANNEL_INDEX_MASK_7,
    [8] = AUDIO_CHANNEL_INDEX_MASK_8,

    [9] = AUDIO_CHANNEL_INDEX_MASK_9,
    [10] = AUDIO_CHANNEL_INDEX_MASK_10,
    [11] = AUDIO_CHANNEL_INDEX_MASK_11,
    [12] = AUDIO_CHANNEL_INDEX_MASK_12,
    [13] = AUDIO_CHANNEL_INDEX_MASK_13,
    [14] = AUDIO_CHANNEL_INDEX_MASK_14,
    [15] = AUDIO_CHANNEL_INDEX_MASK_15,
    [16] = AUDIO_CHANNEL_INDEX_MASK_16,

    [17] = AUDIO_CHANNEL_INDEX_MASK_17,
    [18] = AUDIO_CHANNEL_INDEX_MASK_18,
    [19] = AUDIO_CHANNEL_INDEX_MASK_19,
    [20] = AUDIO_CHANNEL_INDEX_MASK_20,
    [21] = AUDIO_CHANNEL_INDEX_MASK_21,
    [22] = AUDIO_CHANNEL_INDEX_MASK_22,
    [23] = AUDIO_CHANNEL_INDEX_MASK_23,
    [24] = AUDIO_CHANNEL_INDEX_MASK_24,
};
static const int CHANNEL_INDEX_MASKS_SIZE = AUDIO_ARRAY_SIZE(CHANNEL_INDEX_MASKS_MAP);

static const char* ALL_VOLUME_CONTROL_NAMES[] = {
    "PCM Playback Volume",
    "Headset Playback Volume",
    "Headphone Playback Volume",
    "Master Playback Volume",
};
static const int VOLUME_CONTROL_NAMES_NUM = AUDIO_ARRAY_SIZE(ALL_VOLUME_CONTROL_NAMES);

/*
 * Locking Helpers
 */
/*
 * NOTE: when multiple mutexes have to be acquired, always take the
 * stream_in or stream_out mutex first, followed by the audio_device mutex.
 * stream pre_lock is always acquired before stream lock to prevent starvation of control thread by
 * higher priority playback or capture thread.
 */

static void stream_lock_init(struct stream_lock *lock) {
    pthread_mutex_init(&lock->lock, (const pthread_mutexattr_t *) NULL);
    pthread_mutex_init(&lock->pre_lock, (const pthread_mutexattr_t *) NULL);
}

static void stream_lock(struct stream_lock *lock) {
    if (lock == NULL) {
        return;
    }
    pthread_mutex_lock(&lock->pre_lock);
    pthread_mutex_lock(&lock->lock);
    pthread_mutex_unlock(&lock->pre_lock);
}

static void stream_unlock(struct stream_lock *lock) {
    pthread_mutex_unlock(&lock->lock);
}

static void device_lock(struct audio_device *adev) {
    pthread_mutex_lock(&adev->lock);
}

static int device_try_lock(struct audio_device *adev) {
    return pthread_mutex_trylock(&adev->lock);
}

static void device_unlock(struct audio_device *adev) {
    pthread_mutex_unlock(&adev->lock);
}

/*
 * streams list management
 */
static void adev_add_stream_to_list(
    struct audio_device* adev, struct listnode* list, struct listnode* stream_node) {
    device_lock(adev);

    list_add_tail(list, stream_node);

    device_unlock(adev);
}

static struct stream_out* adev_get_stream_out_by_io_handle_l(
        struct audio_device* adev, audio_io_handle_t handle) {
    struct listnode *node;
    list_for_each (node, &adev->output_stream_list) {
        struct stream_out *out = node_to_item(node, struct stream_out, list_node);
        if (out->handle == handle) {
            return out;
        }
    }
    return NULL;
}

static struct stream_in* adev_get_stream_in_by_io_handle_l(
        struct audio_device* adev, audio_io_handle_t handle) {
    struct listnode *node;
    list_for_each (node, &adev->input_stream_list) {
        struct stream_in *in = node_to_item(node, struct stream_in, list_node);
        if (in->handle == handle) {
            return in;
        }
    }
    return NULL;
}

static struct stream_out* adev_get_stream_out_by_patch_handle_l(
        struct audio_device* adev, audio_patch_handle_t patch_handle) {
    struct listnode *node;
    list_for_each (node, &adev->output_stream_list) {
        struct stream_out *out = node_to_item(node, struct stream_out, list_node);
        if (out->patch_handle == patch_handle) {
            return out;
        }
    }
    return NULL;
}

static struct stream_in* adev_get_stream_in_by_patch_handle_l(
        struct audio_device* adev, audio_patch_handle_t patch_handle) {
    struct listnode *node;
    list_for_each (node, &adev->input_stream_list) {
        struct stream_in *in = node_to_item(node, struct stream_in, list_node);
        if (in->patch_handle == patch_handle) {
            return in;
        }
    }
    return NULL;
}

/*
 * Extract the card and device numbers from the supplied key/value pairs.
 *   kvpairs    A null-terminated string containing the key/value pairs or card and device.
 *              i.e. "card=1;device=42"
 *   card   A pointer to a variable to receive the parsed-out card number.
 *   device A pointer to a variable to receive the parsed-out device number.
 * NOTE: The variables pointed to by card and device return -1 (undefined) if the
 *  associated key/value pair is not found in the provided string.
 *  Return true if the kvpairs string contain a card/device spec, false otherwise.
 */
static bool parse_card_device_params(const char *kvpairs, int *card, int *device)
{
    struct str_parms * parms = str_parms_create_str(kvpairs);
    char value[32];
    int param_val;

    // initialize to "undefined" state.
    *card = -1;
    *device = -1;

    param_val = str_parms_get_str(parms, "card", value, sizeof(value));
    if (param_val >= 0) {
        *card = atoi(value);
    }

    param_val = str_parms_get_str(parms, "device", value, sizeof(value));
    if (param_val >= 0) {
        *device = atoi(value);
    }

    str_parms_destroy(parms);

    return *card >= 0 && *device >= 0;
}

static char *device_get_parameters(const alsa_device_profile *profile, const char * keys)
{
    if (profile->card < 0 || profile->device < 0) {
        return strdup("");
    }

    struct str_parms *query = str_parms_create_str(keys);
    struct str_parms *result = str_parms_create();

    /* These keys are from hardware/libhardware/include/audio.h */
    /* supported sample rates */
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
        char* rates_list = profile_get_sample_rate_strs(profile);
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES,
                          rates_list);
        free(rates_list);
    }

    /* supported channel counts */
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)) {
        char* channels_list = profile_get_channel_count_strs(profile);
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_CHANNELS,
                          channels_list);
        free(channels_list);
    }

    /* supported sample formats */
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
        char * format_params = profile_get_format_strs(profile);
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_FORMATS,
                          format_params);
        free(format_params);
    }
    str_parms_destroy(query);

    char* result_str = str_parms_to_str(result);
    str_parms_destroy(result);

    ALOGV("device_get_parameters = %s", result_str);

    return result_str;
}

static audio_format_t audio_format_from(enum pcm_format format)
{
    switch (format) {
    case PCM_FORMAT_S16_LE:
        return AUDIO_FORMAT_PCM_16_BIT;
    case PCM_FORMAT_S32_LE:
        return AUDIO_FORMAT_PCM_32_BIT;
    case PCM_FORMAT_S8:
        return AUDIO_FORMAT_PCM_8_BIT;
    case PCM_FORMAT_S24_LE:
        return AUDIO_FORMAT_PCM_8_24_BIT;
    case PCM_FORMAT_S24_3LE:
        return AUDIO_FORMAT_PCM_24_BIT_PACKED;
    default:
        return AUDIO_FORMAT_INVALID;
    }
}

static unsigned int populate_channel_mask_from_profile(const alsa_device_profile* profile,
                                                       bool is_output,
                                                       audio_channel_mask_t channel_masks[])
{
    unsigned int num_channel_masks = 0;
    const audio_channel_mask_t* channel_masks_map =
            is_output ? OUT_CHANNEL_MASKS_MAP : IN_CHANNEL_MASKS_MAP;
    int channel_masks_size = is_output ? OUT_CHANNEL_MASKS_SIZE : IN_CHANNEL_MASKS_SIZE;
    if (channel_masks_size > FCC_LIMIT + 1) {
        channel_masks_size = FCC_LIMIT + 1;
    }
    unsigned int channel_count = 0;
    for (size_t i = 0; i < min(channel_masks_size, AUDIO_PORT_MAX_CHANNEL_MASKS) &&
            (channel_count = profile->channel_counts[i]) != 0 &&
            num_channel_masks < AUDIO_PORT_MAX_CHANNEL_MASKS; ++i) {
        if (channel_count < channel_masks_size &&
            channel_masks_map[channel_count] != AUDIO_CHANNEL_NONE) {
            channel_masks[num_channel_masks++] = channel_masks_map[channel_count];
            if (num_channel_masks >= AUDIO_PORT_MAX_CHANNEL_MASKS) {
                break;
            }
        }
        if (channel_count < CHANNEL_INDEX_MASKS_SIZE &&
            CHANNEL_INDEX_MASKS_MAP[channel_count] != AUDIO_CHANNEL_NONE) {
            channel_masks[num_channel_masks++] = CHANNEL_INDEX_MASKS_MAP[channel_count];
        }
    }
    return num_channel_masks;
}

static unsigned int populate_sample_rates_from_profile(const alsa_device_profile* profile,
                                                       unsigned int sample_rates[])
{
    unsigned int num_sample_rates = 0;
    for (;num_sample_rates < min(MAX_PROFILE_SAMPLE_RATES, AUDIO_PORT_MAX_SAMPLING_RATES) &&
            profile->sample_rates[num_sample_rates] != 0; num_sample_rates++) {
        sample_rates[num_sample_rates] = profile->sample_rates[num_sample_rates];
    }
    return num_sample_rates;
}

static bool are_all_devices_found(unsigned int num_devices_to_find,
                                  const int cards_to_find[],
                                  const int devices_to_find[],
                                  unsigned int num_devices,
                                  const int cards[],
                                  const int devices[]) {
    for (unsigned int i = 0; i < num_devices_to_find; ++i) {
        unsigned int j = 0;
        for (; j < num_devices; ++j) {
            if (cards_to_find[i] == cards[j] && devices_to_find[i] == devices[j]) {
                break;
            }
        }
        if (j >= num_devices) {
            return false;
        }
    }
    return true;
}

static bool are_devices_the_same(unsigned int left_num_devices,
                                 const int left_cards[],
                                 const int left_devices[],
                                 unsigned int right_num_devices,
                                 const int right_cards[],
                                 const int right_devices[]) {
    if (left_num_devices != right_num_devices) {
        return false;
    }
    return are_all_devices_found(left_num_devices, left_cards, left_devices,
                                 right_num_devices, right_cards, right_devices) &&
           are_all_devices_found(right_num_devices, right_cards, right_devices,
                                 left_num_devices, left_cards, left_devices);
}

static void out_stream_find_mixer_volume_control(struct stream_out* out, int card) {
    out->mixer = mixer_open(card);
    if (out->mixer == NULL) {
        ALOGI("%s, no mixer found for card=%d", __func__, card);
        return;
    }
    unsigned int num_ctls = mixer_get_num_ctls(out->mixer);
    for (int i = 0; i < VOLUME_CONTROL_NAMES_NUM; ++i) {
        for (unsigned int j = 0; j < num_ctls; ++j) {
            struct mixer_ctl *ctl = mixer_get_ctl(out->mixer, j);
            enum mixer_ctl_type ctl_type = mixer_ctl_get_type(ctl);
            if (strcasestr(mixer_ctl_get_name(ctl), ALL_VOLUME_CONTROL_NAMES[i]) == NULL ||
                ctl_type != MIXER_CTL_TYPE_INT) {
                continue;
            }
            ALOGD("%s, mixer volume control(%s) found", __func__, ALL_VOLUME_CONTROL_NAMES[i]);
            out->volume_ctl_num_values = mixer_ctl_get_num_values(ctl);
            if (out->volume_ctl_num_values <= 0) {
                ALOGE("%s the num(%d) of volume ctl values is wrong",
                        __func__, out->volume_ctl_num_values);
                out->volume_ctl_num_values = 0;
                continue;
            }
            out->max_volume_level = mixer_ctl_get_range_max(ctl);
            out->min_volume_level = mixer_ctl_get_range_min(ctl);
            if (out->max_volume_level < out->min_volume_level) {
                ALOGE("%s the max volume level(%d) is less than min volume level(%d)",
                        __func__, out->max_volume_level, out->min_volume_level);
                out->max_volume_level = 0;
                out->min_volume_level = 0;
                continue;
            }
            out->volume_ctl = ctl;
            return;
        }
    }
    ALOGI("%s, no volume control found", __func__);
}

/*
 * HAl Functions
 */
/**
 * NOTE: when multiple mutexes have to be acquired, always respect the
 * following order: hw device > out stream
 */

static struct alsa_device_info* stream_get_first_alsa_device(const struct listnode *alsa_devices) {
    if (list_empty(alsa_devices)) {
        return NULL;
    }
    return node_to_item(list_head(alsa_devices), struct alsa_device_info, list_node);
}

/**
 * Must be called with holding the stream's lock.
 */
static void stream_standby_l(struct listnode *alsa_devices, bool *standby)
{
    if (!*standby) {
        struct listnode *node;
        list_for_each (node, alsa_devices) {
            struct alsa_device_info *device_info =
                    node_to_item(node, struct alsa_device_info, list_node);
            proxy_close(&device_info->proxy);
        }
        *standby = true;
    }
}

static void stream_clear_devices(struct listnode *alsa_devices)
{
    struct listnode *node, *temp;
    struct alsa_device_info *device_info = NULL;
    list_for_each_safe (node, temp, alsa_devices) {
        device_info = node_to_item(node, struct alsa_device_info, list_node);
        if (device_info != NULL) {
            list_remove(&device_info->list_node);
            free(device_info);
        }
    }
}

static int stream_set_new_devices(struct pcm_config *config,
                                  struct listnode *alsa_devices,
                                  unsigned int num_devices,
                                  const int cards[],
                                  const int devices[],
                                  int direction,
                                  bool is_bit_perfect)
{
    int status = 0;
    stream_clear_devices(alsa_devices);

    for (unsigned int i = 0; i < num_devices; ++i) {
        struct alsa_device_info *device_info =
                (struct alsa_device_info *) calloc(1, sizeof(struct alsa_device_info));
        profile_init(&device_info->profile, direction);
        device_info->profile.card = cards[i];
        device_info->profile.device = devices[i];
        status = profile_read_device_info(&device_info->profile) ? 0 : -EINVAL;
        if (status != 0) {
            ALOGE("%s failed to read device info card=%d;device=%d",
                    __func__, cards[i], devices[i]);
            goto exit;
        }
        status = proxy_prepare(&device_info->proxy, &device_info->profile, config, is_bit_perfect);
        if (status != 0) {
            ALOGE("%s failed to prepare device card=%d;device=%d",
                    __func__, cards[i], devices[i]);
            goto exit;
        }
        list_add_tail(alsa_devices, &device_info->list_node);
    }

exit:
    if (status != 0) {
        stream_clear_devices(alsa_devices);
    }
    return status;
}

static void stream_dump_alsa_devices(const struct listnode *alsa_devices, int fd) {
    struct listnode *node;
    size_t i = 0;
    list_for_each(node, alsa_devices) {
        struct alsa_device_info *device_info =
                node_to_item(node, struct alsa_device_info, list_node);
        const char* direction = device_info->profile.direction == PCM_OUT ? "Output" : "Input";
        dprintf(fd, "%s Profile %zu:\n", direction, i);
        profile_dump(&device_info->profile, fd);

        dprintf(fd, "%s Proxy %zu:\n", direction, i);
        proxy_dump(&device_info->proxy, fd);
    }
}

/*
 * OUT functions
 */
static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    struct alsa_device_info *device_info = stream_get_first_alsa_device(
            &((struct stream_out*)stream)->alsa_devices);
    if (device_info == NULL) {
        ALOGW("%s device info is null", __func__);
        return 0;
    }
    uint32_t rate = proxy_get_sample_rate(&device_info->proxy);
    ALOGV("out_get_sample_rate() = %d", rate);
    return rate;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    return 0;
}

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    const struct stream_out* out = (const struct stream_out*)stream;
    const struct alsa_device_info* device_info = stream_get_first_alsa_device(&out->alsa_devices);
    if (device_info == NULL) {
        ALOGW("%s device info is null", __func__);
        return 0;
    }
    return proxy_get_period_size(&device_info->proxy) * audio_stream_out_frame_size(&(out->stream));
}

static uint32_t out_get_channels(const struct audio_stream *stream)
{
    const struct stream_out *out = (const struct stream_out*)stream;
    return out->hal_channel_mask;
}

static audio_format_t out_get_format(const struct audio_stream *stream)
{
    /* Note: The HAL doesn't do any FORMAT conversion at this time. It
     * Relies on the framework to provide data in the specified format.
     * This could change in the future.
     */
    struct alsa_device_info *device_info = stream_get_first_alsa_device(
            &((struct stream_out*)stream)->alsa_devices);
    if (device_info == NULL) {
        ALOGW("%s device info is null", __func__);
        return AUDIO_FORMAT_DEFAULT;
    }
    audio_format_t format = audio_format_from_pcm_format(proxy_get_format(&device_info->proxy));
    return format;
}

static int out_set_format(struct audio_stream *stream, audio_format_t format)
{
    return 0;
}

static int out_standby(struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    stream_lock(&out->lock);
    device_lock(out->adev);
    stream_standby_l(&out->alsa_devices, &out->standby);
    device_unlock(out->adev);
    stream_unlock(&out->lock);
    return 0;
}

static int out_dump(const struct audio_stream *stream, int fd) {
    const struct stream_out* out_stream = (const struct stream_out*) stream;

    if (out_stream != NULL) {
        stream_dump_alsa_devices(&out_stream->alsa_devices, fd);
    }

    return 0;
}

static int out_set_parameters(struct audio_stream *stream __unused, const char *kvpairs)
{
    ALOGV("out_set_parameters() keys:%s", kvpairs);

    // The set parameters here only matters when the routing devices are changed.
    // When the device version is not less than 3.0, the framework will use create
    // audio patch API instead of set parameters to chanage audio routing.
    return 0;
}

static char * out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    struct stream_out *out = (struct stream_out *)stream;
    stream_lock(&out->lock);
    struct alsa_device_info *device_info = stream_get_first_alsa_device(&out->alsa_devices);
    char *params_str = NULL;
    if (device_info != NULL) {
        params_str =  device_get_parameters(&device_info->profile, keys);
    }
    stream_unlock(&out->lock);
    return params_str;
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    struct alsa_device_info *device_info = stream_get_first_alsa_device(
            &((struct stream_out*)stream)->alsa_devices);
    if (device_info == NULL) {
        ALOGW("%s device info is null", __func__);
        return 0;
    }
    return proxy_get_latency(&device_info->proxy);
}

static int out_set_volume(struct audio_stream_out *stream, float left, float right)
{
    struct stream_out *out = (struct stream_out *)stream;
    int result = -ENOSYS;
    stream_lock(&out->lock);
    if (out->volume_ctl != NULL) {
        int left_volume =
            out->min_volume_level + ceil((out->max_volume_level - out->min_volume_level) * left);
        int right_volume =
            out->min_volume_level + ceil((out->max_volume_level - out->min_volume_level) * right);
        int volumes[out->volume_ctl_num_values];
        if (out->volume_ctl_num_values == 1) {
            volumes[0] = left_volume;
        } else {
            volumes[0] = left_volume;
            volumes[1] = right_volume;
            for (int i = 2; i < out->volume_ctl_num_values; ++i) {
                volumes[i] = left_volume;
            }
        }
        result = mixer_ctl_set_array(out->volume_ctl, volumes, out->volume_ctl_num_values);
        if (result != 0) {
            ALOGE("%s error=%d left=%f right=%f", __func__, result, left, right);
        }
    }
    stream_unlock(&out->lock);
    return result;
}

/* must be called with hw device and output stream mutexes locked */
static int start_output_stream(struct stream_out *out)
{
    int status = 0;
    struct listnode *node;
    list_for_each(node, &out->alsa_devices) {
        struct alsa_device_info *device_info =
                node_to_item(node, struct alsa_device_info, list_node);
        ALOGV("start_output_stream(card:%d device:%d)",
                device_info->profile.card, device_info->profile.device);
        status = proxy_open(&device_info->proxy);
        if (status != 0) {
            ALOGE("%s failed to open device(card: %d device: %d)",
                    __func__, device_info->profile.card, device_info->profile.device);
            goto exit;
        } else {
            out->standby = false;
        }
    }

exit:
    if (status != 0) {
        list_for_each(node, &out->alsa_devices) {
            struct alsa_device_info *device_info =
                    node_to_item(node, struct alsa_device_info, list_node);
            proxy_close(&device_info->proxy);
        }

    }
    return status;
}

static ssize_t out_write(struct audio_stream_out *stream, const void* buffer, size_t bytes)
{
    int ret;
    struct stream_out *out = (struct stream_out *)stream;

    stream_lock(&out->lock);
    if (out->standby) {
        ret = start_output_stream(out);
        if (ret != 0) {
            goto err;
        }
    }

    struct listnode* node;
    list_for_each(node, &out->alsa_devices) {
        struct alsa_device_info* device_info =
                node_to_item(node, struct alsa_device_info, list_node);
        alsa_device_proxy* proxy = &device_info->proxy;
        const void * write_buff = buffer;
        int num_write_buff_bytes = bytes;
        const int num_device_channels = proxy_get_channel_count(proxy); /* what we told alsa */
        const int num_req_channels = out->hal_channel_count; /* what we told AudioFlinger */
        if (num_device_channels != num_req_channels) {
            /* allocate buffer */
            const size_t required_conversion_buffer_size =
                     bytes * num_device_channels / num_req_channels;
            if (required_conversion_buffer_size > out->conversion_buffer_size) {
                out->conversion_buffer_size = required_conversion_buffer_size;
                out->conversion_buffer = realloc(out->conversion_buffer,
                                                 out->conversion_buffer_size);
            }
            /* convert data */
            const audio_format_t audio_format = out_get_format(&(out->stream.common));
            const unsigned sample_size_in_bytes = audio_bytes_per_sample(audio_format);
            num_write_buff_bytes =
                    adjust_channels(write_buff, num_req_channels,
                                    out->conversion_buffer, num_device_channels,
                                    sample_size_in_bytes, num_write_buff_bytes);
            write_buff = out->conversion_buffer;
        }

        if (write_buff != NULL && num_write_buff_bytes != 0) {
            proxy_write(proxy, write_buff, num_write_buff_bytes);
        }
    }

    stream_unlock(&out->lock);

    return bytes;

err:
    stream_unlock(&out->lock);
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

static int out_get_presentation_position(const struct audio_stream_out *stream,
                                         uint64_t *frames, struct timespec *timestamp)
{
    struct stream_out *out = (struct stream_out *)stream; // discard const qualifier
    stream_lock(&out->lock);

    const struct alsa_device_info* device_info = stream_get_first_alsa_device(&out->alsa_devices);
    const int ret = device_info == NULL ? -ENODEV :
            proxy_get_presentation_position(&device_info->proxy, frames, timestamp);
    stream_unlock(&out->lock);
    return ret;
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

static int adev_open_output_stream(struct audio_hw_device *hw_dev,
                                   audio_io_handle_t handle,
                                   audio_devices_t devicesSpec __unused,
                                   audio_output_flags_t flags,
                                   struct audio_config *config,
                                   struct audio_stream_out **stream_out,
                                   const char *address /*__unused*/)
{
    ALOGV("adev_open_output_stream() handle:0x%X, devicesSpec:0x%X, flags:0x%X, addr:%s",
          handle, devicesSpec, flags, address);

    const bool is_bit_perfect = ((flags & AUDIO_OUTPUT_FLAG_BIT_PERFECT) != AUDIO_OUTPUT_FLAG_NONE);
    if (is_bit_perfect && (config->format == AUDIO_FORMAT_DEFAULT ||
            config->sample_rate == 0 ||
            config->channel_mask == AUDIO_CHANNEL_NONE)) {
        ALOGE("%s request bit perfect playback, config(format=%#x, sample_rate=%u, "
              "channel_mask=%#x) must be specified", __func__, config->format,
              config->sample_rate, config->channel_mask);
        return -EINVAL;
    }

    struct stream_out *out;

    out = (struct stream_out *)calloc(1, sizeof(struct stream_out));
    if (out == NULL) {
        return -ENOMEM;
    }

    /* setup function pointers */
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
    out->stream.get_presentation_position = out_get_presentation_position;
    out->stream.get_next_write_timestamp = out_get_next_write_timestamp;

    out->handle = handle;

    stream_lock_init(&out->lock);

    out->adev = (struct audio_device *)hw_dev;

    list_init(&out->alsa_devices);
    struct alsa_device_info *device_info =
            (struct alsa_device_info *)calloc(1, sizeof(struct alsa_device_info));
    profile_init(&device_info->profile, PCM_OUT);

    // build this to hand to the alsa_device_proxy
    struct pcm_config proxy_config = {};

    /* Pull out the card/device pair */
    parse_card_device_params(address, &device_info->profile.card, &device_info->profile.device);

    profile_read_device_info(&device_info->profile);

    int ret = 0;

    /* Rate */
    if (config->sample_rate == 0) {
        proxy_config.rate = profile_get_default_sample_rate(&device_info->profile);
    } else if (profile_is_sample_rate_valid(&device_info->profile, config->sample_rate)) {
        proxy_config.rate = config->sample_rate;
    } else {
        ret = -EINVAL;
        if (is_bit_perfect) {
            ALOGE("%s requesting bit-perfect but the sample rate(%u) is not valid",
                    __func__, config->sample_rate);
            return ret;
        }
        proxy_config.rate = config->sample_rate =
                profile_get_default_sample_rate(&device_info->profile);
    }

    /* TODO: This is a problem if the input does not support this rate */
    device_lock(out->adev);
    out->adev->device_sample_rate = config->sample_rate;
    device_unlock(out->adev);

    /* Format */
    if (config->format == AUDIO_FORMAT_DEFAULT) {
        proxy_config.format = profile_get_default_format(&device_info->profile);
        config->format = audio_format_from_pcm_format(proxy_config.format);
    } else {
        enum pcm_format fmt = pcm_format_from_audio_format(config->format);
        if (profile_is_format_valid(&device_info->profile, fmt)) {
            proxy_config.format = fmt;
        } else {
            ret = -EINVAL;
            if (is_bit_perfect) {
                ALOGE("%s request bit-perfect but the format(%#x) is not valid",
                        __func__, config->format);
                return ret;
            }
            proxy_config.format = profile_get_default_format(&device_info->profile);
            config->format = audio_format_from_pcm_format(proxy_config.format);
        }
    }

    /* Channels */
    bool calc_mask = false;
    if (config->channel_mask == AUDIO_CHANNEL_NONE) {
        /* query case */
        out->hal_channel_count = profile_get_default_channel_count(&device_info->profile);
        calc_mask = true;
    } else {
        /* explicit case */
        out->hal_channel_count = audio_channel_count_from_out_mask(config->channel_mask);
    }

    /* The Framework is currently limited to no more than this number of channels */
    if (out->hal_channel_count > FCC_LIMIT) {
        out->hal_channel_count = FCC_LIMIT;
        calc_mask = true;
    }

    if (calc_mask) {
        /* need to calculate the mask from channel count either because this is the query case
         * or the specified mask isn't valid for this device, or is more than the FW can handle */
        config->channel_mask = out->hal_channel_count <= FCC_2
                /* position mask for mono and stereo*/
                ? audio_channel_out_mask_from_count(out->hal_channel_count)
                /* otherwise indexed */
                : audio_channel_mask_for_index_assignment_from_count(out->hal_channel_count);
    }

    out->hal_channel_mask = config->channel_mask;

    // Validate the "logical" channel count against support in the "actual" profile.
    // if they differ, choose the "actual" number of channels *closest* to the "logical".
    // and store THAT in proxy_config.channels
    proxy_config.channels =
            profile_get_closest_channel_count(&device_info->profile, out->hal_channel_count);
    if (is_bit_perfect && proxy_config.channels != out->hal_channel_count) {
        ALOGE("%s request bit-perfect, but channel mask(%#x) cannot find exact match",
                __func__, config->channel_mask);
        return -EINVAL;
    }

    ret = proxy_prepare(&device_info->proxy, &device_info->profile, &proxy_config, is_bit_perfect);
    if (is_bit_perfect && ret != 0) {
        ALOGE("%s failed to prepare proxy for bit-perfect playback, err=%d", __func__, ret);
        return ret;
    }
    out->config = proxy_config;

    list_add_tail(&out->alsa_devices, &device_info->list_node);

    if ((flags & AUDIO_OUTPUT_FLAG_BIT_PERFECT) != AUDIO_OUTPUT_FLAG_NONE) {
        out_stream_find_mixer_volume_control(out, device_info->profile.card);
    }

    /* TODO The retry mechanism isn't implemented in AudioPolicyManager/AudioFlinger
     * So clear any errors that may have occurred above.
     */
    ret = 0;

    out->conversion_buffer = NULL;
    out->conversion_buffer_size = 0;

    out->standby = true;

    /* Save the stream for adev_dump() */
    adev_add_stream_to_list(out->adev, &out->adev->output_stream_list, &out->list_node);

    *stream_out = &out->stream;

    return ret;
}

static void adev_close_output_stream(struct audio_hw_device *hw_dev,
                                     struct audio_stream_out *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    stream_lock(&out->lock);
    /* Close the pcm device */
    stream_standby_l(&out->alsa_devices, &out->standby);
    stream_clear_devices(&out->alsa_devices);

    free(out->conversion_buffer);

    out->conversion_buffer = NULL;
    out->conversion_buffer_size = 0;

    if (out->volume_ctl != NULL) {
        for (int i = 0; i < out->volume_ctl_num_values; ++i) {
            mixer_ctl_set_value(out->volume_ctl, i, out->max_volume_level);
        }
        out->volume_ctl = NULL;
    }
    if (out->mixer != NULL) {
        mixer_close(out->mixer);
        out->mixer = NULL;
    }

    device_lock(out->adev);
    list_remove(&out->list_node);
    out->adev->device_sample_rate = 0;
    device_unlock(out->adev);
    stream_unlock(&out->lock);

    free(stream);
}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *hw_dev,
                                         const struct audio_config *config)
{
    /* TODO This needs to be calculated based on format/channels/rate */
    return 320;
}

/*
 * IN functions
 */
static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
    struct alsa_device_info *device_info = stream_get_first_alsa_device(
            &((const struct stream_in *)stream)->alsa_devices);
    if (device_info == NULL) {
        ALOGW("%s device info is null", __func__);
        return 0;
    }
    uint32_t rate = proxy_get_sample_rate(&device_info->proxy);
    ALOGV("in_get_sample_rate() = %d", rate);
    return rate;
}

static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    ALOGV("in_set_sample_rate(%d) - NOPE", rate);
    return -ENOSYS;
}

static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    const struct stream_in * in = ((const struct stream_in*)stream);
    struct alsa_device_info *device_info = stream_get_first_alsa_device(&in->alsa_devices);
    if (device_info == NULL) {
        ALOGW("%s device info is null", __func__);
        return 0;
    }
    return proxy_get_period_size(&device_info->proxy) * audio_stream_in_frame_size(&(in->stream));
}

static uint32_t in_get_channels(const struct audio_stream *stream)
{
    const struct stream_in *in = (const struct stream_in*)stream;
    return in->hal_channel_mask;
}

static audio_format_t in_get_format(const struct audio_stream *stream)
{
    struct alsa_device_info *device_info = stream_get_first_alsa_device(
            &((const struct stream_in *)stream)->alsa_devices);
    if (device_info == NULL) {
        ALOGW("%s device info is null", __func__);
        return AUDIO_FORMAT_DEFAULT;
    }
     alsa_device_proxy *proxy = &device_info->proxy;
     audio_format_t format = audio_format_from_pcm_format(proxy_get_format(proxy));
     return format;
}

static int in_set_format(struct audio_stream *stream, audio_format_t format)
{
    ALOGV("in_set_format(%d) - NOPE", format);

    return -ENOSYS;
}

static int in_standby(struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

    stream_lock(&in->lock);
    device_lock(in->adev);
    stream_standby_l(&in->alsa_devices, &in->standby);
    device_unlock(in->adev);
    stream_unlock(&in->lock);
    return 0;
}

static int in_dump(const struct audio_stream *stream, int fd)
{
  const struct stream_in* in_stream = (const struct stream_in*)stream;
  if (in_stream != NULL) {
      stream_dump_alsa_devices(&in_stream->alsa_devices, fd);
  }

  return 0;
}

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    ALOGV("in_set_parameters() keys:%s", kvpairs);

    // The set parameters here only matters when the routing devices are changed.
    // When the device version higher than 3.0, the framework will use create_audio_patch
    // API instead of set_parameters to change audio routing.
    return 0;
}

static char * in_get_parameters(const struct audio_stream *stream, const char *keys)
{
    struct stream_in *in = (struct stream_in *)stream;

    stream_lock(&in->lock);
    struct alsa_device_info *device_info = stream_get_first_alsa_device(&in->alsa_devices);
    char *params_str = NULL;
    if (device_info != NULL) {
        params_str =  device_get_parameters(&device_info->profile, keys);
    }
    stream_unlock(&in->lock);

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

/* must be called with hw device and input stream mutexes locked */
static int start_input_stream(struct stream_in *in)
{
    // Only care about the first device as only one input device is allowed.
    struct alsa_device_info *device_info = stream_get_first_alsa_device(&in->alsa_devices);
    if (device_info == NULL) {
        return -ENODEV;
    }

    ALOGV("start_input_stream(card:%d device:%d)",
            device_info->profile.card, device_info->profile.device);
    int ret = proxy_open(&device_info->proxy);
    if (ret == 0) {
        in->standby = false;
    }
    return ret;
}

/* TODO mutex stuff here (see out_write) */
static ssize_t in_read(struct audio_stream_in *stream, void* buffer, size_t bytes)
{
    size_t num_read_buff_bytes = 0;
    void * read_buff = buffer;
    void * out_buff = buffer;
    int ret = 0;

    struct stream_in * in = (struct stream_in *)stream;

    stream_lock(&in->lock);
    if (in->standby) {
        ret = start_input_stream(in);
        if (ret != 0) {
            goto err;
        }
    }

    // Only care about the first device as only one input device is allowed.
    struct alsa_device_info *device_info = stream_get_first_alsa_device(&in->alsa_devices);
    if (device_info == NULL) {
        return 0;
    }

    /*
     * OK, we need to figure out how much data to read to be able to output the requested
     * number of bytes in the HAL format (16-bit, stereo).
     */
    num_read_buff_bytes = bytes;
    int num_device_channels = proxy_get_channel_count(&device_info->proxy); /* what we told Alsa */
    int num_req_channels = in->hal_channel_count; /* what we told AudioFlinger */

    if (num_device_channels != num_req_channels) {
        num_read_buff_bytes = (num_device_channels * num_read_buff_bytes) / num_req_channels;
    }

    /* Setup/Realloc the conversion buffer (if necessary). */
    if (num_read_buff_bytes != bytes) {
        if (num_read_buff_bytes > in->conversion_buffer_size) {
            /*TODO Remove this when AudioPolicyManger/AudioFlinger support arbitrary formats
              (and do these conversions themselves) */
            in->conversion_buffer_size = num_read_buff_bytes;
            in->conversion_buffer = realloc(in->conversion_buffer, in->conversion_buffer_size);
        }
        read_buff = in->conversion_buffer;
    }

    ret = proxy_read(&device_info->proxy, read_buff, num_read_buff_bytes);
    if (ret == 0) {
        if (num_device_channels != num_req_channels) {
            // ALOGV("chans dev:%d req:%d", num_device_channels, num_req_channels);

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

        /* no need to acquire in->adev->lock to read mic_muted here as we don't change its state */
        if (num_read_buff_bytes > 0 && in->adev->mic_muted)
            memset(buffer, 0, num_read_buff_bytes);
    } else {
        num_read_buff_bytes = 0; // reset the value after USB headset is unplugged
    }

err:
    stream_unlock(&in->lock);
    return num_read_buff_bytes;
}

static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream)
{
    return 0;
}

static int in_get_capture_position(const struct audio_stream_in *stream,
                                   int64_t *frames, int64_t *time)
{
    struct stream_in *in = (struct stream_in *)stream; // discard const qualifier
    stream_lock(&in->lock);

    struct alsa_device_info *device_info = stream_get_first_alsa_device(&in->alsa_devices);

    const int ret = device_info == NULL ? -ENODEV
            : proxy_get_capture_position(&device_info->proxy, frames, time);

    stream_unlock(&in->lock);
    return ret;
}

static int in_get_active_microphones(const struct audio_stream_in *stream,
                                     struct audio_microphone_characteristic_t *mic_array,
                                     size_t *mic_count) {
    (void)stream;
    (void)mic_array;
    (void)mic_count;

    return -ENOSYS;
}

static int in_set_microphone_direction(const struct audio_stream_in *stream,
                                           audio_microphone_direction_t dir) {
    (void)stream;
    (void)dir;
    ALOGV("---- in_set_microphone_direction()");
    return -ENOSYS;
}

static int in_set_microphone_field_dimension(const struct audio_stream_in *stream, float zoom) {
    (void)zoom;
    ALOGV("---- in_set_microphone_field_dimension()");
    return -ENOSYS;
}

static int adev_open_input_stream(struct audio_hw_device *hw_dev,
                                  audio_io_handle_t handle,
                                  audio_devices_t devicesSpec __unused,
                                  struct audio_config *config,
                                  struct audio_stream_in **stream_in,
                                  audio_input_flags_t flags __unused,
                                  const char *address,
                                  audio_source_t source __unused)
{
    ALOGV("adev_open_input_stream() rate:%" PRIu32 ", chanMask:0x%" PRIX32 ", fmt:%" PRIu8,
          config->sample_rate, config->channel_mask, config->format);

    /* Pull out the card/device pair */
    int32_t card, device;
    if (!parse_card_device_params(address, &card, &device)) {
        ALOGW("%s fail - invalid address %s", __func__, address);
        *stream_in = NULL;
        return -EINVAL;
    }

    struct stream_in * const in = (struct stream_in *)calloc(1, sizeof(struct stream_in));
    if (in == NULL) {
        *stream_in = NULL;
        return -ENOMEM;
    }

    /* setup function pointers */
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
    in->stream.get_capture_position = in_get_capture_position;

    in->stream.get_active_microphones = in_get_active_microphones;
    in->stream.set_microphone_direction = in_set_microphone_direction;
    in->stream.set_microphone_field_dimension = in_set_microphone_field_dimension;

    in->handle = handle;

    stream_lock_init(&in->lock);

    in->adev = (struct audio_device *)hw_dev;

    list_init(&in->alsa_devices);
    struct alsa_device_info *device_info =
            (struct alsa_device_info *)calloc(1, sizeof(struct alsa_device_info));
    profile_init(&device_info->profile, PCM_IN);

    memset(&in->config, 0, sizeof(in->config));

    int ret = 0;
    device_lock(in->adev);
    int num_open_inputs = in->adev->inputs_open;
    device_unlock(in->adev);

    /* Check if an input stream is already open */
    if (num_open_inputs > 0) {
        if (!profile_is_cached_for(&device_info->profile, card, device)) {
            ALOGW("%s fail - address card:%d device:%d doesn't match existing profile",
                    __func__, card, device);
            ret = -EINVAL;
        }
    } else {
        /* Read input profile only if necessary */
        device_info->profile.card = card;
        device_info->profile.device = device;
        if (!profile_read_device_info(&device_info->profile)) {
            ALOGW("%s fail - cannot read profile", __func__);
            ret = -EINVAL;
        }
    }
    if (ret != 0) {
        free(in);
        *stream_in = NULL;
        return ret;
    }

    /* Rate */
    int request_config_rate = config->sample_rate;
    if (config->sample_rate == 0) {
        config->sample_rate = profile_get_default_sample_rate(&device_info->profile);
    }

    if (in->adev->device_sample_rate != 0 &&   /* we are playing, so lock the rate if possible */
        in->adev->device_sample_rate >= RATELOCK_THRESHOLD) {/* but only for high sample rates */
        if (config->sample_rate != in->adev->device_sample_rate) {
            unsigned highest_rate = profile_get_highest_sample_rate(&device_info->profile);
            if (highest_rate == 0) {
                ret = -EINVAL; /* error with device */
            } else {
                in->config.rate = config->sample_rate =
                        min(highest_rate, in->adev->device_sample_rate);
                if (request_config_rate != 0 && in->config.rate != config->sample_rate) {
                    /* Changing the requested rate */
                    ret = -EINVAL;
                } else {
                    /* Everything AOK! */
                    ret = 0;
                }
            }
        } else if (profile_is_sample_rate_valid(&device_info->profile, config->sample_rate)) {
            in->config.rate = config->sample_rate;
        }
    } else if (profile_is_sample_rate_valid(&device_info->profile, config->sample_rate)) {
        in->config.rate = config->sample_rate;
    } else {
        in->config.rate = config->sample_rate =
                profile_get_default_sample_rate(&device_info->profile);
        ret = -EINVAL;
    }

    /* Format */
    if (config->format == AUDIO_FORMAT_DEFAULT) {
        in->config.format = profile_get_default_format(&device_info->profile);
        config->format = audio_format_from_pcm_format(in->config.format);
    } else {
        enum pcm_format fmt = pcm_format_from_audio_format(config->format);
        if (profile_is_format_valid(&device_info->profile, fmt)) {
            in->config.format = fmt;
        } else {
            in->config.format = profile_get_default_format(&device_info->profile);
            config->format = audio_format_from_pcm_format(in->config.format);
            ret = -EINVAL;
        }
    }

    /* Channels */
    bool calc_mask = false;
    if (config->channel_mask == AUDIO_CHANNEL_NONE) {
        /* query case */
        in->hal_channel_count = profile_get_default_channel_count(&device_info->profile);
        calc_mask = true;
    } else {
        /* explicit case */
        in->hal_channel_count = audio_channel_count_from_in_mask(config->channel_mask);
    }

    /* The Framework is currently limited to no more than this number of channels */
    if (in->hal_channel_count > FCC_LIMIT) {
        in->hal_channel_count = FCC_LIMIT;
        calc_mask = true;
    }

    if (calc_mask) {
        /* need to calculate the mask from channel count either because this is the query case
         * or the specified mask isn't valid for this device, or is more than the FW can handle */
        in->hal_channel_mask = in->hal_channel_count <= FCC_2
            /* position mask for mono & stereo */
            ? audio_channel_in_mask_from_count(in->hal_channel_count)
            /* otherwise indexed */
            : audio_channel_mask_for_index_assignment_from_count(in->hal_channel_count);

        // if we change the mask...
        if (in->hal_channel_mask != config->channel_mask &&
            config->channel_mask != AUDIO_CHANNEL_NONE) {
            config->channel_mask = in->hal_channel_mask;
            ret = -EINVAL;
        }
    } else {
        in->hal_channel_mask = config->channel_mask;
    }

    if (ret == 0) {
        // Validate the "logical" channel count against support in the "actual" profile.
        // if they differ, choose the "actual" number of channels *closest* to the "logical".
        // and store THAT in proxy_config.channels
        in->config.channels =
                profile_get_closest_channel_count(&device_info->profile, in->hal_channel_count);
        ret = proxy_prepare(&device_info->proxy, &device_info->profile, &in->config,
                            false /*require_exact_match*/);
        if (ret == 0) {
            in->standby = true;

            in->conversion_buffer = NULL;
            in->conversion_buffer_size = 0;

            *stream_in = &in->stream;

            /* Save this for adev_dump() */
            adev_add_stream_to_list(in->adev, &in->adev->input_stream_list, &in->list_node);
        } else {
            ALOGW("proxy_prepare error %d", ret);
            unsigned channel_count = proxy_get_channel_count(&device_info->proxy);
            config->channel_mask = channel_count <= FCC_2
                ? audio_channel_in_mask_from_count(channel_count)
                : audio_channel_mask_for_index_assignment_from_count(channel_count);
            config->format = audio_format_from_pcm_format(proxy_get_format(&device_info->proxy));
            config->sample_rate = proxy_get_sample_rate(&device_info->proxy);
        }
    }

    if (ret != 0) {
        // Deallocate this stream on error, because AudioFlinger won't call
        // adev_close_input_stream() in this case.
        *stream_in = NULL;
        free(in);
        return ret;
    }

    list_add_tail(&in->alsa_devices, &device_info->list_node);

    device_lock(in->adev);
    ++in->adev->inputs_open;
    device_unlock(in->adev);

    return ret;
}

static void adev_close_input_stream(struct audio_hw_device *hw_dev,
                                    struct audio_stream_in *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

    stream_lock(&in->lock);
    device_lock(in->adev);
    list_remove(&in->list_node);
    --in->adev->inputs_open;
    struct alsa_device_info *device_info = stream_get_first_alsa_device(&in->alsa_devices);
    if (device_info != NULL) {
        ALOGV("adev_close_input_stream(c:%d d:%d)",
                device_info->profile.card, device_info->profile.device);
    }
    LOG_ALWAYS_FATAL_IF(in->adev->inputs_open < 0,
            "invalid inputs_open: %d", in->adev->inputs_open);

    stream_standby_l(&in->alsa_devices, &in->standby);

    device_unlock(in->adev);

    stream_clear_devices(&in->alsa_devices);
    stream_unlock(&in->lock);

    free(in->conversion_buffer);

    free(stream);
}

/*
 * ADEV Functions
 */
static int adev_set_parameters(struct audio_hw_device *hw_dev, const char *kvpairs)
{
    return 0;
}

static char * adev_get_parameters(const struct audio_hw_device *hw_dev, const char *keys)
{
    return strdup("");
}

static int adev_init_check(const struct audio_hw_device *hw_dev)
{
    return 0;
}

static int adev_set_voice_volume(struct audio_hw_device *hw_dev, float volume)
{
    return -ENOSYS;
}

static int adev_set_master_volume(struct audio_hw_device *hw_dev, float volume)
{
    return -ENOSYS;
}

static int adev_set_mode(struct audio_hw_device *hw_dev, audio_mode_t mode)
{
    return 0;
}

static int adev_set_mic_mute(struct audio_hw_device *hw_dev, bool state)
{
    struct audio_device * adev = (struct audio_device *)hw_dev;
    device_lock(adev);
    adev->mic_muted = state;
    device_unlock(adev);
    return -ENOSYS;
}

static int adev_get_mic_mute(const struct audio_hw_device *hw_dev, bool *state)
{
    return -ENOSYS;
}

static int adev_create_audio_patch(struct audio_hw_device *dev,
                                   unsigned int num_sources,
                                   const struct audio_port_config *sources,
                                   unsigned int num_sinks,
                                   const struct audio_port_config *sinks,
                                   audio_patch_handle_t *handle) {
    if (num_sources != 1 || num_sinks == 0 || num_sinks > AUDIO_PATCH_PORTS_MAX) {
        // Only accept mix->device and device->mix cases. In that case, the number of sources
        // must be 1. The number of sinks must be in the range of (0, AUDIO_PATCH_PORTS_MAX].
        return -EINVAL;
    }

    if (sources[0].type == AUDIO_PORT_TYPE_DEVICE) {
        // If source is a device, the number of sinks should be 1.
        if (num_sinks != 1 || sinks[0].type != AUDIO_PORT_TYPE_MIX) {
            return -EINVAL;
        }
    } else if (sources[0].type == AUDIO_PORT_TYPE_MIX) {
        // If source is a mix, all sinks should be device.
        for (unsigned int i = 0; i < num_sinks; i++) {
            if (sinks[i].type != AUDIO_PORT_TYPE_DEVICE) {
                ALOGE("%s() invalid sink type %#x for mix source", __func__, sinks[i].type);
                return -EINVAL;
            }
        }
    } else {
        // All other cases are invalid.
        return -EINVAL;
    }

    struct audio_device* adev = (struct audio_device*) dev;
    bool generatedPatchHandle = false;
    device_lock(adev);
    if (*handle == AUDIO_PATCH_HANDLE_NONE) {
        *handle = ++adev->next_patch_handle;
        generatedPatchHandle = true;
    }

    int cards[AUDIO_PATCH_PORTS_MAX];
    int devices[AUDIO_PATCH_PORTS_MAX];
    const struct audio_port_config *port_configs =
            sources[0].type == AUDIO_PORT_TYPE_DEVICE ? sources : sinks;
    int num_configs = 0;
    audio_io_handle_t io_handle = 0;
    bool wasStandby = true;
    int direction = PCM_OUT;
    audio_patch_handle_t *patch_handle = NULL;
    struct listnode *alsa_devices = NULL;
    struct stream_lock *lock = NULL;
    struct pcm_config *config = NULL;
    struct stream_in *in = NULL;
    struct stream_out *out = NULL;
    bool is_bit_perfect = false;

    unsigned int num_saved_devices = 0;
    int saved_cards[AUDIO_PATCH_PORTS_MAX];
    int saved_devices[AUDIO_PATCH_PORTS_MAX];

    struct listnode *node;

    // Only handle patches for mix->devices and device->mix case.
    if (sources[0].type == AUDIO_PORT_TYPE_DEVICE) {
        in = adev_get_stream_in_by_io_handle_l(adev, sinks[0].ext.mix.handle);
        if (in == NULL) {
            ALOGE("%s()can not find stream with handle(%d)", __func__, sinks[0].ext.mix.handle);
            device_unlock(adev);
            return -EINVAL;
        }

        direction = PCM_IN;
        wasStandby = in->standby;
        io_handle = in->handle;
        num_configs = num_sources;
        patch_handle = &in->patch_handle;
        alsa_devices = &in->alsa_devices;
        lock = &in->lock;
        config = &in->config;
    } else {
        out = adev_get_stream_out_by_io_handle_l(adev, sources[0].ext.mix.handle);
        if (out == NULL) {
            ALOGE("%s()can not find stream with handle(%d)", __func__, sources[0].ext.mix.handle);
            device_unlock(adev);
            return -EINVAL;
        }

        direction = PCM_OUT;
        wasStandby = out->standby;
        io_handle = out->handle;
        num_configs = num_sinks;
        patch_handle = &out->patch_handle;
        alsa_devices = &out->alsa_devices;
        lock = &out->lock;
        config = &out->config;
        is_bit_perfect = out->is_bit_perfect;
    }

    // Check if the patch handle match the recorded one if a valid patch handle is passed.
    if (!generatedPatchHandle && *patch_handle != *handle) {
        ALOGE("%s() the patch handle(%d) does not match recorded one(%d) for stream "
              "with handle(%d) when creating audio patch",
              __func__, *handle, *patch_handle, io_handle);
        device_unlock(adev);
        return -EINVAL;
    }
    device_unlock(adev);

    for (unsigned int i = 0; i < num_configs; ++i) {
        if (!parse_card_device_params(port_configs[i].ext.device.address, &cards[i], &devices[i])) {
            ALOGE("%s, failed to parse card and device %s",
                    __func__, port_configs[i].ext.device.address);
            return -EINVAL;
        }
    }

    stream_lock(lock);
    list_for_each (node, alsa_devices) {
        struct alsa_device_info *device_info =
                node_to_item(node, struct alsa_device_info, list_node);
        saved_cards[num_saved_devices] = device_info->profile.card;
        saved_devices[num_saved_devices++] = device_info->profile.device;
    }

    if (are_devices_the_same(
                num_configs, cards, devices, num_saved_devices, saved_cards, saved_devices)) {
        // The new devices are the same as original ones. No need to update.
        stream_unlock(lock);
        return 0;
    }

    device_lock(adev);
    stream_standby_l(alsa_devices, out == NULL ? &in->standby : &out->standby);
    device_unlock(adev);

    // Timestamps:
    // Audio timestamps assume continuous PCM frame counts which are maintained
    // with the device proxy.transferred variable.  Technically it would be better
    // associated with in or out stream, not the device; here we save and restore
    // using the first alsa device as a simplification.
    uint64_t saved_transferred_frames = 0;
    struct alsa_device_info *device_info = stream_get_first_alsa_device(alsa_devices);
    if (device_info != NULL) saved_transferred_frames = device_info->proxy.transferred;

    int ret = stream_set_new_devices(
            config, alsa_devices, num_configs, cards, devices, direction, is_bit_perfect);

    if (ret != 0) {
        *handle = generatedPatchHandle ? AUDIO_PATCH_HANDLE_NONE : *handle;
        stream_set_new_devices(
                config, alsa_devices, num_saved_devices, saved_cards, saved_devices, direction,
                is_bit_perfect);
    } else {
        *patch_handle = *handle;
    }

    // Timestamps: Restore transferred frames.
    if (saved_transferred_frames != 0) {
        device_info = stream_get_first_alsa_device(alsa_devices);
        if (device_info != NULL) device_info->proxy.transferred = saved_transferred_frames;
    }

    if (!wasStandby) {
        device_lock(adev);
        if (in != NULL) {
            ret = start_input_stream(in);
        }
        if (out != NULL) {
            ret = start_output_stream(out);
        }
        device_unlock(adev);
    }
    stream_unlock(lock);
    return ret;
}

static int adev_release_audio_patch(struct audio_hw_device *dev,
                                    audio_patch_handle_t patch_handle)
{
    struct audio_device* adev = (struct audio_device*) dev;

    device_lock(adev);
    struct stream_out *out = adev_get_stream_out_by_patch_handle_l(adev, patch_handle);
    device_unlock(adev);
    if (out != NULL) {
        stream_lock(&out->lock);
        device_lock(adev);
        stream_standby_l(&out->alsa_devices, &out->standby);
        device_unlock(adev);
        out->patch_handle = AUDIO_PATCH_HANDLE_NONE;
        stream_unlock(&out->lock);
        return 0;
    }

    device_lock(adev);
    struct stream_in *in = adev_get_stream_in_by_patch_handle_l(adev, patch_handle);
    device_unlock(adev);
    if (in != NULL) {
        stream_lock(&in->lock);
        device_lock(adev);
        stream_standby_l(&in->alsa_devices, &in->standby);
        device_unlock(adev);
        in->patch_handle = AUDIO_PATCH_HANDLE_NONE;
        stream_unlock(&in->lock);
        return 0;
    }

    ALOGE("%s cannot find stream with patch handle as %d", __func__, patch_handle);
    return -EINVAL;
}

static int adev_get_audio_port(struct audio_hw_device *dev, struct audio_port *port)
{
    if (port->type != AUDIO_PORT_TYPE_DEVICE) {
        return -EINVAL;
    }

    alsa_device_profile profile;
    const bool is_output = audio_is_output_device(port->ext.device.type);
    profile_init(&profile, is_output ? PCM_OUT : PCM_IN);
    if (!parse_card_device_params(port->ext.device.address, &profile.card, &profile.device)) {
        return -EINVAL;
    }

    if (!profile_read_device_info(&profile)) {
        return -ENOENT;
    }

    port->num_formats = 0;;
    for (size_t i = 0; i < min(MAX_PROFILE_FORMATS, AUDIO_PORT_MAX_FORMATS) &&
            profile.formats[i] != 0; ++i) {
        audio_format_t format = audio_format_from(profile.formats[i]);
        if (format != AUDIO_FORMAT_INVALID) {
            port->formats[port->num_formats++] = format;
        }
    }

    port->num_sample_rates = populate_sample_rates_from_profile(&profile, port->sample_rates);
    port->num_channel_masks = populate_channel_mask_from_profile(
            &profile, is_output, port->channel_masks);

    return 0;
}

static int adev_get_audio_port_v7(struct audio_hw_device *dev, struct audio_port_v7 *port)
{
    if (port->type != AUDIO_PORT_TYPE_DEVICE) {
        return -EINVAL;
    }

    alsa_device_profile profile;
    const bool is_output = audio_is_output_device(port->ext.device.type);
    profile_init(&profile, is_output ? PCM_OUT : PCM_IN);
    if (!parse_card_device_params(port->ext.device.address, &profile.card, &profile.device)) {
        return -EINVAL;
    }

    if (!profile_read_device_info(&profile)) {
        return -ENOENT;
    }

    audio_channel_mask_t channel_masks[AUDIO_PORT_MAX_CHANNEL_MASKS];
    unsigned int num_channel_masks = populate_channel_mask_from_profile(
            &profile, is_output, channel_masks);
    unsigned int sample_rates[AUDIO_PORT_MAX_SAMPLING_RATES];
    const unsigned int num_sample_rates =
            populate_sample_rates_from_profile(&profile, sample_rates);
    port->num_audio_profiles = 0;;
    for (size_t i = 0; i < min(MAX_PROFILE_FORMATS, AUDIO_PORT_MAX_AUDIO_PROFILES) &&
            profile.formats[i] != 0; ++i) {
        audio_format_t format = audio_format_from(profile.formats[i]);
        if (format == AUDIO_FORMAT_INVALID) {
            continue;
        }
        const unsigned int j = port->num_audio_profiles++;
        port->audio_profiles[j].format = format;
        port->audio_profiles[j].num_sample_rates = num_sample_rates;
        memcpy(port->audio_profiles[j].sample_rates,
               sample_rates,
               num_sample_rates * sizeof(unsigned int));
        port->audio_profiles[j].num_channel_masks = num_channel_masks;
        memcpy(port->audio_profiles[j].channel_masks,
               channel_masks,
               num_channel_masks* sizeof(audio_channel_mask_t));
    }

    return 0;
}

static int adev_dump(const struct audio_hw_device *device, int fd)
{
    dprintf(fd, "\nUSB audio module:\n");

    struct audio_device* adev = (struct audio_device*)device;
    const int kNumRetries = 3;
    const int kSleepTimeMS = 500;

    // use device_try_lock() in case we dumpsys during a deadlock
    int retry = kNumRetries;
    while (retry > 0 && device_try_lock(adev) != 0) {
      sleep(kSleepTimeMS);
      retry--;
    }

    if (retry > 0) {
        if (list_empty(&adev->output_stream_list)) {
            dprintf(fd, "  No output streams.\n");
        } else {
            struct listnode* node;
            list_for_each(node, &adev->output_stream_list) {
                struct audio_stream* stream =
                        (struct audio_stream *)node_to_item(node, struct stream_out, list_node);
                out_dump(stream, fd);
            }
        }

        if (list_empty(&adev->input_stream_list)) {
            dprintf(fd, "\n  No input streams.\n");
        } else {
            struct listnode* node;
            list_for_each(node, &adev->input_stream_list) {
                struct audio_stream* stream =
                        (struct audio_stream *)node_to_item(node, struct stream_in, list_node);
                in_dump(stream, fd);
            }
        }

        device_unlock(adev);
    } else {
        // Couldn't lock
        dprintf(fd, "  Could not obtain device lock.\n");
    }

    return 0;
}

static int adev_close(hw_device_t *device)
{
    free(device);

    return 0;
}

static int adev_open(const hw_module_t* module, const char* name, hw_device_t** device)
{
    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0)
        return -EINVAL;

    struct audio_device *adev = calloc(1, sizeof(struct audio_device));
    if (!adev)
        return -ENOMEM;

    pthread_mutex_init(&adev->lock, (const pthread_mutexattr_t *) NULL);

    list_init(&adev->output_stream_list);
    list_init(&adev->input_stream_list);

    adev->hw_device.common.tag = HARDWARE_DEVICE_TAG;
    adev->hw_device.common.version = AUDIO_DEVICE_API_VERSION_3_2;
    adev->hw_device.common.module = (struct hw_module_t *)module;
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
    adev->hw_device.create_audio_patch = adev_create_audio_patch;
    adev->hw_device.release_audio_patch = adev_release_audio_patch;
    adev->hw_device.get_audio_port = adev_get_audio_port;
    adev->hw_device.get_audio_port_v7 = adev_get_audio_port_v7;
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
