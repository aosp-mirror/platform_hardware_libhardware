/*
 * Copyright (C) 2011 The Android Open Source Project
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


/* This HAL simulates triggers from the DSP.
 * To send a trigger from the command line you can type:
 *
 * adb forward tcp:14035 tcp:14035
 * echo $'\001' | nc -q -1 localhost 14035
 *
 * $'\001' corresponds to the index_position of loaded sound
 * model, you can also send $'\002' etc. $'\000' is the kill
 * signal for the trigger listening thread.
 *
 * To enable this file, you must change the src file in this
 * directory's Android.mk file, then change the sound_trigger
 * product package in the device-specific device.mk file,
 * for example, in device/htc/flounder
 */

#define LOG_TAG "sound_trigger_hw_default"
/*#define LOG_NDEBUG 0*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <errno.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <cutils/log.h>

#include <hardware/hardware.h>
#include <system/sound_trigger.h>
#include <hardware/sound_trigger.h>

/* Although max_sound_models is specified in sound_trigger_properties, having a maximum maximum
allows a dramatic simplification of data structures in this file */
#define MAX_MAX_SOUND_MODELS 5

static const struct sound_trigger_properties hw_properties = {
        "The Android Open Source Project", // implementor
        "Sound Trigger stub HAL", // description
        1, // version
        { 0xed7a7d60, 0xc65e, 0x11e3, 0x9be4, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
        2, // max_sound_models
        1, // max_key_phrases
        1, // max_users
        RECOGNITION_MODE_VOICE_TRIGGER, // recognition_modes
        false, // capture_transition
        0, // max_buffer_ms
        false, // concurrent_capture
        false, // trigger_in_event
        0 // power_consumption_mw
};

struct recognition_context {
      /* Sound Model Information Modified On Load */
    sound_model_handle_t loaded_sound_model;
    sound_model_callback_t sound_model_callback;
    void *sound_model_cookie;

    /* Sound Model Information Modified On Recognition Start */
    struct sound_trigger_recognition_config *config;
    recognition_callback_t recognition_callback;
    void *recognition_cookie;
}

struct stub_sound_trigger_device {
    struct sound_trigger_hw_device device;
    pthread_mutex_t lock;
    pthread_t callback_thread;

    struct recognition_context re_context[MAX_MAX_SOUND_MODELS];

    int next_sound_model_id;
};

/* Will reuse ids when overflow occurs */
static unsigned int generate_sound_model_id(const struct sound_trigger_hw_device *dev) {
    struct stub_sound_trigger_device *stdev = (struct stub_sound_trigger_device *)dev;
    int new_id = stdev->next_sound_model_id;
    ++stdev->next_sound_model_id;
    if (stdev->next_sound_model_id == 0) {
        stdev->next_sound_model_id = 1;
    }
    return new_id;
}

static char *sound_trigger_event_alloc(struct stub_sound_trigger_device *stdev,
                                       sound_model_handle_t handle) {
    struct sound_trigger_phrase_recognition_event *event;
    char *data;
    data = (char *)calloc(1, sizeof(struct sound_trigger_phrase_recognition_event));
    if (!data)
        return NULL;

    unsigned int model_index;
    bool found = false;
    for(model_index = 0; model_index < hw_properties.max_sound_models; model_index++) {
        if (stdev->re_context[model_index]->loaded_sound_model == handle) {
            found = true;
            break;
        }
    }
    if (found == false) {
        ALOGW("Can't find model");
        return NULL;
    }

    event = (struct sound_trigger_phrase_recognition_event *)data;
    event->common.status = RECOGNITION_STATUS_SUCCESS;
    event->common.type = SOUND_MODEL_TYPE_KEYPHRASE;
    event->common.model = handle;

    if (stdev->re_context[model_index]->config) {
        unsigned int i;

        event->num_phrases = stdev->re_context[model_index]->config->num_phrases;
        if (event->num_phrases > SOUND_TRIGGER_MAX_PHRASES)
            event->num_phrases = SOUND_TRIGGER_MAX_PHRASES;
        for (i=0; i < event->num_phrases; i++)
            memcpy(&event->phrase_extras[i], &stdev->re_context[model_index]->config->phrases[i],
                   sizeof(struct sound_trigger_phrase_recognition_extra));
    }

    event->num_phrases = 1;
    event->phrase_extras[0].confidence_level = 100;
    event->phrase_extras[0].num_levels = 1;
    event->phrase_extras[0].levels[0].level = 100;
    event->phrase_extras[0].levels[0].user_id = 0;
    // Signify that all the data is comming through streaming, not through the buffer.
    event->common.capture_available = true;

    event->common.audio_config = AUDIO_CONFIG_INITIALIZER;
    event->common.audio_config.sample_rate = 16000;
    event->common.audio_config.channel_mask = AUDIO_CHANNEL_IN_MONO;
    event->common.audio_config.format = AUDIO_FORMAT_PCM_16_BIT;

    return data;
}

static void *callback_thread_loop(void *context) {
    struct stub_sound_trigger_device *stdev = (struct stub_sound_trigger_device *)context;
    struct sockaddr_in incoming_info;
    struct sockaddr_in self_info;
    int self_socket;
    socklen_t sock_size = sizeof(struct sockaddr_in);
    memset(&self_info, 0, sizeof(self_info));
    self_info.sin_family = AF_INET;
    self_info.sin_addr.s_addr = htonl(INADDR_ANY);
    self_info.sin_port = htons(14035);

    bool exit = false;
    while(!exit) {
        int received_count;
        int requested_count = 1;
        char buffer[requested_count];
        ALOGE("Opening socket");
        self_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (self_socket < 0) {
            ALOGE("Error on socket creation");
            exit = true;
        } else {
            ALOGI("Socket created");
        }

        int reuse = 1;
        if (setsockopt(self_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0) {
            ALOGE("setsockopt(SO_REUSEADDR) failed");
        }

        int bind_result = bind(self_socket, (struct sockaddr *)&self_info, sizeof(struct sockaddr));
        if (bind_result < 0) {
            ALOGE("Error on bind");
            exit = true;
        }

        int listen_result = listen(self_socket, 1);
        if (listen_result < 0) {
            ALOGE("Error on Listen");
            exit = true;
        }

        while(!exit) {
            int con_socket = accept(self_socket, (struct sockaddr *)&incoming_info, &sock_size);
            if (!con_socket) {
                ALOGE("Lost socket, cannot send trigger");
                break;
            }
            ALOGI("Connection from %s", inet_ntoa(incoming_info.sin_addr));
            received_count = recv(con_socket, buffer, requested_count, 0);
            unsigned int index = buffer[0] - 1;
            ALOGI("Received data");
            pthread_mutex_lock(&stdev->lock);
            if (received_count > 0) {
                if (buffer[0] == 0) {
                    ALOGI("Received kill signal: stop listening to incoming server messages");
                    exit = true;
                } else if (index < hw_properties.max_sound_models) {
                    ALOGI("Going to send trigger for model #%d", index );
                    if (stdev->re_context[index]->recognition_callback != NULL) {
                        sound_model_handle_t handle = stdev->re_context[index]->loaded_sound_model;
                        if (handle == 0) {
                            ALOGW("This trigger is not loaded");
                        } else {
                            struct sound_trigger_phrase_recognition_event *event;
                            event = (struct sound_trigger_phrase_recognition_event *)
                                   sound_trigger_event_alloc(stdev, handle);
                            if (event) {
                               ALOGI("%s send callback model %d", __func__, index);
                               stdev->re_context[index]->recognition_callback(&event->common,
                                                     stdev->re_context[index]->recognition_cookie);
                               free(event);
                               stdev->re_context[index]->recognition_callback = NULL;
                            }
                            exit = true;
                        }
                    } else {
                       ALOGI("%s No matching callback for %d", __func__, index);
                    }
                } else {
                    ALOGI("Data is not recognized: %d", index);
                }
            } else {
                ALOGI("Received sata is size 0");
            }
            pthread_mutex_unlock(&stdev->lock);
            close(con_socket);
        }
        ALOGE("Closing socket");
        close(self_socket);
    }

    return NULL;
}

static void send_loop_kill_signal() {
    ALOGI("Sending loop thread kill signal");
    int self_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in remote_info;
    memset(&remote_info, 0, sizeof(remote_info));
    remote_info.sin_family = AF_INET;
    remote_info.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    remote_info.sin_port = htons(14035);
    if (connect(self_socket, (struct sockaddr *)&remote_info, sizeof(struct sockaddr)) == 0) {
        char msg[] = {0};
        send(self_socket, msg, 1, 0);
    } else {
        ALOGI("Could not connect");
    }
    close(self_socket);
    ALOGI("Sent loop thread kill signal");
}

static int stdev_get_properties(const struct sound_trigger_hw_device *dev,
                                struct sound_trigger_properties *properties) {
    struct stub_sound_trigger_device *stdev = (struct stub_sound_trigger_device *)dev;

    ALOGI("%s", __func__);
    if (properties == NULL)
        return -EINVAL;
    memcpy(properties, &hw_properties, sizeof(struct sound_trigger_properties));
    return 0;
}

static int stdev_load_sound_model(const struct sound_trigger_hw_device *dev,
                                  struct sound_trigger_sound_model *sound_model,
                                  sound_model_callback_t callback,
                                  void *cookie,
                                  sound_model_handle_t *handle) {
    struct stub_sound_trigger_device *stdev = (struct stub_sound_trigger_device *)dev;
    int status = 0;

    ALOGI("%s stdev %p", __func__, stdev);
    pthread_mutex_lock(&stdev->lock);

    if (handle == NULL || sound_model == NULL) {
        pthread_mutex_unlock(&stdev->lock);
        return -EINVAL;
    }
    if (sound_model->data_size == 0 ||
            sound_model->data_offset < sizeof(struct sound_trigger_sound_model)) {
        pthread_mutex_unlock(&stdev->lock);
        return -EINVAL;
    }

    /* Find if there is space for this sound model */
    unsigned int model_index;
    bool found = false;
    for(model_index = 0; model_index < hw_properties.max_sound_models; model_index++) {
        if (stdev->re_context[model_index]->loaded_sound_model == 0) {
            found = true;
            break;
        }
    }
    if (found == false) {
        ALOGW("Can't load model: reached max sound model limit");
        pthread_mutex_unlock(&stdev->lock);
        return -ENOSYS;
    }

    stdev->re_context[model_index]->loaded_sound_model = generate_sound_model_id(dev);
    *handle = stdev->re_context[model_index]->loaded_sound_model;

    char *data = (char *)sound_model + sound_model->data_offset;
    ALOGI("%s data size %d data %d - %d", __func__,
          sound_model->data_size, data[0], data[sound_model->data_size - 1]);
    stdev->re_context[model_index]->sound_model_callback = callback;
    stdev->re_context[model_index]->sound_model_cookie = cookie;

    pthread_mutex_unlock(&stdev->lock);
    return status;
}

static int stdev_unload_sound_model(const struct sound_trigger_hw_device *dev,
                                    sound_model_handle_t handle) {
    struct stub_sound_trigger_device *stdev = (struct stub_sound_trigger_device *)dev;
    int status = 0;
    ALOGI("unload_sound_model");
    pthread_mutex_lock(&stdev->lock);

    unsigned int i;
    unsigned int model_index;
    bool found = false;
    bool other_callbacks_found = false;
    for(i = 0; i < hw_properties.max_sound_models; i++) {
        if (stdev->re_context[i]->loaded_sound_model == handle) {
            found = true;
            model_index = i;
            break;
        } else if (stdev->re_context[i]->recognition_callback != NULL) {
            other_callbacks_found = true;
        }
    }
    if (found == false) {
        ALOGW("Can't sound model %d in registered list", handle);
        pthread_mutex_unlock(&stdev->lock);
        return -ENOSYS;
    }

    stdev->re_context[i]->loaded_sound_model = 0;
    stdev->re_context[i]->sound_model_callback = NULL;
    stdev->re_context[i]->sound_model_cookie = NULL;

    free(stdev->re_context[i]->config);
    stdev->re_context[i]->config = NULL;
    stdev->re_context[i]->recognition_callback = NULL;
    stdev->re_context[i]->recognition_cookie = NULL;

    /* If no more models running with callbacks, stop trigger thread */
    if (!other_callbacks_found) {
        send_loop_kill_signal();
        pthread_mutex_unlock(&stdev->lock);
        pthread_join(stdev->callback_thread, (void **)NULL);
    } else {
        pthread_mutex_unlock(&stdev->lock);
    }

    return status;
}

static int stdev_start_recognition(const struct sound_trigger_hw_device *dev,
                                   sound_model_handle_t handle,
                                   const struct sound_trigger_recognition_config *config,
                                   recognition_callback_t callback,
                                   void *cookie) {
    ALOGI("%s", __func__);
    struct stub_sound_trigger_device *stdev = (struct stub_sound_trigger_device *)dev;
    pthread_mutex_lock(&stdev->lock);
    unsigned int i;
    bool found = false;
    for(i = 0; i < hw_properties.max_sound_models; i++) {
        if (stdev->re_context[i]->loaded_sound_model == handle) {
            found = true;
            break;
        }
    }
    if (found == false) {
        ALOGW("Can't sound model %d in registered list", handle);
        pthread_mutex_unlock(&stdev->lock);
        return -ENOSYS;
    }

    free(stdev->re_context[i]->config);
    stdev->re_context[i]->config = NULL;
    if (config) {
        stdev->re_context[i]->config = malloc(sizeof(*config));
        if (!stdev->re_context[i]->config) {
            pthread_mutex_unlock(&stdev->lock);
            return -ENOMEM;
        }
        memcpy(stdev->re_context[i]->config, config, sizeof(*config));
    }
    stdev->re_context[i]->recognition_callback = callback;
    stdev->re_context[i]->recognition_cookie = cookie;

    pthread_create(&stdev->callback_thread, (const pthread_attr_t *) NULL,
                        callback_thread_loop, stdev);
    pthread_mutex_unlock(&stdev->lock);
    return 0;
}

static int stdev_stop_recognition(const struct sound_trigger_hw_device *dev,
                                 sound_model_handle_t handle) {
    struct stub_sound_trigger_device *stdev = (struct stub_sound_trigger_device *)dev;
    ALOGI("%s", __func__);
    pthread_mutex_lock(&stdev->lock);

    unsigned int i;
    bool found = false;
    for(i = 0; i < hw_properties.max_sound_models; i++) {
        if (stdev->re_context[i]->loaded_sound_model == handle) {
            found = true;
            break;
        }
    }
    if (found == false) {
        ALOGW("Can't sound model %d in registered list", handle);
        pthread_mutex_unlock(&stdev->lock);
        return -ENOSYS;
    }

    free(stdev->re_context[i]->config);
    stdev->re_context[i]->config = NULL;
    stdev->re_context[i]->recognition_callback = NULL;
    stdev->re_context[i]->recognition_cookie = NULL;

    send_loop_kill_signal();
    pthread_mutex_unlock(&stdev->lock);
    pthread_join(stdev->callback_thread, (void **) NULL);
    return 0;
}

static int stdev_close(hw_device_t *device) {
    free(device);
    return 0;
}

static int stdev_open(const hw_module_t* module, const char* name,
                     hw_device_t** device) {
    struct stub_sound_trigger_device *stdev;
    int ret;

    if (strcmp(name, SOUND_TRIGGER_HARDWARE_INTERFACE) != 0)
        return -EINVAL;

    stdev = calloc(1, sizeof(struct stub_sound_trigger_device));
    if (!stdev)
        return -ENOMEM;

    if (MAX_MAX_SOUND_MODELS < hw_properties.max_sound_models) {
        ALOGW("max_sound_models is greater than the allowed %d", MAX_MAX_SOUND_MODELS);
        return -EINVAL;
    }

    stdev->next_sound_model_id = 1;
    unsigned int i;
    for(i = 0; i < hw_properties.max_sound_models; i++) {
        stdev->re_context[i]->loaded_sound_model = 0;
        stdev->re_context[i]->sound_model_callback = NULL;
        stdev->re_context[i]->sound_model_cookie = NULL;
        stdev->re_context[i]->config = NULL;
        stdev->re_context[i]->recognition_callback = NULL;
        stdev->re_context[i]->recognition_cookie = NULL;
    }

    stdev->device.common.tag = HARDWARE_DEVICE_TAG;
    stdev->device.common.version = SOUND_TRIGGER_DEVICE_API_VERSION_1_0;
    stdev->device.common.module = (struct hw_module_t *) module;
    stdev->device.common.close = stdev_close;
    stdev->device.get_properties = stdev_get_properties;
    stdev->device.load_sound_model = stdev_load_sound_model;
    stdev->device.unload_sound_model = stdev_unload_sound_model;
    stdev->device.start_recognition = stdev_start_recognition;
    stdev->device.stop_recognition = stdev_stop_recognition;

    pthread_mutex_init(&stdev->lock, (const pthread_mutexattr_t *) NULL);

    *device = &stdev->device.common;

    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = stdev_open,
};

struct sound_trigger_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = SOUND_TRIGGER_MODULE_API_VERSION_1_0,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = SOUND_TRIGGER_HARDWARE_MODULE_ID,
        .name = "Default sound trigger HAL",
        .author = "The Android Open Source Project",
        .methods = &hal_module_methods,
    },
};
