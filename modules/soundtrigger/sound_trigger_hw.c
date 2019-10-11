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
 *
 * telnet localhost 14035
 *
 * Commands include:
 * ls : Lists all models that have been loaded.
 * trig <uuid> : Sends a recognition event for the model at the given uuid
 * update <uuid> : Sends a model update event for the model at the given uuid.
 * close : Closes the network connection.
 *
 * To enable this file, you can make with command line parameter
 * SOUND_TRIGGER_USE_STUB_MODULE=1
 */

#define LOG_TAG "sound_trigger_hw_default"
#define LOG_NDEBUG 1
#define PARSE_BUF_LEN 1024  // Length of the parsing buffer.S

#define EVENT_RECOGNITION 1
#define EVENT_SOUND_MODEL 2

// The following commands work with the network port:
#define COMMAND_LS "ls"
#define COMMAND_RECOGNITION_TRIGGER "trig"  // Argument: model index.
#define COMMAND_RECOGNITION_ABORT "abort"  // Argument: model index.
#define COMMAND_RECOGNITION_FAILURE "fail"  // Argument: model index.
#define COMMAND_UPDATE "update"  // Argument: model index.
#define COMMAND_CLEAR "clear" // Removes all models from the list.
#define COMMAND_CLOSE "close" // Close just closes the network port, keeps thread running.
#define COMMAND_END "end" // Closes connection and stops the thread.

#define ERROR_BAD_COMMAND "Bad command"

#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <log/log.h>

#include <hardware/hardware.h>
#include <system/sound_trigger.h>
#include <hardware/sound_trigger.h>

