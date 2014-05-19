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

/*
 * Activity Recognition HAL. The goal is to provide low power, low latency, always-on activity
 * recognition implemented in hardware (i.e. these activity recognition algorithms/classifers
 * should NOT be run on the AP). By low power we mean that this may be activated 24/7 without
 * impacting the battery drain speed (goal in order of 1mW including the power for sensors).
 * This HAL does not specify the input sources that are used towards detecting these activities.
 * It has one monitor interface which can be used to batch activities for always-on
 * activity_recognition and if the latency is zero, the same interface can be used for low latency
 * detection.
 */

#ifndef ANDROID_ACTIVITY_RECOGNITION_INTERFACE_H
#define ANDROID_ACTIVITY_RECOGNITION_INTERFACE_H

#include <hardware/hardware.h>

__BEGIN_DECLS

#define ACTIVITY_RECOGNITION_HEADER_VERSION   1
#define ACTIVITY_RECOGNITION_API_VERSION_0_1  HARDWARE_DEVICE_API_VERSION_2(0, 1, ACTIVITY_RECOGNITION_HEADER_VERSION)

#define ACTIVITY_RECOGNITION_HARDWARE_MODULE_ID "activity_recognition"
#define ACTIVITY_RECOGNITION_HARDWARE_INTERFACE "activity_recognition_hw_if"

/*
 * Define constants for various activity types. Multiple activities may be active at the same time
 * and sometimes none of these activities may be active.
 */

/* Reserved. get_supported_activities_list() should not return this activity. */
#define ACTIVITY_RESERVED          (0)

#define ACTIVITY_IN_VEHICLE        (1)

#define ACTIVITY_ON_BICYCLE        (2)

#define ACTIVITY_WALKING           (3)

#define ACTIVITY_RUNNING           (4)

#define ACTIVITY_STILL             (5)

#define ACTIVITY_TILTING           (6)

/* Values for activity_event.event_types. */
enum {
    /*
     * A flush_complete event which indicates that a flush() has been successfully completed. This
     * does not correspond to any activity/event. An event of this type should be added to the end
     * of a batch FIFO and it indicates that all the events in the batch FIFO have been successfully
     * reported to the framework. An event of this type should be generated only if flush() has been
     * explicitly called and if the FIFO is empty at the time flush() is called it should trivially
     * return a flush_complete_event to indicate that the FIFO is empty.
     *
     * A flush complete event should have the following parameters set.
     * activity_event_t.event_type = ACTIVITY_EVENT_TYPE_FLUSH_COMPLETE
     * activity_event_t.activity = ACTIVITY_RESERVED
     * activity_event_t.timestamp = 0
     * activity_event_t.reserved = 0
     * See (*flush)() for more details.
     */
    ACTIVITY_EVENT_TYPE_FLUSH_COMPLETE = 0,

    /* Signifies entering an activity. */
    ACTIVITY_EVENT_TYPE_ENTER = 1,

    /* Signifies exiting an activity. */
    ACTIVITY_EVENT_TYPE_EXIT  = 2
};

/*
 * Each event is a separate activity with event_type indicating whether this activity has started
 * or ended. Eg event: (event_type="enter", activity="ON_FOOT", timestamp)
 */
typedef struct activity_event {
    /* One of the ACTIVITY_EVENT_TYPE_* constants defined above. */
    uint32_t event_type;

    /* One of ACTIVITY_* constants defined above. */
    uint32_t activity;

    /* Time at which the transition/event has occurred in nanoseconds using elapsedRealTimeNano. */
    int64_t timestamp;

    /* Set to zero. */
    int32_t reserved[4];
} activity_event_t;

