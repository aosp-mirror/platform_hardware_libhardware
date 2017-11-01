/*
 * Copyright (C) 2015 The Android Open Source Project
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

#define LOG_TAG "radio_hw_stub"
#define LOG_NDEBUG 0

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <cutils/list.h>
#include <log/log.h>

#include <hardware/hardware.h>
#include <hardware/radio.h>
#include <system/radio.h>
#include <system/radio_metadata.h>

static const radio_hal_properties_t hw_properties = {
    .class_id = RADIO_CLASS_AM_FM,
    .implementor = "The Android Open Source Project",
    .product = "Radio stub HAL",
    .version = "0.1",
    .serial = "0123456789",
    .num_tuners = 1,
    .num_audio_sources = 1,
    .supports_capture = false,
    .num_bands = 2,
    .bands = {
        {
            .type = RADIO_BAND_FM,
            .antenna_connected = true,
            .lower_limit = 87900,
            .upper_limit = 107900,
            .num_spacings = 1,
            .spacings = { 200 },
            .fm = {
                .deemphasis = RADIO_DEEMPHASIS_75,
                .stereo = true,
                .rds = RADIO_RDS_US,
                .ta = false,
                .af = false,
                .ea = true,
            }
        },
        {
            .type = RADIO_BAND_AM,
            .antenna_connected = true,
            .lower_limit = 540,
            .upper_limit = 1610,
            .num_spacings = 1,
            .spacings = { 10 },
            .am = {
                .stereo = true,
            }
        }
    }
};

static const radio_metadata_clock_t hw_clock = {
    .utc_seconds_since_epoch = 1234567890,
    .timezone_offset_in_minutes = (-8 * 60),
};

struct stub_radio_tuner {
    struct radio_tuner interface;
    struct stub_radio_device *dev;
    radio_callback_t callback;
    void *cookie;
    radio_hal_band_config_t config;
    radio_program_info_t program;
    bool audio;
    pthread_t callback_thread;
    pthread_mutex_t lock;
    pthread_cond_t  cond;
    struct listnode command_list;
};

struct stub_radio_device {
    struct radio_hw_device device;
    struct stub_radio_tuner *tuner;
    pthread_mutex_t lock;
};


typedef enum {
    CMD_EXIT,
    CMD_CONFIG,
    CMD_STEP,
    CMD_SCAN,
    CMD_TUNE,
    CMD_CANCEL,
    CMD_METADATA,
    CMD_ANNOUNCEMENTS,
    CMD_NUM
} thread_cmd_type_t;

uint32_t thread_cmd_delay_ms[CMD_NUM] = {
    [CMD_EXIT]          = 0,
    [CMD_CONFIG]        = 50,
    [CMD_STEP]          = 100,
    [CMD_SCAN]          = 200,
    [CMD_TUNE]          = 150,
    [CMD_CANCEL]        = 0,
    [CMD_METADATA]      = 1000,
    [CMD_ANNOUNCEMENTS] = 1000
};
struct thread_command {
    struct listnode node;
    thread_cmd_type_t type;
    struct timespec ts;
    union {
        unsigned int param;
        radio_hal_band_config_t config;
    };
};

/* must be called with out->lock locked */
static int send_command_l(struct stub_radio_tuner *tuner,
                          thread_cmd_type_t type,
                          unsigned int delay_ms,
                          void *param)
{
    struct thread_command *cmd = (struct thread_command *)calloc(1, sizeof(struct thread_command));
    struct timespec ts;

    if (cmd == NULL)
        return -ENOMEM;

    ALOGV("%s %d delay_ms %d", __func__, type, delay_ms);

    cmd->type = type;
    if (param != NULL) {
        if (cmd->type == CMD_CONFIG) {
            cmd->config = *(radio_hal_band_config_t *)param;
            ALOGV("%s CMD_CONFIG type %d", __func__, cmd->config.type);
        } else
            cmd->param = *(unsigned int *)param;
    }

    clock_gettime(CLOCK_REALTIME, &ts);

    ts.tv_sec  += delay_ms/1000;
    ts.tv_nsec += (delay_ms%1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_nsec -= 1000000000;
        ts.tv_sec  += 1;
    }
    cmd->ts = ts;
    list_add_tail(&tuner->command_list, &cmd->node);
    pthread_cond_signal(&tuner->cond);
    return 0;
}

