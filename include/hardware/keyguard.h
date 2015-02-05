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

#ifndef ANDROID_HARDWARE_KEYGUARD_H
#define ANDROID_HARDWARE_KEYGUARD_H

#include <sys/cdefs.h>
#include <sys/types.h>
#include <hardware/hardware.h>

__BEGIN_DECLS

#define KEYGUARD_HARDWARE_MODULE_ID "keyguard"

#define KEYGUARD_MODULE_API_VERSION_0_1 HARDWARE_MODULE_API_VERSION(0, 1)

#define HARDWARE_KEYGUARD "keyguard"

struct keyguard_module {
    /**
     * Comon methods of the keyguard module. This *must* be the first member of
     * keyguard_module as users of this structure will cast a hw_module_t to
     * a keyguard_module pointer in the appropriate context.
     */
    hw_module_t common;
};

struct keyguard_device {
    /**
     * Common methods of the keyguard device. As above, this must be the first
     * member of keymaster_device.
     */
    hw_device_t common;

    /**
     * Enrolls password_payload, which should be derived from a user selected pin or password,
     * with the authentication factor private key used only for enrolling authentication
     * factor data.
     *
     * Returns: 0 on success or an error code less than 0 on error.
     * On error, enrolled_password_handle will not be allocated.
     */
    int (*enroll)(const struct keyguard_device *dev, uint32_t uid,
            const uint8_t *password_payload, size_t password_payload_length,
            uint8_t **enrolled_password_handle, size_t *enrolled_password_handle_length);

    /**
     * Verifies provided_password matches enrolled_password_handle.
     *
     * Implementations of this module may retain the result of this call
     * to attest to the recency of authentication.
     *
     * On success, writes the address of a verification token to verification_token,
     * usable to attest password verification to other trusted services. Clients
     * may pass NULL for this value.
     *
     * Returns: 0 on success or an error code less than 0 on error
     * On error, verification token will not be allocated
     */
    int (*verify)(const struct keyguard_device *dev, uint32_t uid,
            const uint8_t *enrolled_password_handle, size_t enrolled_password_handle_length,
            const uint8_t *provided_password, size_t provided_password_length,
            uint8_t **verification_token, size_t *verification_token_length);

};
typedef struct keyguard_device keyguard_device_t;

static inline int keyguard_open(const struct hw_module_t *module,
        keyguard_device_t **device) {
    return module->methods->open(module, HARDWARE_KEYGUARD,
            (struct hw_device_t **) device);
}

static inline int keyguard_close(keyguard_device_t *device) {
    return device->common.close(&device->common);
}

__END_DECLS

#endif // ANDROID_HARDWARE_KEYGUARD_H