static const struct sound_trigger_properties hw_properties = {
        "The Android Open Source Project", // implementor
        "Sound Trigger stub HAL", // description
        1, // version
        { 0xed7a7d60, 0xc65e, 0x11e3, 0x9be4, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
        4, // max_sound_models
        1, // max_key_phrases
        1, // max_users
        RECOGNITION_MODE_VOICE_TRIGGER, // recognition_modes
        false, // capture_transition
        0, // max_buffer_ms
        true, // concurrent_capture
        false, // trigger_in_event
        0 // power_consumption_mw
};

struct recognition_context {
    // Sound Model information, added in method load_sound_model
    sound_model_handle_t model_handle;
    sound_trigger_uuid_t model_uuid;
    sound_trigger_sound_model_type_t model_type;
    sound_model_callback_t model_callback;
    void *model_cookie;

    // Sound Model information, added in start_recognition
    struct sound_trigger_recognition_config *config;
    recognition_callback_t recognition_callback;
    void *recognition_cookie;

    bool model_started;

    // Next recognition_context in the linked list
    struct recognition_context *next;
};

char tmp_write_buffer[PARSE_BUF_LEN];

struct stub_sound_trigger_device {
    struct sound_trigger_hw_device device;
    pthread_mutex_t lock;

    // This thread opens a port that can be used to monitor and inject events
    // into the stub HAL.
    pthread_t control_thread;

    // Recognition contexts are stored as a linked list
    struct recognition_context *root_model_context;

    int next_sound_model_id;
};

static bool check_uuid_equality(sound_trigger_uuid_t uuid1, sound_trigger_uuid_t uuid2) {
    if (uuid1.timeLow != uuid2.timeLow ||
        uuid1.timeMid != uuid2.timeMid ||
        uuid1.timeHiAndVersion != uuid2.timeHiAndVersion ||
        uuid1.clockSeq != uuid2.clockSeq) {
        return false;
    }
    for (int i = 0; i < 6; i++) {
        if(uuid1.node[i] != uuid2.node[i]) {
            return false;
        }
    }
    return true;
}

bool str_to_uuid(char* uuid_str, sound_trigger_uuid_t* uuid) {
    if (uuid_str == NULL) {
        ALOGI("Invalid str_to_uuid input.");
        return false;
    }

    int tmp[10];
    if (sscanf(uuid_str, "%08x-%04x-%04x-%04x-%02x%02x%02x%02x%02x%02x",
               tmp, tmp+1, tmp+2, tmp+3, tmp+4, tmp+5, tmp+6, tmp+7, tmp+8, tmp+9) < 10) {
        ALOGI("Invalid UUID, got: %s", uuid_str);
        return false;
    }
    uuid->timeLow = (unsigned int)tmp[0];
    uuid->timeMid = (unsigned short)tmp[1];
    uuid->timeHiAndVersion = (unsigned short)tmp[2];
    uuid->clockSeq = (unsigned short)tmp[3];
    uuid->node[0] = (unsigned char)tmp[4];
    uuid->node[1] = (unsigned char)tmp[5];
    uuid->node[2] = (unsigned char)tmp[6];
    uuid->node[3] = (unsigned char)tmp[7];
    uuid->node[4] = (unsigned char)tmp[8];
    uuid->node[5] = (unsigned char)tmp[9];
    return true;
}

void write_bad_command_error(int conn_socket, char* command) {
    int num = snprintf(tmp_write_buffer, PARSE_BUF_LEN, "Bad command received: %s", command);
    tmp_write_buffer[PARSE_BUF_LEN - 1] = '\0';  // Just to be sure.
    tmp_write_buffer[PARSE_BUF_LEN - 2] = '\n';
    write(conn_socket, tmp_write_buffer, num);
}

void write_string(int conn_socket, char* str) {
    int num = snprintf(tmp_write_buffer, PARSE_BUF_LEN, "%s", str);
    tmp_write_buffer[PARSE_BUF_LEN - 1] = '\0';
    tmp_write_buffer[PARSE_BUF_LEN - 2] = '\n';
    write(conn_socket, tmp_write_buffer, num);
}

void write_vastr(int conn_socket, char* format, ...) {
    va_list argptr;
    va_start(argptr, format);
    int num = vsnprintf(tmp_write_buffer, PARSE_BUF_LEN, format, argptr);
    va_end(argptr);
    tmp_write_buffer[PARSE_BUF_LEN - 1] = '\0';
    tmp_write_buffer[PARSE_BUF_LEN - 2] = '\n';
    write(conn_socket, tmp_write_buffer, num);
}

static void print_uuid(sound_trigger_uuid_t uuid) {
    ALOGI("%s %08x-%04x-%04x-%04x-%02x%02x%02x%02x%02x%02x", __func__, uuid.timeLow, uuid.timeMid,
          uuid.timeHiAndVersion, uuid.clockSeq, uuid.node[0], uuid.node[1], uuid.node[2],
          uuid.node[3], uuid.node[4], uuid.node[5]);
}

static void write_uuid(int conn_socket, sound_trigger_uuid_t uuid) {
    write_vastr(conn_socket, "%08x-%04x-%04x-%04x-%02x%02x%02x%02x%02x%02x\n", uuid.timeLow, uuid.timeMid,
                uuid.timeHiAndVersion, uuid.clockSeq, uuid.node[0], uuid.node[1], uuid.node[2],
                uuid.node[3], uuid.node[4], uuid.node[5]);
}

// Returns model at the given index, null otherwise (error, doesn't exist, etc).
// Note that here index starts from zero.
struct recognition_context* fetch_model_with_handle(
        struct stub_sound_trigger_device* stdev, sound_model_handle_t* model_handle) {
    ALOGI("%s", __func__);
    struct recognition_context *model_context = NULL;
    struct recognition_context *last_model_context = stdev->root_model_context;
    while(last_model_context) {
        if (last_model_context->model_handle == *model_handle) {
            model_context = last_model_context;
            break;
        }
        last_model_context = last_model_context->next;
    }
    return model_context;
}

// Returns the first model that matches the sound model UUID.
static sound_model_handle_t* get_model_handle_with_uuid(struct stub_sound_trigger_device* stdev,
                                                        sound_trigger_uuid_t uuid) {
    sound_model_handle_t* model_handle_str = NULL;
    struct recognition_context *last_model_context = stdev->root_model_context;
    while(last_model_context) {
        if (check_uuid_equality(last_model_context->model_uuid, uuid)) {
            model_handle_str = &last_model_context->model_handle;
            break;
        }
        last_model_context = last_model_context->next;
    }
    return model_handle_str;
}

/* Will reuse ids when overflow occurs */
static sound_model_handle_t generate_sound_model_handle(const struct sound_trigger_hw_device *dev) {
    struct stub_sound_trigger_device *stdev = (struct stub_sound_trigger_device *)dev;
    int new_id = stdev->next_sound_model_id;
    ++stdev->next_sound_model_id;
    if (stdev->next_sound_model_id == 0) {
        stdev->next_sound_model_id = 1;
    }
    return (sound_model_handle_t) new_id;
}

bool parse_socket_data(int conn_socket, struct stub_sound_trigger_device* stdev);
static void unload_all_sound_models(struct stub_sound_trigger_device *stdev);

static char *sound_trigger_keyphrase_event_alloc(sound_model_handle_t handle,
                                                 struct sound_trigger_recognition_config *config,
                                                 int recognition_status) {
    char *data;
    struct sound_trigger_phrase_recognition_event *event;
    data = (char *)calloc(1, sizeof(struct sound_trigger_phrase_recognition_event));
    if (!data)
        return NULL;
    event = (struct sound_trigger_phrase_recognition_event *)data;
    event->common.status = recognition_status;
    event->common.type = SOUND_MODEL_TYPE_KEYPHRASE;
    event->common.model = handle;

    if (config) {
        unsigned int i;

        event->num_phrases = config->num_phrases;
        if (event->num_phrases > SOUND_TRIGGER_MAX_PHRASES)
            event->num_phrases = SOUND_TRIGGER_MAX_PHRASES;
        for (i=0; i < event->num_phrases; i++)
            memcpy(&event->phrase_extras[i],
                   &config->phrases[i],
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

static char *sound_trigger_generic_event_alloc(sound_model_handle_t handle,
                                               struct sound_trigger_recognition_config *config,
                                               int recognition_status) {
    char *data;
    struct sound_trigger_generic_recognition_event *event;
    data = (char *)calloc(1, sizeof(struct sound_trigger_generic_recognition_event));
    if (!data)
        return NULL;
    event = (struct sound_trigger_generic_recognition_event *)data;
    event->common.status = recognition_status;
    event->common.type = SOUND_MODEL_TYPE_GENERIC;
    event->common.model = handle;

    // Signify that all the data is comming through streaming, not through the buffer.
    event->common.capture_available = true;
    event->common.audio_config = AUDIO_CONFIG_INITIALIZER;
    event->common.audio_config.sample_rate = 16000;
    event->common.audio_config.channel_mask = AUDIO_CHANNEL_IN_MONO;
    event->common.audio_config.format = AUDIO_FORMAT_PCM_16_BIT;
    return data;
}

void send_event_with_handle(sound_model_handle_t* model_handle_str,
                            struct stub_sound_trigger_device* stdev, int event_type,
                            int status) {
    ALOGI("%s", __func__);
    struct recognition_context *model_context = fetch_model_with_handle(stdev, model_handle_str);
    if (model_context) {
        if (event_type == EVENT_RECOGNITION) {
            if (model_context->recognition_callback == NULL) {
                ALOGI("%s No matching callback", __func__);
                return;
            }

            if (model_context->model_type == SOUND_MODEL_TYPE_KEYPHRASE) {
                struct sound_trigger_phrase_recognition_event *event;
                event = (struct sound_trigger_phrase_recognition_event *)
                        sound_trigger_keyphrase_event_alloc(model_context->model_handle,
                                                            model_context->config, status);
                if (event) {
                    model_context->recognition_callback(event, model_context->recognition_cookie);
                    free(event);
                }
            } else if (model_context->model_type == SOUND_MODEL_TYPE_GENERIC) {
                struct sound_trigger_generic_recognition_event *event;
                event = (struct sound_trigger_generic_recognition_event *)
                        sound_trigger_generic_event_alloc(model_context->model_handle,
                                                          model_context->config, status);
                if (event) {
                    model_context->recognition_callback(event, model_context->recognition_cookie);
                    free(event);
                }
            } else {
                ALOGI("Unknown Sound Model Type, No Event to Send");
            }
        } else if (event_type == EVENT_SOUND_MODEL) {
            char *data;
            data = (char *)calloc(1, sizeof(struct sound_trigger_model_event));
            if (!data) {
                ALOGW("%s Could not allocate event", __func__);
                return;
            }

            struct sound_trigger_model_event *event;
            event = (struct sound_trigger_model_event *)data;
            event->status = SOUND_MODEL_STATUS_UPDATED;
            event->model = model_context->model_handle;
            if (event) {
                model_context->model_callback(&event, model_context->model_cookie);
                free(event);
            }
        }
    } else {
        ALOGI("No model for this handle");
    }
}

static void send_event(int conn_socket, struct stub_sound_trigger_device* stdev, int event_type,
                       int status) {
    char* model_uuid_str = strtok(NULL, " \r\n");
    sound_trigger_uuid_t model_uuid;
    if (str_to_uuid(model_uuid_str, &model_uuid)) {
        sound_model_handle_t* model_handle_str = get_model_handle_with_uuid(stdev, model_uuid);
        if (model_handle_str == NULL) {
            ALOGI("%s Bad sound model handle.", __func__);
            write_string(conn_socket, "Bad sound model handle.\n");
            return;
        }
        send_event_with_handle(model_handle_str, stdev, event_type, status);
    } else {
        ALOGI("%s Not a valid UUID", __func__);
        write_string(conn_socket, "Not a valid UUID.\n");
    }
}

static bool recognition_callback_exists(struct stub_sound_trigger_device *stdev) {
    bool callback_found = false;
    if (stdev->root_model_context) {
        struct recognition_context *current_model_context = stdev->root_model_context;
        while(current_model_context) {
            if (current_model_context->recognition_callback != NULL) {
                callback_found = true;
                break;
            }
            current_model_context = current_model_context->next;
        }
    }
    return callback_found;
}

static struct recognition_context * get_model_context(struct stub_sound_trigger_device *stdev,
            sound_model_handle_t handle) {
    struct recognition_context *model_context = NULL;
    if (stdev->root_model_context) {
        struct recognition_context *current_model_context = stdev->root_model_context;
        while(current_model_context) {
            if (current_model_context->model_handle == handle) {
                model_context = current_model_context;
                break;
            }
            current_model_context = current_model_context->next;
        }
    }
    return model_context;
}

static void *control_thread_loop(void *context) {
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
        int requested_count = 2;
        char buffer[requested_count];
        ALOGE("Opening socket");
        self_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (self_socket < 0) {
            ALOGE("Error on socket creation: %s", strerror(errno));
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
            if (!parse_socket_data(con_socket, stdev)) {
                ALOGI("Done processing commands over network. Stopping thread.");
                exit = true;
            }
            close(con_socket);
        }
        ALOGE("Closing socket");
        close(self_socket);
    }

    return NULL;
}

void list_models(int conn_socket, char* buffer,
                 struct stub_sound_trigger_device* stdev) {
    ALOGI("%s", __func__);
    struct recognition_context *last_model_context = stdev->root_model_context;
    unsigned int model_index = 0;
    write_string(conn_socket, "-----------------------\n");
    if (!last_model_context) {
        ALOGI("ZERO Models exist.");
        write_string(conn_socket, "Zero models exist.\n");
    }
    while (last_model_context) {
        write_vastr(conn_socket, "Model Index: %d\n", model_index);
        ALOGI("Model Index: %d", model_index);
        write_vastr(conn_socket, "Model handle: %d\n", last_model_context->model_handle);
        ALOGI("Model handle: %d", last_model_context->model_handle);
        write_uuid(conn_socket, last_model_context->model_uuid);
        print_uuid(last_model_context->model_uuid);
        sound_trigger_sound_model_type_t model_type = last_model_context->model_type;

        if (model_type == SOUND_MODEL_TYPE_KEYPHRASE) {
            write_string(conn_socket, "Keyphrase sound Model.\n");
            ALOGI("Keyphrase sound Model.");
        } else if (model_type == SOUND_MODEL_TYPE_GENERIC) {
            write_string(conn_socket, "Generic sound Model.\n");
            ALOGI("Generic sound Model.");
        } else {
            write_vastr(conn_socket, "Unknown sound model type: %d\n", model_type);
            ALOGI("Unknown sound model type: %d", model_type);
        }
        if (last_model_context->model_started) {
            write_string(conn_socket, "Model started.\n");
            ALOGI("Model started.\n");
        } else {
            write_string(conn_socket, "Model stopped.\n");
            ALOGI("Model stopped.\n");
        }
        write_string(conn_socket, "-----------------------\n\n");
        ALOGI("----\n\n");
        last_model_context = last_model_context->next;
        model_index++;
    }
}

// Gets the next word from buffer, replaces '\n' or ' ' with '\0'.
char* get_command(char* buffer) {
    char* command = strtok(buffer, " ");
    char* newline = strchr(command, '\n');
    if (newline != NULL) {
        *newline = '\0';
    }
    return command;
}

// Parses data coming in from the local socket, executes commands. Returns when
// done. Return code indicates whether the server should continue listening or
// abort (true if continue listening).
bool parse_socket_data(int conn_socket, struct stub_sound_trigger_device* stdev) {
    ALOGI("Calling parse_socket_data");
    bool input_done = false;
    char buffer[PARSE_BUF_LEN];
    FILE* input_fp = fdopen(conn_socket, "r");
    bool continue_listening = true;

    // Note: Since we acquire a lock inside this loop, do not use break or other
    // exit methods without releasing this lock.
    write_string(conn_socket, "\n>>> ");
    while(!input_done) {
        if (fgets(buffer, PARSE_BUF_LEN, input_fp) != NULL) {
            pthread_mutex_lock(&stdev->lock);
            char* command = strtok(buffer, " \r\n");
            if (command == NULL) {
                write_bad_command_error(conn_socket, command);
            } else if (strncmp(command, COMMAND_LS, 2) == 0) {
                list_models(conn_socket, buffer, stdev);
            } else if (strcmp(command, COMMAND_RECOGNITION_TRIGGER) == 0) {
                send_event(conn_socket, stdev, EVENT_RECOGNITION, RECOGNITION_STATUS_SUCCESS);
            } else if (strcmp(command, COMMAND_RECOGNITION_ABORT) == 0) {
                send_event(conn_socket, stdev, EVENT_RECOGNITION, RECOGNITION_STATUS_ABORT);
            } else if (strcmp(command, COMMAND_RECOGNITION_FAILURE) == 0) {
                send_event(conn_socket, stdev, EVENT_RECOGNITION, RECOGNITION_STATUS_FAILURE);
            } else if (strcmp(command, COMMAND_UPDATE) == 0) {
                send_event(conn_socket, stdev, EVENT_SOUND_MODEL, SOUND_MODEL_STATUS_UPDATED);
            } else if (strncmp(command, COMMAND_CLEAR, 5) == 0) {
                unload_all_sound_models(stdev);
            } else if (strncmp(command, COMMAND_CLOSE, 5) == 0) {
                ALOGI("Closing this connection.");
                write_string(conn_socket, "Closing this connection.");
                input_done = true;
            } else if (strncmp(command, COMMAND_END, 3) == 0) {
                ALOGI("End command received.");
                write_string(conn_socket, "End command received. Stopping connection.");
                continue_listening = false;
                input_done = true;
            } else {
                write_vastr(conn_socket, "\nBad command %s.\n\n", command);
            }
            pthread_mutex_unlock(&stdev->lock);
        } else {
            ALOGI("parse_socket_data done (got null)");
            input_done = true;  // break.
        }
        write_string(conn_socket, "\n>>> ");
    }
    return continue_listening;
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
        send(self_socket, COMMAND_END, 3, 0);
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
    ALOGI("%s stdev %p", __func__, stdev);
    int status = 0;
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

    struct recognition_context *model_context;
    model_context = malloc(sizeof(struct recognition_context));
    if(!model_context) {
        ALOGW("Could not allocate recognition_context");
        pthread_mutex_unlock(&stdev->lock);
        return -ENOSYS;
    }

    // Add the new model context to the recognition_context linked list
    if (stdev->root_model_context) {
        // Find the tail
        struct recognition_context *current_model_context = stdev->root_model_context;
        unsigned int model_count = 0;
        while(current_model_context->next) {
            current_model_context = current_model_context->next;
            model_count++;
            if (model_count >= hw_properties.max_sound_models) {
                ALOGW("Can't load model: reached max sound model limit");
                free(model_context);
                pthread_mutex_unlock(&stdev->lock);
                return -ENOSYS;
            }
        }
        current_model_context->next = model_context;
    } else {
        stdev->root_model_context = model_context;
    }

    model_context->model_handle = generate_sound_model_handle(dev);
    *handle = model_context->model_handle;
    model_context->model_type = sound_model->type;

    char *data = (char *)sound_model + sound_model->data_offset;
    ALOGI("%s data size %d data %d - %d", __func__,
          sound_model->data_size, data[0], data[sound_model->data_size - 1]);
    model_context->model_uuid = sound_model->uuid;
    model_context->model_callback = callback;
    model_context->model_cookie = cookie;
    model_context->config = NULL;
    model_context->recognition_callback = NULL;
    model_context->recognition_cookie = NULL;
    model_context->next = NULL;
    model_context->model_started = false;
    ALOGI("Sound model loaded: Handle %d ", *handle);

    pthread_mutex_unlock(&stdev->lock);
    return status;
}

static void unload_all_sound_models(struct stub_sound_trigger_device *stdev) {
    ALOGI("%s", __func__);
    struct recognition_context *model_context = stdev->root_model_context;
    stdev->root_model_context = NULL;
    pthread_mutex_lock(&stdev->lock);
    while (model_context) {
        ALOGI("Deleting model with handle: %d", model_context->model_handle);
        struct recognition_context *temp = model_context;
        model_context = model_context->next;
        free(temp->config);
        free(temp);
    }
    pthread_mutex_unlock(&stdev->lock);
}

static int stdev_unload_sound_model(const struct sound_trigger_hw_device *dev,
                                    sound_model_handle_t handle) {
    // If recognizing, stop_recognition must be called for a sound model before unload_sound_model
    ALOGI("%s", __func__);
    struct stub_sound_trigger_device *stdev = (struct stub_sound_trigger_device *)dev;
    int status = 0;
    ALOGI("unload_sound_model:%d", handle);
    pthread_mutex_lock(&stdev->lock);

    struct recognition_context *model_context = NULL;
    struct recognition_context *previous_model_context = NULL;
    if (stdev->root_model_context) {
        struct recognition_context *current_model_context = stdev->root_model_context;
        while(current_model_context) {
            if (current_model_context->model_handle == handle) {
                model_context = current_model_context;
                break;
            }
            previous_model_context = current_model_context;
            current_model_context = current_model_context->next;
        }
    }
    if (!model_context) {
        ALOGW("Can't find sound model handle %d in registered list", handle);
        pthread_mutex_unlock(&stdev->lock);
        return -ENOSYS;
    }
    if (previous_model_context) {
        previous_model_context->next = model_context->next;
    } else {
        stdev->root_model_context = model_context->next;
    }
    free(model_context->config);
    free(model_context);
    pthread_mutex_unlock(&stdev->lock);
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

    /* If other models running with callbacks, don't start trigger thread */
    bool other_callbacks_found = recognition_callback_exists(stdev);

    struct recognition_context *model_context = get_model_context(stdev, handle);
    if (!model_context) {
        ALOGW("Can't find sound model handle %d in registered list", handle);
        pthread_mutex_unlock(&stdev->lock);
        return -ENOSYS;
    }

    free(model_context->config);
    model_context->config = NULL;
    if (config) {
        model_context->config = malloc(sizeof(*config));
        if (!model_context->config) {
            pthread_mutex_unlock(&stdev->lock);
            return -ENOMEM;
        }
        memcpy(model_context->config, config, sizeof(*config));
    }
    model_context->recognition_callback = callback;
    model_context->recognition_cookie = cookie;
    model_context->model_started = true;

    pthread_mutex_unlock(&stdev->lock);
    ALOGI("%s done for handle %d", __func__, handle);
    return 0;
}

static int stdev_stop_recognition(const struct sound_trigger_hw_device *dev,
            sound_model_handle_t handle) {
    struct stub_sound_trigger_device *stdev = (struct stub_sound_trigger_device *)dev;
    ALOGI("%s", __func__);
    pthread_mutex_lock(&stdev->lock);

    struct recognition_context *model_context = get_model_context(stdev, handle);
    if (!model_context) {
        ALOGW("Can't find sound model handle %d in registered list", handle);
        pthread_mutex_unlock(&stdev->lock);
        return -ENOSYS;
    }

    free(model_context->config);
    model_context->config = NULL;
    model_context->recognition_callback = NULL;
    model_context->recognition_cookie = NULL;
    model_context->model_started = false;

    pthread_mutex_unlock(&stdev->lock);
    ALOGI("%s done for handle %d", __func__, handle);

    return 0;
}

static int stdev_stop_all_recognitions(const struct sound_trigger_hw_device *dev) {
    struct stub_sound_trigger_device *stdev = (struct stub_sound_trigger_device *)dev;
    ALOGI("%s", __func__);
    pthread_mutex_lock(&stdev->lock);

    struct recognition_context *model_context = stdev->root_model_context;
    while (model_context) {
        free(model_context->config);
        model_context->config = NULL;
        model_context->recognition_callback = NULL;
        model_context->recognition_cookie = NULL;
        model_context->model_started = false;
        ALOGI("%s stopped handle %d", __func__, model_context->model_handle);

        model_context = model_context->next;
    }

    pthread_mutex_unlock(&stdev->lock);

    return 0;
}

static int stdev_get_model_state(const struct sound_trigger_hw_device *dev,
                                 sound_model_handle_t handle) {
    int ret = 0;
    struct stub_sound_trigger_device *stdev = (struct stub_sound_trigger_device *)dev;
    ALOGI("%s", __func__);
    pthread_mutex_lock(&stdev->lock);

    struct recognition_context *model_context = get_model_context(stdev, handle);
    if (!model_context) {
        ALOGW("Can't find sound model handle %d in registered list", handle);
        ret = -ENOSYS;
        goto exit;
    }

    if (!model_context->model_started) {
        ALOGW("Sound model %d not started", handle);
        ret = -ENOSYS;
        goto exit;
    }

    if (model_context->recognition_callback == NULL) {
        ALOGW("Sound model %d not initialized", handle);
        ret = -ENOSYS;
        goto exit;
    }

    // TODO(mdooley): trigger recognition event

exit:
    pthread_mutex_unlock(&stdev->lock);
    ALOGI("%s done for handle %d", __func__, handle);

    return ret;
}

__attribute__ ((visibility ("default")))
int sound_trigger_open_for_streaming() {
    int ret = 0;
    return ret;
}

__attribute__ ((visibility ("default")))
size_t sound_trigger_read_samples(int audio_handle, void *buffer, size_t  buffer_len) {
    size_t ret = 0;
    return ret;
}

__attribute__ ((visibility ("default")))
int sound_trigger_close_for_streaming(int audio_handle __unused) {
    return 0;
}

static int stdev_close(hw_device_t *device) {
    // TODO: Implement the ability to stop the control thread. Since this is a
    // test hal, we have skipped implementing this for now. A possible method
    // would register a signal handler for the control thread so that any
    // blocking socket calls can be interrupted. We would send that signal here
    // to interrupt and quit the thread.
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

    stdev->next_sound_model_id = 1;
    stdev->root_model_context = NULL;

    stdev->device.common.tag = HARDWARE_DEVICE_TAG;
    stdev->device.common.version = SOUND_TRIGGER_DEVICE_API_VERSION_1_1;
    stdev->device.common.module = (struct hw_module_t *) module;
    stdev->device.common.close = stdev_close;
    stdev->device.get_properties = stdev_get_properties;
    stdev->device.load_sound_model = stdev_load_sound_model;
    stdev->device.unload_sound_model = stdev_unload_sound_model;
    stdev->device.start_recognition = stdev_start_recognition;
    stdev->device.stop_recognition = stdev_stop_recognition;
    stdev->device.stop_all_recognitions = stdev_stop_all_recognitions;
    stdev->device.get_model_state = stdev_get_model_state;

    pthread_mutex_init(&stdev->lock, (const pthread_mutexattr_t *) NULL);

    *device = &stdev->device.common;

    pthread_create(&stdev->control_thread, (const pthread_attr_t *) NULL,
                control_thread_loop, stdev);
    ALOGI("Starting control thread for the stub hal.");

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
