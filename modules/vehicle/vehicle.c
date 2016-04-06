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

#define LOG_TAG "vehicle_hw_default"
#define LOG_NDEBUG 1
#define RADIO_PRESET_NUM 6

#define UNUSED __attribute__((__unused__))

#include <errno.h>
#include <inttypes.h>
#include <malloc.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <time.h>

#include <cutils/log.h>
#include <system/radio.h>
#include <hardware/hardware.h>
#include <hardware/vehicle.h>

extern int64_t elapsedRealtimeNano();

static char VEHICLE_MAKE[] = "android_car";

typedef struct vehicle_device_impl {
    vehicle_hw_device_t vehicle_device;
    uint32_t initialized_;
    vehicle_event_callback_fn event_fn_;
    vehicle_error_callback_fn error_fn_;
} vehicle_device_impl_t ;

static pthread_mutex_t lock_;

typedef struct subscription {
    // Each subscription has it's own thread.
    pthread_t thread_id;
    int32_t prop;
    float sample_rate;
    pthread_mutex_t lock;
    // This field should be protected by the above mutex.
    // TODO change this to something better as flag alone takes long time to finish.
    uint32_t stop_thread;
    vehicle_device_impl_t* impl;
    pthread_t thread;
    pthread_cond_t cond;
    char name[100];
} subscription_t;

static vehicle_prop_config_t CONFIGS[] = {
    {
        .prop = VEHICLE_PROPERTY_INFO_MAKE,
        .access = VEHICLE_PROP_ACCESS_READ,
        .change_mode = VEHICLE_PROP_CHANGE_MODE_STATIC,
        .value_type = VEHICLE_VALUE_TYPE_STRING,
        .min_sample_rate = 0,
        .max_sample_rate = 0,
        .hal_data = NULL,
    },
    {
        .prop = VEHICLE_PROPERTY_GEAR_SELECTION,
        .access = VEHICLE_PROP_ACCESS_READ,
        .change_mode = VEHICLE_PROP_CHANGE_MODE_ON_CHANGE,
        .value_type = VEHICLE_VALUE_TYPE_INT32,
        .min_sample_rate = 0,
        .max_sample_rate = 0,
        .hal_data = NULL,
    },
    {
        .prop = VEHICLE_PROPERTY_DRIVING_STATUS,
        .access = VEHICLE_PROP_ACCESS_READ,
        .change_mode = VEHICLE_PROP_CHANGE_MODE_ON_CHANGE,
        .value_type = VEHICLE_VALUE_TYPE_INT32,
        .min_sample_rate = 0,
        .max_sample_rate = 0,
        .hal_data = NULL,
    },
    {
        .prop = VEHICLE_PROPERTY_PARKING_BRAKE_ON,
        .access = VEHICLE_PROP_ACCESS_READ,
        .change_mode = VEHICLE_PROP_CHANGE_MODE_ON_CHANGE,
        .value_type = VEHICLE_VALUE_TYPE_BOOLEAN,
        .min_sample_rate = 0,
        .max_sample_rate = 0,
        .hal_data = NULL,
    },
    {
        .prop = VEHICLE_PROPERTY_PERF_VEHICLE_SPEED,
        .access = VEHICLE_PROP_ACCESS_READ,
        .change_mode = VEHICLE_PROP_CHANGE_MODE_CONTINUOUS,
        .value_type = VEHICLE_VALUE_TYPE_FLOAT,
        .min_sample_rate = 0.1,
        .max_sample_rate = 10.0,
        .hal_data = NULL,
    },
    {
        .prop = VEHICLE_PROPERTY_RADIO_PRESET,
        .access = VEHICLE_PROP_ACCESS_READ_WRITE,
        .change_mode = VEHICLE_PROP_CHANGE_MODE_ON_CHANGE,
        .value_type = VEHICLE_VALUE_TYPE_INT32_VEC4,
        .vehicle_radio_num_presets = RADIO_PRESET_NUM,
        .min_sample_rate = 0,
        .max_sample_rate = 0,
        .hal_data = NULL,
    },
};

vehicle_prop_config_t* find_config(int prop) {
    unsigned int i;
    for (i = 0; i < sizeof(CONFIGS) / sizeof(vehicle_prop_config_t); i++) {
        if (CONFIGS[i].prop == prop) {
            return &CONFIGS[i];
        }
    }
    return NULL;
}

static int alloc_vehicle_str_from_cstr(const char* string, vehicle_str_t* vehicle_str) {
    int len = strlen(string);
    vehicle_str->data = (uint8_t*) malloc(len);
    if (vehicle_str->data == NULL) {
        return -ENOMEM;
    }
    memcpy(vehicle_str->data, string, len);
    vehicle_str->len = len;
    return 0;
}