#define BITMAP_FILE_PATH "/data/misc/audioserver/android.png"

static int add_bitmap_metadata(radio_metadata_t **metadata, radio_metadata_key_t key,
                               const char *source)
{
    int fd;
    ssize_t ret = 0;
    struct stat info;
    void *data = NULL;
    size_t size;

    fd = open(source, O_RDONLY);
    if (fd < 0)
        return -EPIPE;

    fstat(fd, &info);
    size = info.st_size;
    data = malloc(size);
    if (data == NULL) {
        ret = -ENOMEM;
        goto exit;
    }
    ret = read(fd, data, size);
    if (ret < 0)
        goto exit;
    ret = radio_metadata_add_raw(metadata, key, (const unsigned char *)data, size);

exit:
    close(fd);
    free(data);
    ALOGE_IF(ret != 0, "%s error %d", __func__, (int)ret);
    return (int)ret;
}

static int prepare_metadata(struct stub_radio_tuner *tuner,
                            radio_metadata_t **metadata, bool program)
{
    int ret = 0;
    char text[RADIO_STRING_LEN_MAX];
    struct timespec ts;

    if (metadata == NULL)
        return -EINVAL;

    if (*metadata != NULL)
        radio_metadata_deallocate(*metadata);

    *metadata = NULL;

    ret = radio_metadata_allocate(metadata, tuner->program.channel, 0);

    if (ret != 0)
        return ret;

    if (program) {
        ret = radio_metadata_add_int(metadata, RADIO_METADATA_KEY_RBDS_PTY, 5);
        if (ret != 0)
            goto exit;
        ret = radio_metadata_add_text(metadata, RADIO_METADATA_KEY_RDS_PS, "RockBand");
        if (ret != 0)
            goto exit;
        ret = add_bitmap_metadata(metadata, RADIO_METADATA_KEY_ICON, BITMAP_FILE_PATH);
        if (ret != 0 && ret != -EPIPE)
            goto exit;
        ret = radio_metadata_add_clock(metadata, RADIO_METADATA_KEY_CLOCK, &hw_clock);
        if (ret != 0)
            goto exit;
    } else {
        ret = add_bitmap_metadata(metadata, RADIO_METADATA_KEY_ART, BITMAP_FILE_PATH);
        if (ret != 0 && ret != -EPIPE)
            goto exit;
    }

    clock_gettime(CLOCK_REALTIME, &ts);
    snprintf(text, RADIO_STRING_LEN_MAX, "Artist %ld", ts.tv_sec % 10);
    ret = radio_metadata_add_text(metadata, RADIO_METADATA_KEY_ARTIST, text);
    if (ret != 0)
        goto exit;

    snprintf(text, RADIO_STRING_LEN_MAX, "Song %ld", ts.tv_nsec % 10);
    ret = radio_metadata_add_text(metadata, RADIO_METADATA_KEY_TITLE, text);
    if (ret != 0)
        goto exit;

    return 0;

exit:
    radio_metadata_deallocate(*metadata);
    *metadata = NULL;
    return ret;
}

