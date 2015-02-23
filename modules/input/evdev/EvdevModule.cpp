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

#define LOG_NDEBUG 0
#define LOG_TAG "EvdevModule"

#include <assert.h>
#include <hardware/hardware.h>
#include <hardware/input.h>

namespace input {

extern "C" {

static int dummy_open(const hw_module_t __unused *module, const char __unused *id,
                            hw_device_t __unused **device) {
    assert(false);
    return 0;
}

static void input_init(const input_module_t* module,
        input_host_t* host, input_host_callbacks_t cb) {
    return;
}

static void input_notify_report(input_report_t* r) {
    return;
}

static struct hw_module_methods_t input_module_methods = {
    .open = dummy_open,
};

input_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag                = HARDWARE_MODULE_TAG,
        .module_api_version = INPUT_MODULE_API_VERSION_1_0,
        .hal_api_version    = HARDWARE_HAL_API_VERSION,
        .id                 = INPUT_HARDWARE_MODULE_ID,
        .name               = "Input evdev HAL",
        .author             = "The Android Open Source Project",
        .methods            = &input_module_methods,
        .dso                = NULL,
        .reserved           = {0},
    },

    .init = input_init,
    .notify_report = input_notify_report,
};

}  // extern "C"

}  // namespace input
