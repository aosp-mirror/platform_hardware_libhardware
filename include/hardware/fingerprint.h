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

#include <hardware/hw_auth_token.h>

#define FINGERPRINT_MODULE_API_VERSION_1_0 HARDWARE_MODULE_API_VERSION(1, 0)
#define FINGERPRINT_MODULE_API_VERSION_2_0 HARDWARE_MODULE_API_VERSION(2, 0)
#define FINGERPRINT_HARDWARE_MODULE_ID "fingerprint"

typedef enum fingerprint_msg_type {
    FINGERPRINT_ERROR = -1,
    FINGERPRINT_ACQUIRED = 1,
    FINGERPRINT_TEMPLATE_ENROLLING = 3,
    FINGERPRINT_TEMPLATE_REMOVED = 4,
    FINGERPRINT_AUTHENTICATED = 5
} fingerprint_msg_type_t;

typedef enum fingerprint_error {
    FINGERPRINT_ERROR_HW_UNAVAILABLE = 1,
    FINGERPRINT_ERROR_UNABLE_TO_PROCESS = 2,
    FINGERPRINT_ERROR_TIMEOUT = 3,
    FINGERPRINT_ERROR_NO_SPACE = 4, /* No space available to store a template */
    FINGERPRINT_ERROR_CANCELED = 5,
    FINGERPRINT_ERROR_UNABLE_TO_REMOVE = 6, /* fingerprint id can't be removed */
    FINGERPRINT_ERROR_VENDOR_BASE = 1000 /* vendor-specific error messages start here */
} fingerprint_error_t;

typedef enum fingerprint_acquired_info {
    FINGERPRINT_ACQUIRED_GOOD = 0,
    FINGERPRINT_ACQUIRED_PARTIAL = 1,
    FINGERPRINT_ACQUIRED_INSUFFICIENT = 2,
    FINGERPRINT_ACQUIRED_IMAGER_DIRTY = 3,
    FINGERPRINT_ACQUIRED_TOO_SLOW = 4,
    FINGERPRINT_ACQUIRED_TOO_FAST = 5,
    FINGERPRINT_ACQUIRED_VENDOR_BASE = 1000 /* vendor-specific acquisition messages start here */
} fingerprint_acquired_info_t;

typedef struct fingerprint_finger_id {
    uint32_t gid;
    uint32_t fid;
} fingerprint_finger_id_t;

/* The progress indication may be augmented by a bitmap encoded indication
* of what finger area is considered as collected.
* Bit numbers mapped to physical location:
*
*             distal
*        +--+--+--+--+--+
*        | 4| 3| 2| 1| 0|
*        | 9| 8| 7| 6| 5|
* medial |14|13|12|11|10| lateral
*        |19|18|17|16|15|
*        |24|23|22|21|20|
*        +--+--+--+--+--+
*            proximal
*
*/
typedef uint32_t finger_map_bmp;

typedef enum fingerprint_enroll_msg_type {
    FINGERPRINT_ENROLL_MSG_NONE = 0,
    FINGERPRINT_ENROLL_MSG_PREDEFINED = 1,  /* TODO: define standard enroll cues */
    FINGERPRINT_ENROLL_MSG_BITMAP = 2,  /* typeof(fingerprint_enroll.msg) == *finger_map_bmp */
    FINGERPRINT_ENROLL_MSG_VENDOR = 3
} fingerprint_enroll_msg_type_t;

typedef struct fingerprint_enroll {
    fingerprint_finger_id_t finger;
    /* samples_remaining goes from N (no data collected, but N scans needed)
     * to 0 (no more data is needed to build a template). */
    uint32_t samples_remaining;
    fingerprint_enroll_msg_type_t msg_type;
    size_t msg_size;
    void *msg;
} fingerprint_enroll_t;

typedef struct fingerprint_removed {
    fingerprint_finger_id_t finger;
} fingerprint_removed_t;

typedef struct fingerprint_acquired {
    fingerprint_acquired_info_t acquired_info; /* information about the image */
} fingerprint_acquired_t;

typedef struct fingerprint_authenticated {
    fingerprint_finger_id_t finger;
    hw_auth_token_t hat;
} fingerprint_authenticated_t;

typedef struct fingerprint_msg {
    fingerprint_msg_type_t type;
    union {
        fingerprint_error_t error;
        fingerprint_enroll_t enroll;
        fingerprint_removed_t removed;
        fingerprint_acquired_t acquired;
        fingerprint_authenticated_t authenticated;
    } data;
} fingerprint_msg_t;

/* Callback function type */
typedef void (*fingerprint_notify_t)(fingerprint_msg_t msg);