static void *callback_thread_loop(void *context)
{
    struct stub_radio_tuner *tuner = (struct stub_radio_tuner *)context;
    struct timespec ts = {0, 0};

    ALOGI("%s", __func__);

    prctl(PR_SET_NAME, (unsigned long)"sound trigger callback", 0, 0, 0);

    pthread_mutex_lock(&tuner->lock);

    // Fields which are used to toggle the state of traffic announcements and
    // ea announcements at random. They are access protected by tuner->lock.
    bool ea_state = false;

    while (true) {
        struct thread_command *cmd = NULL;
        struct listnode *item;
        struct listnode *tmp;
        struct timespec cur_ts;
        bool got_cancel = false;
        bool send_meta_data = false;

        if (list_empty(&tuner->command_list) || ts.tv_sec != 0) {
            ALOGV("%s SLEEPING", __func__);
            if (ts.tv_sec != 0) {
                ALOGV("%s SLEEPING with timeout", __func__);
                pthread_cond_timedwait(&tuner->cond, &tuner->lock, &ts);
            } else {
                ALOGV("%s SLEEPING forever", __func__);
                pthread_cond_wait(&tuner->cond, &tuner->lock);
            }
            ts.tv_sec = 0;
            ALOGV("%s RUNNING", __func__);
        }

        clock_gettime(CLOCK_REALTIME, &cur_ts);

        list_for_each_safe(item, tmp, &tuner->command_list) {
            cmd = node_to_item(item, struct thread_command, node);

            if (got_cancel && (cmd->type == CMD_STEP || cmd->type == CMD_SCAN ||
                    cmd->type == CMD_TUNE || cmd->type == CMD_METADATA ||
                    cmd->type == CMD_ANNOUNCEMENTS)) {
                 list_remove(item);
                 free(cmd);
                 continue;
            }

            if ((cmd->ts.tv_sec < cur_ts.tv_sec) ||
                    ((cmd->ts.tv_sec == cur_ts.tv_sec) && (cmd->ts.tv_nsec < cur_ts.tv_nsec))) {
                radio_hal_event_t event;
                radio_metadata_t *metadata = NULL;

                event.type = RADIO_EVENT_HW_FAILURE;
                list_remove(item);

                ALOGV("%s processing command %d time %ld.%ld", __func__, cmd->type, cmd->ts.tv_sec,
                      cmd->ts.tv_nsec);

                switch (cmd->type) {
                default:
                case CMD_EXIT:
                    free(cmd);
                    goto exit;

                case CMD_CONFIG: {
                    tuner->config = cmd->config;
                    tuner->config.antenna_connected = true;
                    event.type = RADIO_EVENT_CONFIG;
                    event.config = tuner->config;
                    ALOGV("%s CMD_CONFIG type %d low %d up %d",
                          __func__, tuner->config.type,
                          tuner->config.lower_limit, tuner->config.upper_limit);
                    if (tuner->config.type == RADIO_BAND_FM) {
                        ALOGV("  - stereo %d\n  - rds %d\n  - ta %d\n  - af %d\n"
                              "  - ea %d\n",
                              tuner->config.fm.stereo, tuner->config.fm.rds,
                              tuner->config.fm.ta, tuner->config.fm.af,
                              tuner->config.fm.ea);
                    } else {
                        ALOGV("  - stereo %d", tuner->config.am.stereo);
                    }
                } break;

                case CMD_STEP: {
                    int frequency;
                    frequency = tuner->program.channel;
                    if (cmd->param == RADIO_DIRECTION_UP) {
                        frequency += tuner->config.spacings[0];
                    } else {
                        frequency -= tuner->config.spacings[0];
                    }
                    if (frequency > (int)tuner->config.upper_limit) {
                        frequency = tuner->config.lower_limit;
                    }
                    if (frequency < (int)tuner->config.lower_limit) {
                        frequency = tuner->config.upper_limit;
                    }
                    tuner->program.channel = frequency;
                    tuner->program.tuned  = (frequency / (tuner->config.spacings[0] * 5)) % 2;
                    tuner->program.signal_strength = 20;
                    if (tuner->config.type == RADIO_BAND_FM)
                        tuner->program.stereo = false;
                    else
                        tuner->program.stereo = false;
                    prepare_metadata(tuner, &tuner->program.metadata, tuner->program.tuned);

                    event.type = RADIO_EVENT_TUNED;
                    event.info = tuner->program;
                } break;

                case CMD_SCAN: {
                    int frequency;
                    frequency = tuner->program.channel;
                    if (cmd->param == RADIO_DIRECTION_UP) {
                        frequency += tuner->config.spacings[0] * 25;
                    } else {
                        frequency -= tuner->config.spacings[0] * 25;
                    }
                    if (frequency > (int)tuner->config.upper_limit) {
                        frequency = tuner->config.lower_limit;
                    }
                    if (frequency < (int)tuner->config.lower_limit) {
                        frequency = tuner->config.upper_limit;
                    }
                    tuner->program.channel = (unsigned int)frequency;
                    tuner->program.tuned  = true;
                    if (tuner->config.type == RADIO_BAND_FM)
                        tuner->program.stereo = tuner->config.fm.stereo;
                    else
                        tuner->program.stereo = tuner->config.am.stereo;
                    tuner->program.signal_strength = 50;
                    prepare_metadata(tuner, &tuner->program.metadata, tuner->program.tuned);

                    event.type = RADIO_EVENT_TUNED;
                    event.info = tuner->program;
                    send_meta_data = true;
                } break;

                case CMD_TUNE: {
                    tuner->program.channel = cmd->param;
                    tuner->program.tuned  = (tuner->program.channel /
                                                (tuner->config.spacings[0] * 5)) % 2;

                    if (tuner->program.tuned) {
                        send_command_l(tuner, CMD_ANNOUNCEMENTS, thread_cmd_delay_ms[CMD_ANNOUNCEMENTS], NULL);
                    }
                    tuner->program.signal_strength = 100;
                    if (tuner->config.type == RADIO_BAND_FM)
                        tuner->program.stereo =
                                tuner->program.tuned ? tuner->config.fm.stereo : false;
                    else
                        tuner->program.stereo =
                            tuner->program.tuned ? tuner->config.am.stereo : false;
                    prepare_metadata(tuner, &tuner->program.metadata, tuner->program.tuned);

                    event.type = RADIO_EVENT_TUNED;
                    event.info = tuner->program;
                    send_meta_data = true;
                } break;

                case CMD_METADATA: {
                    int ret = prepare_metadata(tuner, &metadata, false);
                    if (ret == 0) {
                        event.type = RADIO_EVENT_METADATA;
                        event.metadata = metadata;
                    }
                } break;

                case CMD_CANCEL: {
                    got_cancel = true;
                } break;

                // Fire emergency announcements if they are enabled in the config. Stub
                // implementation simply fires an announcement for 5 second
                // duration with a gap of 5 seconds.
                case CMD_ANNOUNCEMENTS: {
                    ALOGV("In annoucements. %d %d %d\n",
                          ea_state, tuner->config.type, tuner->config.fm.ea);
                    if (tuner->config.type == RADIO_BAND_FM ||
                        tuner->config.type == RADIO_BAND_FM_HD) {
                        if (ea_state) {
                            ea_state = false;
                            event.type = RADIO_EVENT_EA;
                        } else if (tuner->config.fm.ea) {
                            ea_state = true;
                            event.type = RADIO_EVENT_EA;
                        }
                        event.on = ea_state;

                        if (tuner->config.fm.ea) {
                            send_command_l(tuner, CMD_ANNOUNCEMENTS, 5000, NULL);
                        }
                    }
                } break;
                }
                if (event.type != RADIO_EVENT_HW_FAILURE && tuner->callback != NULL) {
                    pthread_mutex_unlock(&tuner->lock);
                    tuner->callback(&event, tuner->cookie);
                    pthread_mutex_lock(&tuner->lock);
                    if (event.type == RADIO_EVENT_METADATA && metadata != NULL) {
                        radio_metadata_deallocate(metadata);
                        metadata = NULL;
                    }
                }
                ALOGV("%s processed command %d", __func__, cmd->type);
                free(cmd);
            } else {
                if ((ts.tv_sec == 0) ||
                        (cmd->ts.tv_sec < ts.tv_sec) ||
                        ((cmd->ts.tv_sec == ts.tv_sec) && (cmd->ts.tv_nsec < ts.tv_nsec))) {
                    ts.tv_sec = cmd->ts.tv_sec;
                    ts.tv_nsec = cmd->ts.tv_nsec;
                }
            }
        }

        if (send_meta_data) {
            list_for_each_safe(item, tmp, &tuner->command_list) {
                cmd = node_to_item(item, struct thread_command, node);
                if (cmd->type == CMD_METADATA) {
                    list_remove(item);
                    free(cmd);
                }
            }
            send_command_l(tuner, CMD_METADATA, thread_cmd_delay_ms[CMD_METADATA], NULL);
        }
    }

exit:
    pthread_mutex_unlock(&tuner->lock);

    ALOGV("%s Exiting", __func__);

    return NULL;
}


