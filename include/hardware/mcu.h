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

#ifndef ANDROID_INCLUDE_HARDWARE_MCU_H
#define ANDROID_INCLUDE_HARDWARE_MCU_H

#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#include <hardware/hardware.h>

__BEGIN_DECLS

#define MCU_MODULE_API_VERSION_0_1  HARDWARE_MODULE_API_VERSION(0, 1)

/*
 * The id of this module
 */
#define MCU_HARDWARE_MODULE_ID "mcu"

/*
 * MCU message keys passed to (*sendMessage)
 */
#define MCU_PARAMETER_MSG_ENABLE_MCU "enable_mcu"

/*
 * MCU message values passed to (*sendMessage)
 */
#define MCU_PARAMETER_ARG_ON "on"
#define MCU_PARAMETER_ARG_OFF "off"

/*
 * Every hardware module must have a data structure named HAL_MODULE_INFO_SYM
 * and the fields of this data structure must begin with hw_module_t
 * followed by module specific information.
 */
typedef struct mcu_module {
    struct hw_module_t common;

    /*
     * (*init)() performs MCU module setup actions at runtime startup, such
     * as to initialize an external MCU. This is called only by the MCU HAL
     * instance loaded by PowerManagerService.
     *
     * Returns 0 on success or -errno on error.
     */
    int (*init)(struct mcu_module *module);

    /*
     * (*sendMessage)() passes a message/argument pair to the MCU to execute
     * a function. msg is NULL-terminated. If arg is text, then arg_len must
     * reflect the string length. result is a heap-allocated buffer that the
     * caller must free. If there is no result, then *result will be NULL and
     * *result_len will be 0.
     *
     * Returns 0 on success or -errno in case of error (for example, if the
     * MCU does not support the specified message.)
     *
     */
    int (*sendMessage)(struct mcu_module *module, const char *msg,
                        const void *arg, size_t arg_len, void **result,
                        size_t *result_len);

} mcu_module_t;

__END_DECLS

#endif  // ANDROID_INCLUDE_HARDWARE_MCU_H
