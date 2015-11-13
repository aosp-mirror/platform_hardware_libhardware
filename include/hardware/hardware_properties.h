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

#ifndef ANDROID_INCLUDE_HARDWARE_HARDWAREPROPERTIES_H
#define ANDROID_INCLUDE_HARDWARE_HARDWAREPROPERTIES_H

#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#include <hardware/hardware.h>

__BEGIN_DECLS

#define HARDWARE_PROPERTIES_HARDWARE_MODULE_API_VERSION_0_1 HARDWARE_MODULE_API_VERSION(0, 1)

/**
 * The id of this module
 */
#define HARDWARE_PROPERTIES_HARDWARE_MODULE_ID "hardware_properties"

/**
 * Every hardware module must have a data structure named HAL_MODULE_INFO_SYM
 * and the fields of this data structure must begin with hw_module_t
 * followed by module specific information.
 */
typedef struct hardware_properties_module {
    struct hw_module_t common;

    /*
     * (*getCpuTemperatures) is called to get CPU temperatures in Celsius of
     * each core.
     *
     * Returns number of cores or negative value on error.
     *
     */
    ssize_t (*getCpuTemperatures)(struct hardware_properties_module *module, float **temps);

    /*
     * (*getGpuTemperatures) is called to get GPU temperatures in Celsius of
     * each GPU.
     *
     * Returns number of GPUs or negative value on error.
     *
     */
    ssize_t (*getGpuTemperatures)(struct hardware_properties_module *module, float **temps);

    /*
     * (*getBatteryTemperatures) is called to get battery temperatures in
     * Celsius.
     *
     * Returns number of battery temperatures or negative value on error.
     *
     */
    ssize_t (*getBatteryTemperatures)(struct hardware_properties_module *module, float **temps);

    /*
     * (*getCpuUsages) is called to get CPU usage information of each core:
     * active and total times in ms since first boot.
     *
     * Returns number of cores or negative value on error.
     *
     */
    ssize_t (*getCpuUsages)(struct hardware_properties_module *module,
                            int64_t **active_times, int64_t **total_times);

    /*
     * (*getFanSpeeds) is called to get the fan speeds in RPM of each fan.
     *
     * Returns number of fans or negative value on error.
     *
     */
    ssize_t (*getFanSpeeds)(struct hardware_properties_module *module,
                        float **fan_speeds);
} hardware_properties_module_t;

__END_DECLS

#endif  // ANDROID_INCLUDE_HARDWARE_HARDWAREPROPERTIES_H