static int tuner_set_configuration(const struct radio_tuner *tuner,
                         const radio_hal_band_config_t *config)
{
    struct stub_radio_tuner *stub_tuner = (struct stub_radio_tuner *)tuner;
    int status = 0;

    ALOGI("%s stub_tuner %p", __func__, stub_tuner);
    pthread_mutex_lock(&stub_tuner->lock);
    if (config == NULL) {
        status = -EINVAL;
        goto exit;
    }
    if (config->lower_limit > config->upper_limit) {
        status = -EINVAL;
        goto exit;
    }
    send_command_l(stub_tuner, CMD_CANCEL, thread_cmd_delay_ms[CMD_CANCEL], NULL);
    send_command_l(stub_tuner, CMD_CONFIG, thread_cmd_delay_ms[CMD_CONFIG], (void *)config);

exit:
    pthread_mutex_unlock(&stub_tuner->lock);
    return status;
}

static int tuner_get_configuration(const struct radio_tuner *tuner,
                                   radio_hal_band_config_t *config)
{
    struct stub_radio_tuner *stub_tuner = (struct stub_radio_tuner *)tuner;
    int status = 0;
    struct listnode *item;
    radio_hal_band_config_t *src_config;

    ALOGI("%s stub_tuner %p", __func__, stub_tuner);
    pthread_mutex_lock(&stub_tuner->lock);
    src_config = &stub_tuner->config;

    if (config == NULL) {
        status = -EINVAL;
        goto exit;
    }
    list_for_each(item, &stub_tuner->command_list) {
        struct thread_command *cmd = node_to_item(item, struct thread_command, node);
        if (cmd->type == CMD_CONFIG) {
            src_config = &cmd->config;
        }
    }
    *config = *src_config;

exit:
    pthread_mutex_unlock(&stub_tuner->lock);
    return status;
}