static vehicle_prop_config_t const * vdev_list_properties(vehicle_hw_device_t* device UNUSED,
        int* num_properties) {
    ALOGD("vdev_list_properties.");

    *num_properties = sizeof(CONFIGS) / sizeof(vehicle_prop_config_t);
    return CONFIGS;
}

static int vdev_init(vehicle_hw_device_t* device,
                     vehicle_event_callback_fn event_callback_fn,
                     vehicle_error_callback_fn error_callback_fn) {
    ALOGD("vdev_init.");
    vehicle_device_impl_t* impl = (vehicle_device_impl_t*)device;
    pthread_mutex_lock(&lock_);
    if (impl->initialized_) {
        ALOGE("vdev_init: Callback and Error functions are already existing.");
        pthread_mutex_unlock(&lock_);
        return -EEXIST;
    }

    impl->initialized_ = 1;
    impl->event_fn_ = event_callback_fn;
    impl->error_fn_ = error_callback_fn;
    pthread_mutex_unlock(&lock_);
    return 0;
}

static int vdev_release(vehicle_hw_device_t* device) {
    vehicle_device_impl_t* impl = (vehicle_device_impl_t*)device;
    pthread_mutex_lock(&lock_);
    if (!impl->initialized_) {
        ALOGD("vdev_release: Already released before, returning early.");
    } else {
        // unsubscribe_all()
        impl->initialized_ = 0;
    }
    pthread_mutex_unlock(&lock_);
    return 0;
}

static int vdev_get(vehicle_hw_device_t* device UNUSED, vehicle_prop_value_t* data) {
    ALOGD("vdev_get.");
    //TODO all data supporting read should support get
    if (!data) {
        ALOGE("vdev_get: Data cannot be null.");
        return -EINVAL;
    }
    vehicle_prop_config_t* config = find_config(data->prop);
    if (config == NULL) {
        ALOGE("vdev_get: cannot find config 0x%x", data->prop);
        return -EINVAL;
    }
    data->value_type = config->value_type;
    // for STATIC type, time can be just 0 instead
    data->timestamp = elapsedRealtimeNano();
    int r;
    switch (data->prop) {
        case VEHICLE_PROPERTY_INFO_MAKE:
            r = alloc_vehicle_str_from_cstr(VEHICLE_MAKE, &(data->value.str_value));
            if (r != 0) {
                ALOGE("vdev_get: alloc failed");
                return r;
            }
            break;

        case VEHICLE_PROPERTY_RADIO_PRESET: {
              int radio_preset = data->value.int32_array[0];
              if (radio_preset < VEHICLE_RADIO_PRESET_MIN_VALUE ||
                  radio_preset >= RADIO_PRESET_NUM) {
                  ALOGE("%s Invalid radio preset: %d\n", __func__, radio_preset);
                  return -1;
              }
              ALOGD("%s Radio Preset number: %d", __func__, radio_preset);
              int32_t selector = radio_preset % 2 == 0;
              // Populate the channel and subchannel to be some variation of the
              // preset number for mocking.

              // Restore the preset number.
              data->value.int32_array[0] = radio_preset;
              // Channel type values taken from
              // system/core/include/system/radio.h
              data->value.int32_array[1] = selector ? RADIO_BAND_FM : RADIO_BAND_AM;
              // For FM set a value in Mhz and for AM set a value in Khz range
              // (channel).
              data->value.int32_array[2] = selector ? 99000000 : 100000;
              // For FM we have a sub-channel and we care about it, for AM pass
              // a dummy value.
              data->value.int32_array[3] = selector ? radio_preset : -1;
              break;
        }

        default:
            // actual implementation will be much complex than this. It should track proper last
            // state. Here just fill with zero.
            memset(&(data->value), 0, sizeof(data->value));
            break;
    }
    ALOGI("vdev_get, type 0x%x, time %" PRId64 ", value_type %d", data->prop, data->timestamp,
            data->value_type);
    return 0;
}

static void vdev_release_memory_from_get(struct vehicle_hw_device* device UNUSED,
        vehicle_prop_value_t *data) {
    switch (data->value_type) {
        case VEHICLE_VALUE_TYPE_STRING:
        case VEHICLE_VALUE_TYPE_BYTES:
            free(data->value.str_value.data);
            data->value.str_value.data = NULL;
            break;
        default:
            ALOGW("release_memory_from_get for property 0x%x which is not string or bytes type 0x%x"
                    , data->prop, data->value_type);
            break;
    }
}