/* Synchronous operation */
typedef struct fingerprint_device {
    /**
     * Common methods of the fingerprint device. This *must* be the first member
     * of fingerprint_device as users of this structure will cast a hw_device_t
     * to fingerprint_device pointer in contexts where it's known
     * the hw_device_t references a fingerprint_device.
     */
    struct hw_device_t common;

    /*
     * Fingerprint enroll request:
     * Switches the HAL state machine to collect and store a new fingerprint
     * template. Switches back as soon as enroll is complete
     * (fingerprint_msg.type == FINGERPRINT_TEMPLATE_ENROLLING &&
     *  fingerprint_msg.data.enroll.samples_remaining == 0)
     * or after timeout_sec seconds.
     * The fingerprint template will be assigned to the group gid. User has a choice
     * to supply the gid or set it to 0 in which case a unique group id will be generated.
     *
     * Function return: 0 if enrollment process can be successfully started
     *                 -1 otherwise. A notify() function may be called
     *                    indicating the error condition.
     */
    int (*enroll)(struct fingerprint_device *dev, const hw_auth_token_t *hat,
                    uint32_t gid, uint32_t timeout_sec);

    /*
     * Fingerprint pre-enroll enroll request:
     * Generates a unique token to upper layers to indicate the start of an enrollment transaction.
     * This token will be wrapped by security for verification and passed to enroll() for
     * verification before enrollment will be allowed. This is to ensure adding a new fingerprint
     * template was preceded by some kind of credential confirmation (e.g. device password).
     *
     * Function return: 0 if function failed
     *                  otherwise, a uint64_t of token
     */
    uint64_t (*pre_enroll)(struct fingerprint_device *dev);

    /*
     * get_authenticator_id:
     * Returns a token associated with the current fingerprint set. This value will
     * change whenever a new fingerprint is enrolled, thus creating a new fingerprint
     * set.
     *
     * Function return: current authenticator id.
     */
    uint64_t (*get_authenticator_id)(struct fingerprint_device *dev);

    /*
     * Cancel pending enroll or authenticate, sending FINGERPRINT_ERROR_CANCELED
     * to all running clients. Switches the HAL state machine back to the idle state.
     * will indicate switch back to the scan mode.
     *
     * Function return: 0 if cancel request is accepted
     *                 -1 otherwise.
     */
    int (*cancel)(struct fingerprint_device *dev);

    /*
     * Fingerprint remove request:
     * deletes a fingerprint template.
     * If the fingerprint id is 0 and the group is 0 then the entire template
     * database will be removed. A combinaiton of fingerprint id 0 and a valid
     * group id deletes all fingreprints in that group.
     * notify() will be called for each template deleted with
     * fingerprint_msg.type == FINGERPRINT_TEMPLATE_REMOVED and
     * fingerprint_msg.data.removed.id indicating each template id removed.
     *
     * Function return: 0 if fingerprint template(s) can be successfully deleted
     *                 -1 otherwise.
     */
    int (*remove)(struct fingerprint_device *dev, fingerprint_finger_id_t finger);

    /*
     * Restricts the HAL operation to a set of fingerprints belonging to a
     * group provided. Gid of 0 signals global operation.
     *
     * Function return: 0 on success
     *                 -1 if the group does not exist.
     */
    int (*set_active_group)(struct fingerprint_device *dev, uint32_t gid);

    /*
     * Authenticates an operation identifed by operation_id
     *
     * Function return: 0 on success
     *                 -1 if the operation cannot be completed
     */
    int (*authenticate)(struct fingerprint_device *dev, uint64_t operation_id, uint32_t gid);

    /*
     * Set notification callback:
     * Registers a user function that would receive notifications from the HAL
     * The call will block if the HAL state machine is in busy state until HAL
     * leaves the busy state.
     *
     * Function return: 0 if callback function is successfuly registered
     *                 -1 otherwise.
     */
    int (*set_notify)(struct fingerprint_device *dev, fingerprint_notify_t notify);

    /*
     * Client provided callback function to receive notifications.
     * Do not set by hand, use the function above instead.
     */
    fingerprint_notify_t notify;
} fingerprint_device_t;

typedef struct fingerprint_module {
    /**
     * Common methods of the fingerprint module. This *must* be the first member
     * of fingerprint_module as users of this structure will cast a hw_module_t
     * to fingerprint_module pointer in contexts where it's known
     * the hw_module_t references a fingerprint_module.
     */
    struct hw_module_t common;
} fingerprint_module_t;

#endif  /* ANDROID_INCLUDE_HARDWARE_FINGERPRINT_H */
