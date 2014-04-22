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

typedef enum fingerprint_msg_type {
    FINGERPRINT_ERROR = -1,
    FINGERPRINT_SCANNED = 1,
    FINGERPRINT_TEMPLATE_COLLECTING = 2,
    FINGERPRINT_TEMPLATE_DELETED = 4
} fingerprint_msg_type_t;

typedef enum fingerprint_error {
    FINGERPRINT_ERROR_HW_UNAVAILABLE = 1,
    FINGERPRINT_ERROR_BAD_CAPTURE = 2,
    FINGERPRINT_ERROR_TIMEOUT = 3
} fingerprint_error_t;

typedef struct fingerprint_enroll {
    uint32_t id;
    /* samples_remaining goes form N (no data collected, but N scans needed)
     * to 0 (no more data is needed to build a template)
     * If HAL fails to decrement samples_remaining between calls the client
     * will declare template collection a failure and should abort the operation
     * by calling fingerprint_close() */
    uint32_t samples_remaining;
} fingerprint_enroll_t;

typedef struct fingerprint_scanned {
    uint32_t id; /* 0 is a special id and means no match */
    uint32_t confidence; /* Goes form 0 (no match) to 0xffffFFFF (100% sure) */
} fingerprint_ident_t;

typedef struct fingerprint_msg {
    fingerprint_msg_type_t type;
    union {
        uint64_t raw;
        fingerprint_error_t error;
        fingerprint_enroll_t enroll;
        fingerprint_ident_t ident;
    } data;
} fingerprint_msg_t;

/* Callback function type */
typedef void (*fingerprint_notify_t)(fingerprint_msg_t msg);

/* Synchronous operation */
typedef struct fingerprint_device {
    struct hw_device_t common;

    /*
     * Figerprint enroll request:
     * Switches the HAL state machine to collect and store a new fingerprint
     * template. Switches back as soon as enroll is complete
     * (fingerprint_msg.type == FINGERPRINT_TEMPLATE_COLLECTING &&
     *  fingerprint_msg.data.enroll.samples_remaining == 0)
     * or after temeout_sec seconds.
     *
     * Function return: 0 if enrollment process can be successfully started
     *                 -1 otherwise.
     */
    int (*enroll)(struct fingerprint_device *dev, unsigned timeout_sec);

    /*
     * Figerprint remove request:
     * deletes a fingerprint template.
     * If the fingerprint id is 0 the entire template database will be removed.
     *
     * Function return: 0 if fingerprint template can be successfully deleted
     *                 -1 otherwise.
     */
    int (*remove)(struct fingerprint_device *dev, uint32_t fingerprint_id);

    /*
     * Set notification callback:
     * Registers a user function that would receive notifications from the HAL
     * The call will block if the HAL state machine is in busy state until HAL
     * leaves the busy state.
     *
     * Function return: 0 if callback function is successfuly registered
     *                 -1 otherwise.
     */
    int (*set_notify)(struct fingerprint_device *dev,
                        fingerprint_notify_t notify);

    /*
     * Client provided callback function to receive notifications.
     * Do not set by hand, use the function above instead.
     */
    fingerprint_notify_t notify;

    /* Reserved for future use. Must be NULL. */
    void* reserved[8 - 4];
} fingerprint_device_t;

typedef struct fingerprint_module {
    struct hw_module_t common;
} fingerprint_module_t;

#endif  /* ANDROID_INCLUDE_HARDWARE_FINGERPRINT_H */