static int vdev_set(vehicle_hw_device_t* device UNUSED, const vehicle_prop_value_t* data) {
    ALOGD("vdev_set.");
    // Just print what data will be setting here.
    ALOGD("Setting property %d with value type %d\n", data->prop, data->value_type);
    vehicle_prop_config_t* config = find_config(data->prop);
    if (config == NULL) {
        ALOGE("vdev_set: cannot find config 0x%x", data->prop);
        return -EINVAL;
    }
    if (config->value_type != data->value_type) {
        ALOGE("vdev_set: type mismatch, passed 0x%x expecting 0x%x", data->value_type,
                config->value_type);
        return -EINVAL;
    }
    switch (data->value_type) {
        case VEHICLE_VALUE_TYPE_FLOAT:
            ALOGD("Value type: FLOAT\nValue: %f\n", data->value.float_value);
            break;
        case VEHICLE_VALUE_TYPE_INT32:
            ALOGD("Value type: INT32\nValue: %d\n", data->value.int32_value);
            break;
        case VEHICLE_VALUE_TYPE_INT64:
            ALOGD("Value type: INT64\nValue: %lld\n", data->value.int64_value);
            break;
        case VEHICLE_VALUE_TYPE_BOOLEAN:
            ALOGD("Value type: BOOLEAN\nValue: %d\n", data->value.boolean_value);
            break;
        case VEHICLE_VALUE_TYPE_STRING:
            ALOGD("Value type: STRING\n Size: %d\n", data->value.str_value.len);
            // NOTE: We only handle ASCII strings here.
            // Print the UTF-8 string.
            char *ascii_out = (char *) malloc ((data->value.str_value.len + 1) * sizeof (char));
            memcpy(ascii_out, data->value.str_value.data, data->value.str_value.len);
            ascii_out[data->value.str_value.len] = '\0';
            ALOGD("Value: %s\n", ascii_out);
            break;
        case VEHICLE_VALUE_TYPE_INT32_VEC4:
            ALOGD("Value type: INT32_VEC4\nValue[0]: %d Value[1] %d Value[2] %d Value[3] %d",
                  data->value.int32_array[0], data->value.int32_array[1],
                  data->value.int32_array[2], data->value.int32_array[3]);
            break;
        default:
            ALOGD("Value type not yet handled: %d.\n", data->value_type);
    }
    return 0;
}

void print_subscribe_info(vehicle_device_impl_t* impl UNUSED) {
    unsigned int i;
    for (i = 0; i < sizeof(CONFIGS) / sizeof(vehicle_prop_config_t); i++) {
        subscription_t* sub = (subscription_t*)CONFIGS[i].hal_data;
        if (sub != NULL) {
            ALOGD("prop: %d rate: %f", sub->prop, sub->sample_rate);
        }
    }
}

