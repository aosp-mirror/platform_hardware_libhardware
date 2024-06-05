/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include <system/audio.h>
#include <system/sound_trigger.h>
#include <hardware/hardware.h>

#ifndef ANDROID_SOUND_TRIGGER_HAL_H
#define ANDROID_SOUND_TRIGGER_HAL_H


__BEGIN_DECLS

/**
 * The id of this module
 */
#define SOUND_TRIGGER_HARDWARE_MODULE_ID "sound_trigger"

/**
 * Name of the audio devices to open
 */
#define SOUND_TRIGGER_HARDWARE_INTERFACE "sound_trigger_hw_if"

#define SOUND_TRIGGER_MODULE_API_VERSION_1_0 HARDWARE_MODULE_API_VERSION(1, 0)
#define SOUND_TRIGGER_MODULE_API_VERSION_CURRENT SOUND_TRIGGER_MODULE_API_VERSION_1_0


#define SOUND_TRIGGER_DEVICE_API_VERSION_1_0 HARDWARE_DEVICE_API_VERSION(1, 0)
#define SOUND_TRIGGER_DEVICE_API_VERSION_1_1 HARDWARE_DEVICE_API_VERSION(1, 1)
#define SOUND_TRIGGER_DEVICE_API_VERSION_1_2 HARDWARE_DEVICE_API_VERSION(1, 2)
#define SOUND_TRIGGER_DEVICE_API_VERSION_1_3 HARDWARE_DEVICE_API_VERSION(1, 3)
#define SOUND_TRIGGER_DEVICE_API_VERSION_CURRENT SOUND_TRIGGER_DEVICE_API_VERSION_1_3

/**
 * List of known sound trigger HAL modules. This is the base name of the sound_trigger HAL
 * library composed of the "sound_trigger." prefix, one of the base names below and
 * a suffix specific to the device.
 * e.g: sondtrigger.primary.goldfish.so or sound_trigger.primary.default.so
 */

#define SOUND_TRIGGER_HARDWARE_MODULE_ID_PRIMARY "primary"


/**
 * Every hardware module must have a data structure named HAL_MODULE_INFO_SYM
 * and the fields of this data structure must begin with hw_module_t
 * followed by module specific information.
 */
struct sound_trigger_module {
    struct hw_module_t common;
};

typedef void (*recognition_callback_t)(struct sound_trigger_recognition_event *event, void *cookie);
typedef void (*sound_model_callback_t)(struct sound_trigger_model_event *event, void *cookie);

struct sound_trigger_hw_device {
    struct hw_device_t common;

    /*
     * Retrieve implementation properties.
     */
    int (*get_properties)(const struct sound_trigger_hw_device *dev,
                          struct sound_trigger_properties *properties);

    /*
     * Load a sound model. Once loaded, recognition of this model can be started and stopped.
     * Only one active recognition per model at a time. The SoundTrigger service will handle
     * concurrent recognition requests by different users/applications on the same model.
     * The implementation returns a unique handle used by other functions (unload_sound_model(),
     * start_recognition(), etc...
     */
    int (*load_sound_model)(const struct sound_trigger_hw_device *dev,
                            struct sound_trigger_sound_model *sound_model,
                            sound_model_callback_t callback,
                            void *cookie,
                            sound_model_handle_t *handle);

    /*
     * Unload a sound model. A sound model can be unloaded to make room for a new one to overcome
     * implementation limitations.
     */
    int (*unload_sound_model)(const struct sound_trigger_hw_device *dev,
                              sound_model_handle_t handle);

    /* Start recognition on a given model. Only one recognition active at a time per model.
     * Once recognition succeeds of fails, the callback is called.
     * TODO: group recognition configuration parameters into one struct and add key phrase options.
     */
    int (*start_recognition)(const struct sound_trigger_hw_device *dev,
                             sound_model_handle_t sound_model_handle,
                             const struct sound_trigger_recognition_config *config,
                             recognition_callback_t callback,
                             void *cookie);

    /* Stop recognition on a given model.
     * The implementation does not have to call the callback when stopped via this method.
     */
    int (*stop_recognition)(const struct sound_trigger_hw_device *dev,
                            sound_model_handle_t sound_model_handle);

