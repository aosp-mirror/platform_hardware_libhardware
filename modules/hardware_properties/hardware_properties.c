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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG "HardwarePropertiesHAL"
#include <utils/Log.h>

#include <hardware/hardware.h>
#include <hardware/hardware_properties.h>

static ssize_t get_device_temperatures(
    struct hardware_properties_module *module, float **temps) {
    *temps = NULL;
    errno = ENOSYS;
    ALOGE("getDeviceTemperatures: %s", strerror(errno));
    return -1;
}

static ssize_t get_cpu_usages(struct hardware_properties_module *module,
                              int64_t **active_times, int64_t **total_times) {
    *active_times = NULL;
    *total_times = NULL;
    errno = ENOSYS;
    ALOGE("getCpuUsages: %s", strerror(errno));
    return -1;
}

static ssize_t get_fan_speeds(struct hardware_properties_module *module,
                              float **fan_speeds) {
    *fan_speeds = NULL;
    errno = ENOSYS;
    ALOGE("getFanSpeeds: %s", strerror(errno));
    return -1;
}

static struct hw_module_methods_t hardware_properties_module_methods = {
    .open = NULL,
};

struct hardware_properties_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = HARDWARE_PROPERTIES_HARDWARE_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = HARDWARE_PROPERTIES_HARDWARE_MODULE_ID,
        .name = "Default Hardware Properties HAL",
        .author = "The Android Open Source Project",
        .methods = &hardware_properties_module_methods,
    },

    .getCpuTemperatures = get_device_temperatures,
    .getGpuTemperatures = get_device_temperatures,
    .getBatteryTemperatures = get_device_temperatures,
    .getCpuUsages = get_cpu_usages,
    .getFanSpeeds = get_fan_speeds,
};