// This should be run in a separate thread always.
void fake_event_thread(struct subscription *sub) {
    if (!sub) {
        ALOGE("oops! subscription object cannot be NULL.");
        exit(-1);
    }
    prctl(PR_SET_NAME, (unsigned long)sub->name, 0, 0, 0);
    // Emit values in a loop, every 2 seconds.
    while (1) {
        // Create a random value depending on the property type.
        vehicle_prop_value_t event;
        event.prop = sub->prop;
        event.timestamp = elapsedRealtimeNano();
        switch (sub->prop) {
            case VEHICLE_PROPERTY_DRIVING_STATUS:
                event.value_type = VEHICLE_VALUE_TYPE_INT32;
                switch ((event.timestamp & 0x30000000)>>28) {
                    case 0:
                        event.value.driving_status = VEHICLE_DRIVING_STATUS_UNRESTRICTED;
                        break;
                    case 1:
                        event.value.driving_status = VEHICLE_DRIVING_STATUS_NO_VIDEO;
                        break;
                    case 2:
                        event.value.driving_status = VEHICLE_DRIVING_STATUS_NO_KEYBOARD_INPUT;
                        break;
                    default:
                        event.value.driving_status = VEHICLE_DRIVING_STATUS_NO_CONFIG;
                }
                break;
            case VEHICLE_PROPERTY_GEAR_SELECTION:
                event.value_type = VEHICLE_VALUE_TYPE_INT32;
                switch ((event.timestamp & 0x30000000)>>28) {
                    case 0:
                        event.value.gear_selection = VEHICLE_GEAR_PARK;
                        break;
                    case 1:
                        event.value.gear_selection = VEHICLE_GEAR_NEUTRAL;
                        break;
                    case 2:
                        event.value.gear_selection = VEHICLE_GEAR_DRIVE;
                        break;
                    case 3:
                        event.value.gear_selection = VEHICLE_GEAR_REVERSE;
                        break;
                }
                break;
            case VEHICLE_PROPERTY_PARKING_BRAKE_ON:
                event.value_type = VEHICLE_VALUE_TYPE_BOOLEAN;
                if (event.timestamp & 0x20000000) {
                    event.value.parking_brake = VEHICLE_FALSE;
                } else {
                    event.value.parking_brake = VEHICLE_TRUE;
                }
                break;
            case VEHICLE_PROPERTY_PERF_VEHICLE_SPEED:
                event.value_type = VEHICLE_VALUE_TYPE_FLOAT;
                event.value.vehicle_speed = (float) ((event.timestamp & 0xff000000)>>24);
                break;
            case VEHICLE_PROPERTY_RADIO_PRESET:
                event.value_type = VEHICLE_VALUE_TYPE_INT32_VEC4;
                int presetInfo1[4] = {1  /* preset number */, 0  /* AM Band */, 1000, 0};
                int presetInfo2[4] = {2  /* preset number */, 1  /* FM Band */, 1000, 0};
                if (event.timestamp & 0x20000000) {
                    memcpy(event.value.int32_array, presetInfo1, sizeof(presetInfo1));
                } else {
                    memcpy(event.value.int32_array, presetInfo2, sizeof(presetInfo2));
                }
                break;
            default: // unsupported
                if (sub->impl == NULL) {
                    ALOGE("subscription impl NULL");
                    return;
                }
                if (sub->impl->error_fn_ != NULL) {
                    sub->impl->error_fn_(-EINVAL, VEHICLE_PROPERTY_INVALID,
                            VEHICLE_OPERATION_GENERIC);
                } else {
                    ALOGE("Error function is null");
                }
                ALOGE("Unsupported prop 0x%x, quit", sub->prop);
                return;
        }
        if (sub->impl->event_fn_ != NULL) {
            sub->impl->event_fn_(&event);
        } else {
            ALOGE("Event function is null");
            return;
        }
        pthread_mutex_lock(&sub->lock);
        if (sub->stop_thread) {
            ALOGD("exiting subscription request here.");
            // Do any cleanup here.
            pthread_mutex_unlock(&sub->lock);
            return;
        }
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        now.tv_sec += 1; // sleep for one sec
        pthread_cond_timedwait(&sub->cond, &sub->lock, &now);
        pthread_mutex_unlock(&sub->lock);
    }
}

static int vdev_subscribe(vehicle_hw_device_t* device, int32_t prop, float sample_rate,
        int32_t zones UNUSED) {
    ALOGD("vdev_subscribe 0x%x, %f", prop, sample_rate);
    vehicle_device_impl_t* impl = (vehicle_device_impl_t*)device;
    // Check that the device is initialized.
    pthread_mutex_lock(&lock_);
    if (!impl->initialized_) {
        pthread_mutex_unlock(&lock_);
        ALOGE("vdev_subscribe: have you called init()?");
        return -EINVAL;
    }
    vehicle_prop_config_t* config = find_config(prop);
    if (config == NULL) {
        pthread_mutex_unlock(&lock_);
        ALOGE("vdev_subscribe not supported property 0x%x", prop);
        return -EINVAL;
    }
    if ((config->access != VEHICLE_PROP_ACCESS_READ) &&
        (config->access != VEHICLE_PROP_ACCESS_READ_WRITE)) {
        pthread_mutex_unlock(&lock_);
        ALOGE("vdev_subscribe read not supported on the property 0x%x", prop);
        return -EINVAL;
    }
    if (config->change_mode == VEHICLE_PROP_CHANGE_MODE_STATIC) {
        pthread_mutex_unlock(&lock_);
        ALOGE("vdev_subscribe cannot subscribe static property 0x%x", prop);
        return -EINVAL;
    }
    if ((config->change_mode == VEHICLE_PROP_CHANGE_MODE_ON_CHANGE) && (sample_rate != 0)) {
        pthread_mutex_unlock(&lock_);
        ALOGE("vdev_subscribe on change type should have 0 sample rate, property 0x%x, sample rate %f",
                prop, sample_rate);
        return -EINVAL;
    }
    if ((config->max_sample_rate < sample_rate) || (config->min_sample_rate > sample_rate)) {

        ALOGE("vdev_subscribe property 0x%x, invalid sample rate %f, min:%f, max:%f",
                prop, sample_rate, config->min_sample_rate, config->max_sample_rate);
        pthread_mutex_unlock(&lock_);
        return -EINVAL;
    }
    subscription_t* sub = (subscription_t*)config->hal_data;
    if (sub == NULL) {
        sub = calloc(1, sizeof(subscription_t));
        sub->prop = prop;
        sub->sample_rate = sample_rate;
        sub->stop_thread = 0;
        sub->impl = impl;
        pthread_mutex_init(&sub->lock, NULL);
        pthread_cond_init(&sub->cond, NULL);
        config->hal_data = sub;
        sprintf(sub->name, "vhal0x%x", prop);
    } else if (sub->sample_rate != sample_rate){ // sample rate changed
        //TODO notify this to fake sensor thread
        sub->sample_rate = sample_rate;
        pthread_mutex_unlock(&lock_);
        return 0;
    }
    int ret_code = pthread_create(
                                  &sub->thread, NULL, (void *(*)(void*))fake_event_thread, sub);
    print_subscribe_info(impl);
    pthread_mutex_unlock(&lock_);
    return 0;
}