    /* Stop recognition on all models.
     * Only supported for device api versions SOUND_TRIGGER_DEVICE_API_VERSION_1_1 or above.
     * If no implementation is provided, stop_recognition will be called for each running model.
     */
    int (*stop_all_recognitions)(const struct sound_trigger_hw_device* dev);

    /* Get the current state of a given model.
     * The state will be returned as a recognition event, via the callback that was registered
     * in the start_recognition method.
     * Only supported for device api versions SOUND_TRIGGER_DEVICE_API_VERSION_1_2 or above.
     */
    int (*get_model_state)(const struct sound_trigger_hw_device *dev,
                           sound_model_handle_t sound_model_handle);

    /* Set a model specific ModelParameter with the given value. This parameter
     * will keep its value for the duration the model is loaded regardless of starting and stopping
     * recognition. Once the model is unloaded, the value will be lost.
     * Returns 0 or an error code.
     * Only supported for device api versions SOUND_TRIGGER_DEVICE_API_VERSION_1_3 or above.
     */
    int (*set_parameter)(const struct sound_trigger_hw_device *dev,
                           sound_model_handle_t sound_model_handle,
                           sound_trigger_model_parameter_t model_param, int32_t value);

    /* Get a model specific ModelParameter. This parameter will keep its value
     * for the duration the model is loaded regardless of starting and stopping recognition.
     * Once the model is unloaded, the value will be lost. If the value is not set, a default
     * value is returned. See sound_trigger_model_parameter_t for parameter default values.
     * Returns 0 or an error code. On return 0, value pointer will be set.
     * Only supported for device api versions SOUND_TRIGGER_DEVICE_API_VERSION_1_3 or above.
     */
    int (*get_parameter)(const struct sound_trigger_hw_device *dev,
                           sound_model_handle_t sound_model_handle,
                           sound_trigger_model_parameter_t model_param, int32_t* value);

    /* Get supported parameter attributes with respect to the provided model
     * handle. Along with determining the valid range, this API is also used
     * to determine if a given parameter ID is supported at all by the
     * modelHandle for use with getParameter and setParameter APIs.
     * Only supported for device api versions SOUND_TRIGGER_DEVICE_API_VERSION_1_3 or above.
     */
    int (*query_parameter)(const struct sound_trigger_hw_device *dev,
                           sound_model_handle_t sound_model_handle,
                           sound_trigger_model_parameter_t model_param,
                           sound_trigger_model_parameter_range_t* param_range);

    /*
     * Retrieve verbose extended implementation properties.
     * The header pointer is intented to be cast to the proper extended
     * properties struct based on the header version.
     * The returned pointer is valid throughout the lifetime of the driver.
     * Only supported for device api versions SOUND_TRIGGER_DEVICE_API_VERSION_1_3 or above.
     */
    const struct sound_trigger_properties_header* (*get_properties_extended)
            (const struct sound_trigger_hw_device *dev);

    /* Start recognition on a given model. Only one recognition active at a time per model.
     * Once recognition succeeds of fails, the callback is called.
     * Recognition API includes extended config fields. The header is intended to be base to
     * the proper config struct based on the header version.
     * Only supported for device api versions SOUND_TRIGGER_DEVICE_API_VERSION_1_3 or above.
     */
    int (*start_recognition_extended)(const struct sound_trigger_hw_device *dev,
                             sound_model_handle_t sound_model_handle,
                             const struct sound_trigger_recognition_config_header *header,
                             recognition_callback_t callback,
                             void *cookie);
};

typedef struct sound_trigger_hw_device sound_trigger_hw_device_t;

/** convenience API for opening and closing a supported device */

static inline int sound_trigger_hw_device_open(const struct hw_module_t* module,
                                       struct sound_trigger_hw_device** device)
{
    return module->methods->open(module, SOUND_TRIGGER_HARDWARE_INTERFACE,
                                 TO_HW_DEVICE_T_OPEN(device));
}

static inline int sound_trigger_hw_device_close(struct sound_trigger_hw_device* device)
{
    return device->common.close(&device->common);
}

__END_DECLS

#endif  // ANDROID_SOUND_TRIGGER_HAL_H