static int tuner_step(const struct radio_tuner *tuner,
                     radio_direction_t direction, bool skip_sub_channel)
{
    struct stub_radio_tuner *stub_tuner = (struct stub_radio_tuner *)tuner;

    ALOGI("%s stub_tuner %p direction %d, skip_sub_channel %d",
          __func__, stub_tuner, direction, skip_sub_channel);

    pthread_mutex_lock(&stub_tuner->lock);
    send_command_l(stub_tuner, CMD_STEP, thread_cmd_delay_ms[CMD_STEP], &direction);
    pthread_mutex_unlock(&stub_tuner->lock);
    return 0;
}

static int tuner_scan(const struct radio_tuner *tuner,
                     radio_direction_t direction, bool skip_sub_channel)
{
    struct stub_radio_tuner *stub_tuner = (struct stub_radio_tuner *)tuner;

    ALOGI("%s stub_tuner %p direction %d, skip_sub_channel %d",
          __func__, stub_tuner, direction, skip_sub_channel);

    pthread_mutex_lock(&stub_tuner->lock);
    send_command_l(stub_tuner, CMD_SCAN, thread_cmd_delay_ms[CMD_SCAN], &direction);
    pthread_mutex_unlock(&stub_tuner->lock);
    return 0;
}

static int tuner_tune(const struct radio_tuner *tuner,
                     unsigned int channel, unsigned int sub_channel)
{
    struct stub_radio_tuner *stub_tuner = (struct stub_radio_tuner *)tuner;

    ALOGI("%s stub_tuner %p channel %d, sub_channel %d",
          __func__, stub_tuner, channel, sub_channel);

    pthread_mutex_lock(&stub_tuner->lock);
    if (channel < stub_tuner->config.lower_limit || channel > stub_tuner->config.upper_limit) {
        pthread_mutex_unlock(&stub_tuner->lock);
        ALOGI("%s channel out of range", __func__);
        return -EINVAL;
    }
    send_command_l(stub_tuner, CMD_TUNE, thread_cmd_delay_ms[CMD_TUNE], &channel);
    pthread_mutex_unlock(&stub_tuner->lock);
    return 0;
}