static int vdev_unsubscribe(vehicle_hw_device_t* device, int32_t prop) {
    ALOGD("vdev_unsubscribe 0x%x", prop);
    vehicle_device_impl_t* impl = (vehicle_device_impl_t*)device;
    pthread_mutex_lock(&lock_);
    vehicle_prop_config_t* config = find_config(prop);
    if (config == NULL) {
        pthread_mutex_unlock(&lock_);
        return -EINVAL;
    }
    subscription_t* sub = (subscription_t*)config->hal_data;
    if (sub == NULL) {
        pthread_mutex_unlock(&lock_);
        return -EINVAL;
    }
    config->hal_data = NULL;
    pthread_mutex_unlock(&lock_);
    pthread_mutex_lock(&sub->lock);
    sub->stop_thread = 1;
    pthread_cond_signal(&sub->cond);
    pthread_mutex_unlock(&sub->lock);
    pthread_join(sub->thread, NULL);
    pthread_cond_destroy(&sub->cond);
    pthread_mutex_destroy(&sub->lock);
    free(sub);
    pthread_mutex_lock(&lock_);
    print_subscribe_info(impl);
    pthread_mutex_unlock(&lock_);
    return 0;
}

static int vdev_close(hw_device_t* device) {
    vehicle_device_impl_t* impl = (vehicle_device_impl_t*)device;
    if (impl) {
        free(impl);
        return 0;
    } else {
        return -1;
    }
}

static int vdev_dump(struct vehicle_hw_device* device UNUSED, int fd UNUSED) {
    //TODO
    return 0;
}

/*
 * The open function is provided as an interface in harwdare.h which fills in
 * all the information about specific implementations and version specific
 * informations in hw_device_t structure. After calling open() the client should
 * use the hw_device_t to execute any Vehicle HAL device specific functions.
 */
static int vdev_open(const hw_module_t* module, const char* name UNUSED,
                     hw_device_t** device) {
    ALOGD("vdev_open");

    // Oops, out of memory!
    vehicle_device_impl_t* vdev = calloc(1, sizeof(vehicle_device_impl_t));
    if (vdev == NULL) {
        return -ENOMEM;
    }

    // Common functions provided by harware.h to access module and device(s).
    vdev->vehicle_device.common.tag = HARDWARE_DEVICE_TAG;
    vdev->vehicle_device.common.version = VEHICLE_DEVICE_API_VERSION_1_0;
    vdev->vehicle_device.common.module = (hw_module_t *) module;
    vdev->vehicle_device.common.close = vdev_close;

    // Define the Vehicle HAL device specific functions.
    vdev->vehicle_device.list_properties = vdev_list_properties;
    vdev->vehicle_device.init = vdev_init;
    vdev->vehicle_device.release = vdev_release;
    vdev->vehicle_device.get = vdev_get;
    vdev->vehicle_device.release_memory_from_get = vdev_release_memory_from_get;
    vdev->vehicle_device.set = vdev_set;
    vdev->vehicle_device.subscribe = vdev_subscribe;
    vdev->vehicle_device.unsubscribe = vdev_unsubscribe;
    vdev->vehicle_device.dump = vdev_dump;

    *device = (hw_device_t *) vdev;
    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = vdev_open,
};

/*
 * This structure is mandatory to be implemented by each HAL implementation. It
 * exposes the open method (see hw_module_methods_t above) which opens a device.
 * The vehicle HAL is supposed to be used as a single device HAL hence all the
 * functions should be implemented inside of the vehicle_hw_device_t struct (see
 * the vehicle.h in include/ folder.
 */
vehicle_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = VEHICLE_MODULE_API_VERSION_1_0,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = VEHICLE_HARDWARE_MODULE_ID,
        .name = "Default vehicle HW HAL",
        .author = "",
        .methods = &hal_module_methods,
    },
};
