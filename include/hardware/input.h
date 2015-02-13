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

#ifndef ANDROID_INCLUDE_HARDWARE_INPUT_H
#define ANDROID_INCLUDE_HARDWARE_INPUT_H

#include <hardware/hardware.h>
#include <stdint.h>

__BEGIN_DECLS

#define INPUT_MODULE_API_VERSION_1_0 HARDWARE_MODULE_API_VERSION(1, 0)
#define INPUT_HARDWARE_MODULE_ID "input"

#define INPUT_INSTANCE_EVDEV "evdev"

typedef enum input_bus {
    INPUT_BUS_BT,
    INPUT_BUS_USB,
    INPUT_BUS_SERIAL,
    INPUT_BUS_BUILTIN
} input_bus_t;

typedef struct input_host input_host_t;

typedef struct input_device_handle input_device_handle_t;

typedef struct input_device_identifier input_device_identifier_t;

typedef struct input_device_definition input_device_definition_t;

typedef struct input_report_definition input_report_definition_t;

typedef struct input_report input_report_t;

typedef struct input_collection input_collection_t;

typedef enum {
    INPUT_USAGE_TOUCHPAD_X,
    INPUT_USAGE_TOUCHPAD_Y,
    // etc
} input_usage_t;

typedef enum {
    INPUT_COLLECTION_ID_TOUCH,
    INPUT_COLLECTION_ID_KEYBOARD,
    INPUT_COLLECTION_ID_MOUSE,
    INPUT_COLLECTION_ID_TOUCHPAD,
    // etc
} input_collection_id_t;

typedef struct input_message input_message_t;

typedef struct input_host_callbacks {

    /**
     * Creates a device identifier with the given properties.
     * The unique ID should be a string that precisely identifies a given piece of hardware. For
     * example, an input device connected via Bluetooth could use its MAC address as its unique ID.
     */
    input_device_identifier_t* (*create_device_identifier)(input_host_t* host,
            const char* name, int32_t product_id, int32_t vendor_id,
            input_bus_t bus, const char* unique_id);

    /**
     * Allocates the device definition which will describe the input capabilities of a device. A
     * device definition may be used to register as many devices as desired.
     */
    input_device_definition_t* (*create_device_definition)(input_host_t* host);

    /**
     * Allocate either an input report, which the HAL will use to tell the host of incoming input
     * events, or an output report, which the host will use to tell the HAL of desired state
     * changes (e.g. setting an LED).
     */
    input_report_definition_t* (*create_input_report_definition)(input_host_t* host);
    input_report_definition_t* (*create_output_report_definition)(input_host_t* host);

    /**
     * Append the report to the given input device.
     */
    void (*input_device_definition_add_report)(input_host_t* host,
            input_device_definition_t* d, input_report_definition_t* r);

    /**
     * Add a collection with the given arity and ID. A collection describes a set
     * of logically grouped properties such as the X and Y coordinates of a single finger touch or
     * the set of keys on a keyboard. The arity declares how many repeated instances of this
     * collection will appear in whatever report it is attached to. The ID describes the type of
     * grouping being represented by the collection. For example, a touchscreen capable of
     * reporting up to 2 fingers simultaneously might have a collection with the X and Y
     * coordinates, an arity of 2, and an ID of INPUT_COLLECTION_USAGE_TOUCHSCREEN. Any given ID
     * may only be present once for a given report.
     */
    void (*input_report_definition_add_collection)(input_host_t* host,
            input_report_definition_t* report, input_collection_id_t id, int32_t arity);

    /**
     * Declare an int usage with the given properties. The report and collection defines where the
     * usage is being declared.
     */
    void (*input_report_definition_declare_usage_int)(input_host_t* host,
            input_report_definition_t* report, input_collection_id_t id,
            input_usage_t usage, int32_t min, int32_t max, float resolution);

    /**
     * Declare a set of boolean usages with the given properties.  The report and collection
     * defines where the usages are being declared.
     */
    void (*input_report_definition_declare_usages_bool)(input_host_t* host,
            input_report_definition_t* report, input_collection_id_t id,
            input_usage_t* usage, size_t usage_count);


    /**
     * Register a given input device definition. This notifies the host that an input device has
     * been connected and gives a description of all its capabilities.
     */
    input_device_handle_t* (*register_device)(input_host_t* host,
            input_device_identifier_t* id, input_device_definition_t* d);

    /** Unregister the given device */
    void (*unregister_device)(input_host_t* host, input_device_handle_t* handle);

    /**
     * Allocate a report that will contain all of the state as described by the given report.
     */
    input_report_t* (*input_allocate_report)(input_host_t* host, input_report_definition_t* r);

    void (*report_event)(input_host_t* host, input_device_handle_t* d, input_report_t* report);
} input_host_callbacks_t;

typedef struct input_module input_module_t;

struct input_module {
    /**
     * Common methods of the input module. This *must* be the first member
     * of input_module as users of this structure will cast a hw_module_t
     * to input_module pointer in contexts where it's known
     * the hw_module_t references a input_module.
     */
    struct hw_module_t common;

    /**
     * Initialize the module with host callbacks. At this point the HAL should start up whatever
     * infrastructure it needs to in order to process input events.
     */
    void (*init)(const input_module_t* module, input_host_t* host, input_host_callbacks_t cb);

    /**
     * Sends an output report with a new set of state the host would like the given device to
     * assume.
     */
    void (*notify_report)(const input_module_t* module, input_report_t* report);
};

static inline int input_open(const struct hw_module_t** module, const char* type) {
    return hw_get_module_by_class(INPUT_HARDWARE_MODULE_ID, type, module);
}

__END_DECLS

#endif  /* ANDROID_INCLUDE_HARDWARE_INPUT_H */
