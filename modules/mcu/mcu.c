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
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define LOG_TAG "Legacy MCU HAL"
#include <utils/Log.h>

#include <hardware/hardware.h>
#include <hardware/mcu.h>

static int mcu_init(struct mcu_module *module)
{
    return 0;
}

static int mcu_send_message(struct mcu_module *module, const char *msg,
                            const void *arg, size_t arg_len, void **result,
                            size_t *result_len)
{
    return 0;
}

static struct hw_module_methods_t mcu_module_methods = {
    .open = NULL,
};

struct mcu_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = MCU_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = MCU_HARDWARE_MODULE_ID,
        .name = "Default MCU HAL",
        .author = "The Android Open Source Project",
        .methods = &mcu_module_methods,
    },

    .init = mcu_init,
    .sendMessage = mcu_send_message,
};