static int tuner_cancel(const struct radio_tuner *tuner)
{
    struct stub_radio_tuner *stub_tuner = (struct stub_radio_tuner *)tuner;

    ALOGI("%s stub_tuner %p", __func__, stub_tuner);

    pthread_mutex_lock(&stub_tuner->lock);
    send_command_l(stub_tuner, CMD_CANCEL, thread_cmd_delay_ms[CMD_CANCEL], NULL);
    pthread_mutex_unlock(&stub_tuner->lock);
    return 0;
}

static int tuner_get_program_information(const struct radio_tuner *tuner,
                                        radio_program_info_t *info)
{
    struct stub_radio_tuner *stub_tuner = (struct stub_radio_tuner *)tuner;
    int status = 0;
    radio_metadata_t *metadata;

    ALOGI("%s stub_tuner %p", __func__, stub_tuner);
    pthread_mutex_lock(&stub_tuner->lock);
    if (info == NULL) {
        status = -EINVAL;
        goto exit;
    }
    metadata = info->metadata;
    *info = stub_tuner->program;
    info->metadata = metadata;
    if (metadata == NULL) {
        ALOGE("%s metadata is a nullptr", __func__);
        status = -EINVAL;
        goto exit;
    }
    if (stub_tuner->program.metadata != NULL)
        radio_metadata_add_metadata(&info->metadata, stub_tuner->program.metadata);

exit:
    pthread_mutex_unlock(&stub_tuner->lock);
    return status;
}

static int rdev_get_properties(const struct radio_hw_device *dev,
                                radio_hal_properties_t *properties)
{
    ALOGI("%s", __func__);
    if (properties == NULL)
        return -EINVAL;
    memcpy(properties, &hw_properties, sizeof(radio_hal_properties_t));
    return 0;
}