typedef struct activity_recognition_module {
    /**
     * Common methods of the activity recognition module.  This *must* be the first member of
     * activity_recognition_module as users of this structure will cast a hw_module_t to
     * activity_recognition_module pointer in contexts where it's known the hw_module_t
     * references an activity_recognition_module.
     */
    hw_module_t common;

    /*
     * List of all activities supported by this module. Each activity is represented as an integer.
     * Each value in the list is one of the ACTIVITY_* constants defined above. Return
     * value is the size of this list.
     */
    int (*get_supported_activities_list)(struct activity_recognition_module* module,
            int** activity_list);
} activity_recognition_module_t;

struct activity_recognition_device;

typedef struct activity_recognition_callback_procs {
    // Callback for activity_data. This is guaranteed to not invoke any HAL methods.
    // Memory allocated for the events can be reused after this method returns.
    //    events - Array of activity_event_t s that are reported.
    //    count  - size of the array.
    void (*activity_callback)(const struct activity_recognition_callback_procs* procs,
            const activity_event_t* events, int count);
} activity_recognition_callback_procs_t;

typedef struct activity_recognition_device {
    /**
     * Common methods of the activity recognition device.  This *must* be the first member of
     * activity_recognition_device as users of this structure will cast a hw_device_t to
     * activity_recognition_device pointer in contexts where it's known the hw_device_t
     * references an activity_recognition_device.
     */
    hw_device_t common;

    /*
     * Sets the callback to invoke when there are events to report. This call overwrites the
     * previously registered callback (if any).
     */
    void (*register_activity_callback)(const struct activity_recognition_device* dev,
            const activity_recognition_callback_procs_t* callback);

    /*
     * Activates monitoring of activity transitions. Activities need not be reported as soon as they
     * are detected. The detected activities are stored in a FIFO and reported in batches when the
     * "max_batch_report_latency" expires or when the batch FIFO is full. The implementation should
     * allow the AP to go into suspend mode while the activities are detected and stored in the
     * batch FIFO. Whenever events need to be reported (like when the FIFO is full or when the
     * max_batch_report_latency has expired for an activity, event pair), it should wake_up the AP
     * so that no events are lost. Activities are stored as transitions and they are allowed to
     * overlap with each other. Each (activity, event_type) pair can be activated or deactivated
     * independently of the other. The HAL implementation needs to keep track of which pairs are
     * currently active and needs to detect only those pairs.
     *
     * activity   - The specific activity that needs to be detected.
     * event_type - Specific transition of the activity that needs to be detected.
     * max_batch_report_latency_ns - a transition can be delayed by at most
     *                               “max_batch_report_latency” nanoseconds.
     * Return 0 on success, negative errno code otherwise.
     */
    int (*enable_activity_event)(const struct activity_recognition_device* dev,
            uint32_t activity, uint32_t event_type, int64_t max_batch_report_latency_ns);

    /*
     * Disables detection of a specific (activity, event_type) pair.
     */
    int (*disable_activity_event)(const struct activity_recognition_device* dev,
            uint32_t activity, uint32_t event_type);

    /*
     * Flush all the batch FIFOs. Report all the activities that were stored in the FIFO so far as
     * if max_batch_report_latency had expired. This shouldn't change the latency in any way. Add
     * a flush_complete_event to indicate the end of the FIFO after all events are delivered.
     * See ACTIVITY_EVENT_TYPE_FLUSH_COMPLETE for more details.
     * Return 0 on success, negative errno code otherwise.
     */
    int (*flush)(const struct activity_recognition_device* dev);

    // Must be set to NULL.
    void (*reserved_procs[16 - 4])(void);
} activity_recognition_device_t;

static inline int activity_recognition_open(const hw_module_t* module,
        activity_recognition_device_t** device) {
    return module->methods->open(module,
            ACTIVITY_RECOGNITION_HARDWARE_INTERFACE, (hw_device_t**)device);
}

static inline int activity_recognition_close(activity_recognition_device_t* device) {
    return device->common.close(&device->common);
}

__END_DECLS

#endif // ANDROID_ACTIVITY_RECOGNITION_INTERFACE_H
