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

#ifndef ANDROID_HARDWARE_GATEKEEPER_H
#define ANDROID_HARDWARE_GATEKEEPER_H

#include <sys/cdefs.h>
#include <sys/types.h>
#include <hardware/hardware.h>

__BEGIN_DECLS

#define GATEKEEPER_HARDWARE_MODULE_ID "gatekeeper"

#define GATEKEEPER_MODULE_API_VERSION_0_1 HARDWARE_MODULE_API_VERSION(0, 1)

#define HARDWARE_GATEKEEPER "gatekeeper"

struct gatekeeper_module {
    /**
     * Comon methods of the gatekeeper module. This *must* be the first member of
     * gatekeeper_module as users of this structure will cast a hw_module_t to
     * a gatekeeper_module pointer in the appropriate context.
     */
    hw_module_t common;
};

struct gatekeeper_device {
    /**
     * Common methods of the gatekeeper device. As above, this must be the first
     * member of keymaster_device.
     */
    hw_device_t common;

    /**
     * Enrolls desired_password, which should be derived from a user selected pin or password,
     * with the authentication factor private key used only for enrolling authentication
     * factor data.
     *
     * If there was already a password enrolled, it should be provided in
     * current_password_handle, along with the current password in current_password
     * that should validate against current_password_handle.
     *
     * Returns: 0 on success or an error code less than 0 on error.
     * On error, enrolled_password_handle will not be allocated.
     */
    int (*enroll)(const struct gatekeeper_device *dev, uint32_t uid,
            const uint8_t *current_password_handle, size_t current_password_handle_length,
            const uint8_t *current_password, size_t current_password_length,
            const uint8_t *desired_password, size_t desired_password_length,
            uint8_t **enrolled_password_handle, size_t *enrolled_password_handle_length);

    /**
     * Verifies provided_password matches enrolled_password_handle.
     *
     * Implementations of this module may retain the result of this call
     * to attest to the recency of authentication.
     *
     * On success, writes the address of a verification token to auth_token,
     * usable to attest password verification to other trusted services. Clients
     * may pass NULL for this value.
     *
     * Returns: 0 on success or an error code less than 0 on error
     * On error, verification token will not be allocated
     */
    int (*verify)(const struct gatekeeper_device *dev, uint32_t uid,
            const uint8_t *enrolled_password_handle, size_t enrolled_password_handle_length,
            const uint8_t *provided_password, size_t provided_password_length,
            uint8_t **auth_token, size_t *auth_token_length);

};
typedef struct gatekeeper_device gatekeeper_device_t;

static inline int gatekeeper_open(const struct hw_module_t *module,
        gatekeeper_device_t **device) {
    return module->methods->open(module, HARDWARE_GATEKEEPER,
            (struct hw_device_t **) device);
}

static inline int gatekeeper_close(gatekeeper_device_t *device) {
    return device->common.close(&device->common);
}

__END_DECLS

#endif // ANDROID_HARDWARE_GATEKEEPER_H