static int rdev_open_tuner(const struct radio_hw_device *dev,
                          const radio_hal_band_config_t *config,
                          bool audio,
                          radio_callback_t callback,
                          void *cookie,
                          const struct radio_tuner **tuner)
{
    struct stub_radio_device *rdev = (struct stub_radio_device *)dev;
    int status = 0;

    ALOGI("%s rdev %p", __func__, rdev);
    pthread_mutex_lock(&rdev->lock);

    if (rdev->tuner != NULL) {
        ALOGE("Can't open tuner twice");
        status = -ENOSYS;
        goto exit;
    }

    if (config == NULL || callback == NULL || tuner == NULL) {
        status = -EINVAL;
        goto exit;
    }

    rdev->tuner = (struct stub_radio_tuner *)calloc(1, sizeof(struct stub_radio_tuner));
    if (rdev->tuner == NULL) {
        status = -ENOMEM;
        goto exit;
    }

    rdev->tuner->interface.set_configuration = tuner_set_configuration;
    rdev->tuner->interface.get_configuration = tuner_get_configuration;
    rdev->tuner->interface.scan = tuner_scan;
    rdev->tuner->interface.step = tuner_step;
    rdev->tuner->interface.tune = tuner_tune;
    rdev->tuner->interface.cancel = tuner_cancel;
    rdev->tuner->interface.get_program_information = tuner_get_program_information;

    rdev->tuner->audio = audio;
    rdev->tuner->callback = callback;
    rdev->tuner->cookie = cookie;

    rdev->tuner->dev = rdev;

    pthread_mutex_init(&rdev->tuner->lock, (const pthread_mutexattr_t *) NULL);
    pthread_cond_init(&rdev->tuner->cond, (const pthread_condattr_t *) NULL);
    pthread_create(&rdev->tuner->callback_thread, (const pthread_attr_t *) NULL,
                        callback_thread_loop, rdev->tuner);
    list_init(&rdev->tuner->command_list);

    pthread_mutex_lock(&rdev->tuner->lock);
    send_command_l(rdev->tuner, CMD_CONFIG, thread_cmd_delay_ms[CMD_CONFIG], (void *)config);
    pthread_mutex_unlock(&rdev->tuner->lock);

    *tuner = &rdev->tuner->interface;

exit:
    pthread_mutex_unlock(&rdev->lock);
    ALOGI("%s DONE", __func__);
    return status;
}

static int rdev_close_tuner(const struct radio_hw_device *dev,
                            const struct radio_tuner *tuner)
{
    struct stub_radio_device *rdev = (struct stub_radio_device *)dev;
    struct stub_radio_tuner *stub_tuner = (struct stub_radio_tuner *)tuner;
    int status = 0;

    ALOGI("%s tuner %p", __func__, tuner);
    pthread_mutex_lock(&rdev->lock);

    if (tuner == NULL) {
        status = -EINVAL;
        goto exit;
    }

    pthread_mutex_lock(&stub_tuner->lock);
    stub_tuner->callback = NULL;
    send_command_l(stub_tuner, CMD_EXIT, thread_cmd_delay_ms[CMD_EXIT], NULL);
    pthread_mutex_unlock(&stub_tuner->lock);
    pthread_join(stub_tuner->callback_thread, (void **) NULL);

    if (stub_tuner->program.metadata != NULL)
        radio_metadata_deallocate(stub_tuner->program.metadata);

    free(stub_tuner);
    rdev->tuner = NULL;

exit:
    pthread_mutex_unlock(&rdev->lock);
    return status;
}

static int rdev_close(hw_device_t *device)
{
    struct stub_radio_device *rdev = (struct stub_radio_device *)device;
    if (rdev != NULL) {
        free(rdev->tuner);
    }
    free(rdev);
    return 0;
}

static int rdev_open(const hw_module_t* module, const char* name,
                     hw_device_t** device)
{
    struct stub_radio_device *rdev;

    if (strcmp(name, RADIO_HARDWARE_DEVICE) != 0)
        return -EINVAL;

    rdev = calloc(1, sizeof(struct stub_radio_device));
    if (!rdev)
        return -ENOMEM;

    rdev->device.common.tag = HARDWARE_DEVICE_TAG;
    rdev->device.common.version = RADIO_DEVICE_API_VERSION_1_0;
    rdev->device.common.module = (struct hw_module_t *) module;
    rdev->device.common.close = rdev_close;
    rdev->device.get_properties = rdev_get_properties;
    rdev->device.open_tuner = rdev_open_tuner;
    rdev->device.close_tuner = rdev_close_tuner;

    pthread_mutex_init(&rdev->lock, (const pthread_mutexattr_t *) NULL);

    *device = &rdev->device.common;

    return 0;
}


static struct hw_module_methods_t hal_module_methods = {
    .open = rdev_open,
};

struct radio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = RADIO_MODULE_API_VERSION_1_0,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = RADIO_HARDWARE_MODULE_ID,
        .name = "Stub radio HAL",
        .author = "The Android Open Source Project",
        .methods = &hal_module_methods,
    },
};
