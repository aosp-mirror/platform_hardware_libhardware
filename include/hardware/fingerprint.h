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

#ifndef ANDROID_INCLUDE_HARDWARE_FINGERPRINT_H
#define ANDROID_INCLUDE_HARDWARE_FINGERPRINT_H

#define FINGERPRINT_MODULE_API_VERSION_1_0 HARDWARE_MODULE_API_VERSION(1, 0)
#define FINGERPRINT_HARDWARE_MODULE_ID "fingerprint"

typedef enum fingerprint_msg {
    FINGERPRINT_ERROR = -1,
    FINGERPRINT_NO_MATCH = 0,
    FINGERPRINT_MATCH = 1,
    FINGERPRINT_TEMPLATE_COLLECTING = 2,
    FINGERPRINT_TEMPLATE_REGISTERED = 3,
    FINGERPRINT_TEMPLATE_DELETED = 4
} fingerprint_msg_t;

typedef enum fingerprint_error {
    FINGERPRINT_ERROR_HW_UNAVAILABLE = 1,
    FINGERPRINT_ERROR_BAD_CAPTURE = 2
} fingerprint_error_t;

/* Synchronous operation */
typedef struct fingerprint_device {
    struct hw_device_t common;

    /*
     * Figerprint enroll request: records and stores a fingerprint template.
     * Timeout after temeout_sec seconds.
     *
     * Function return:
     *   - Machine state: error, collected or registered.
     *   - Data is interpreted as error code, collection percentage
     *     or fingerprint id.
     */
    fingerprint_msg_t (*enroll)(unsigned timeout_sec, unsigned *data);

    /*
     * Figerprint remove request: deletes a fingerprint template.
     *
     * Function return:
     *   - Delete result: error or success.
     */
    fingerprint_msg_t (*remove)(unsigned fingerprint_id);

    /*
     * Figerprint match request: Collect a fingerprint and
     * match against stored template with fingerprint_id.
     * Timeout after temeout_sec seconds.
     *
     * Function return:
     *   - Match, no match or error.
     */
    fingerprint_msg_t (*match)(unsigned fingerprint_id, unsigned timeout_sec);

    /* Reserved for future use. Must be NULL. */
    void* reserved[8 - 3];
} fingerprint_device_t;

typedef struct fingerprint_module {
    struct hw_module_t common;
} fingerprint_module_t;

/* For asyncronous mode - as a possible API model
typedef void (*fingerprint_callback)(int request_id, fingerprint_msg msg, data);
*/

#endif  /* ANDROID_INCLUDE_HARDWARE_FINGERPRINT_H */
