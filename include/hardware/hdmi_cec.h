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

#ifndef ANDROID_INCLUDE_HARDWARE_HDMI_CEC_H
#define ANDROID_INCLUDE_HARDWARE_HDMI_CEC_H

#include <stdint.h>
#include <sys/cdefs.h>

#include <hardware/hardware.h>

__BEGIN_DECLS

#define HDMI_CEC_MODULE_API_VERSION_1_0 HARDWARE_MODULE_API_VERSION(1, 0)
#define HDMI_CEC_MODULE_API_VERSION_CURRENT HDMI_MODULE_API_VERSION_1_0

#define HDMI_CEC_DEVICE_API_VERSION_1_0 HARDWARE_DEVICE_API_VERSION(1, 0)
#define HDMI_CEC_DEVICE_API_VERSION_CURRENT HDMI_DEVICE_API_VERSION_1_0

#define HDMI_CEC_HARDWARE_MODULE_ID "hdmi_cec"
#define HDMI_CEC_HARDWARE_INTERFACE "hdmi_cec_hw_if"

typedef enum cec_device_type {
    CEC_DEVICE_TV = 0,
    CEC_DEVICE_RECORDER = 1,
    CEC_DEVICE_RESERVED = 2,
    CEC_DEVICE_TUNER = 3,
    CEC_DEVICE_PLAYBACK = 4,
    CEC_DEVICE_AUDIO_SYSTEM = 5,
    CEC_DEVICE_MAX = CEC_DEVICE_AUDIO_SYSTEM
} cec_device_type_t;

typedef enum cec_logical_address {
    CEC_ADDR_TV = 0,
    CEC_ADDR_RECORDER_1 = 1,
    CEC_ADDR_RECORDER_2 = 2,
    CEC_ADDR_TUNER_1 = 3,
    CEC_ADDR_PLAYBACK_1 = 4,
    CEC_ADDR_AUDIO_SYSTEM = 5,
    CEC_ADDR_TUNER_2 = 6,
    CEC_ADDR_TUNER_3 = 7,
    CEC_ADDR_PLAYBACK_2 = 8,
    CEC_ADDR_RECORDER_3 = 9,
    CEC_ADDR_TUNER_4 = 10,
    CEC_ADDR_PLAYBACK_3 = 11,
    CEC_ADDR_RESERVED_1 = 12,
    CEC_ADDR_RESERVED_2 = 13,
    CEC_ADDR_FREE_USE = 14,
    CEC_ADDR_UNREGISTERED = 15,
    CEC_ADDR_BROADCAST = 15
} cec_logical_address_t;

/*
 * HDMI CEC messages
 */
enum cec_message_type {
    CEC_MESSAGE_FEATURE_ABORT = 0x00,
    CEC_MESSAGE_IMAGE_VIEW_ON = 0x04,
    CEC_MESSAGE_TUNER_STEP_INCREMENT = 0x05,
    CEC_MESSAGE_TUNER_STEP_DECREMENT = 0x06,
    CEC_MESSAGE_TUNER_DEVICE_STATUS = 0x07,
    CEC_MESSAGE_GIVE_TUNER_DEVICE_STATUS = 0x08,
    CEC_MESSAGE_RECORD_ON = 0x09,
    CEC_MESSAGE_RECORD_STATUS = 0x0A,
    CEC_MESSAGE_RECORD_OFF = 0x0B,
    CEC_MESSAGE_TEXT_VIEW_ON = 0x0D,
    CEC_MESSAGE_RECORD_TV_SCREEN = 0x0F,
    CEC_MESSAGE_GIVE_DECK_STATUS = 0x1A,
    CEC_MESSAGE_DECK_STATUS = 0x1B,
    CEC_MESSAGE_SET_MENU_LANGUAGE = 0x32,
    CEC_MESSAGE_CLEAR_ANALOG_TIMER = 0x33,
    CEC_MESSAGE_SET_ANALOG_TIMER = 0x34,
    CEC_MESSAGE_TIMER_STATUS = 0x35,
    CEC_MESSAGE_STANDBY = 0x36,
    CEC_MESSAGE_PLAY = 0x41,
    CEC_MESSAGE_DECK_CONTROL = 0x42,
    CEC_MESSAGE_TIMER_CLEARED_STATUS = 0x043,
    CEC_MESSAGE_USER_CONTROL_PRESSED = 0x44,
    CEC_MESSAGE_USER_CONTROL_RELEASED = 0x45,
    CEC_MESSAGE_GET_OSD_NAME = 0x46,
    CEC_MESSAGE_SET_OSD_NAME = 0x47,
    CEC_MESSAGE_SET_OSD_STRING = 0x64,
    CEC_MESSAGE_SET_TIMER_PROGRAM_TITLE = 0x67,
    CEC_MESSAGE_SYSTEM_AUDIO_MODE_REQUEST = 0x70,
    CEC_MESSAGE_GIVE_AUDIO_STATUS = 0x71,
    CEC_MESSAGE_SET_SYSTEM_AUDIO_MODE = 0x72,
    CEC_MESSAGE_REPORT_AUDIO_STATUS = 0x7A,
    CEC_MESSAGE_GIVE_SYSTEM_AUDIO_MODE_STATUS = 0x7D,
    CEC_MESSAGE_SYSTEM_AUDIO_MODE_STATUS = 0x7E,
    CEC_MESSAGE_ROUTING_CHANGE = 0x80,
    CEC_MESSAGE_ROUTING_INFORMATION = 0x81,
    CEC_MESSAGE_ACTIVE_SOURCE = 0x82,
    CEC_MESSAGE_GIVE_PHYSICAL_ADDRESS = 0x83,
    CEC_MESSAGE_REPORT_PHYSICAL_ADDRESS = 0x84,
    CEC_MESSAGE_REQUEST_ACTIVE_SOURCE = 0x85,
    CEC_MESSAGE_SET_STREAM_PATH = 0x86,
    CEC_MESSAGE_DEVICE_VENDOR_ID = 0x87,
    CEC_MESSAGE_VENDOR_COMMAND = 0x89,
    CEC_MESSAGE_VENDOR_REMOTE_BUTTON_DOWN = 0x8A,
    CEC_MESSAGE_VENDOR_REMOTE_BUTTON_UP = 0x8B,
    CEC_MESSAGE_GIVE_DEVICE_VENDOR_ID = 0x8C,
    CEC_MESSAGE_MENU_REQUEST = 0x8D,
    CEC_MESSAGE_MENU_STATUS = 0x8E,
    CEC_MESSAGE_GIVE_DEVICE_POWER_STATUS = 0x8F,
    CEC_MESSAGE_REPORT_POWER_STATUS = 0x90,
    CEC_MESSAGE_GET_MENU_LANGUAGE = 0x91,
    CEC_MESSAGE_SELECT_ANALOG_SERVICE = 0x92,
    CEC_MESSAGE_SELECT_DIGITAL_SERVICE = 0x93,
    CEC_MESSAGE_SET_DIGITAL_TIMER = 0x97,
    CEC_MESSAGE_CLEAR_DIGITAL_TIMER = 0x99,
    CEC_MESSAGE_SET_AUDIO_RATE = 0x9A,
    CEC_MESSAGE_INACTIVE_SOURCE = 0x9D,
    CEC_MESSAGE_CEC_VERSION = 0x9E,
    CEC_MESSAGE_GET_CEC_VERSION = 0x9F,
    CEC_MESSAGE_VENDOR_COMMAND_WITH_ID = 0xA0,
    CEC_MESSAGE_CLEAR_EXTERNAL_TIMER = 0xA1,
    CEC_MESSAGE_SET_EXTERNAL_TIMER = 0xA2
};

/*
 * HDMI event type. used for hdmi_event_t.
 */
enum {
    HDMI_EVENT_CEC_MESSAGE = 1,
    HDMI_EVENT_HOT_PLUG = 2
};

/*
 * HDMI hotplug event type. Used when the event
 * type is HDMI_EVENT_HOT_PLUG.
 */
enum {
    HDMI_NOT_CONNECTED = 0,
    HDMI_CONNECTED = 1
};

/*
 * Maximum length in bytes of cec message body (exclude header block),
 * should not exceed 16 (spec CEC 6 Frame Description)
 */
#define CEC_MESSAGE_BODY_MAX_LENGTH 16

typedef struct cec_message {
    /*
     * logical address of sender
     */
    cec_logical_address_t initiator;

    /*
     * logical address of receiver
     */
    cec_logical_address_t destination;

    /*
     * length in bytes of body, range [0, CEC_MESSAGE_BODY_MAX_LENGTH]
     */
    size_t length;
    unsigned char body[CEC_MESSAGE_BODY_MAX_LENGTH];
} cec_message_t;

typedef struct hotplug_event {
    /*
     * true if the cable is connected; otherwise false.
     */
    int connected;
} hotplug_event_t;

/*
 * HDMI event generated from HAL.
 */
typedef struct hdmi_event {
    int type;
    struct hdmi_cec_device* dev;
    union {
        cec_message_t cec;
        hotplug_event_t hotplug;
    };
} hdmi_event_t;

/*
 * Callback function type that will be called by HAL implementation.
 * Services can not close/open the device in the callback.
 */
typedef void (*event_callback_t)(const hdmi_event_t* event, void* arg);

typedef struct hdmi_cec_module {
    struct hw_module_t common;
} hdmi_module_t;

/*
 * HDMI-CEC HAL interface definition.
 */
typedef struct hdmi_cec_device {
    struct hw_device_t common;

    /*
     * (*allocate_logical_address)() allocates a new logical address
     * for a given device type. The address is written to addr. The HAL
     * implementation is also expected to configure itself to start receiving
     * the messages addressed to the allocated one. If allocation
     * is not successful the addr will be set to CEC_ADDR_UNREGISTERED.
     *
     * Returns 0 on success or -errno on error.
     */
    int (*allocate_logical_address)(const struct hdmi_cec_device* dev,
            int device_type, cec_logical_address_t* addr);

    /*
     * (*get_physical_address)() returns the CEC physical address. The
     * address is written to addr.
     *
     * The physical address depends on the topology of the network formed
     * by connected HDMI devices. It is therefore likely to change if the cable
     * is plugged off and on again. It is advised to call get_physical_address
     * to get the updated address when hot plug event takes place.
     *
     * Returns 0 on success or -errno on error.
     */
    int (*get_physical_address)(const struct hdmi_cec_device* dev, uint16_t* addr);

    /*
     * (*send_message)() transmits HDMI-CEC message to other HDMI device.
     *
     * Returns 0 on success or -errno on error.
     */
    int (*send_message)(const struct hdmi_cec_device* dev,
            const cec_message_t*);

    /*
     * (*register_event_callback)() registers a callback that HDMI-CEC HAL
     * can later use for incoming CEC messages or internal HDMI events.
     * When calling from C++, use the argument arg to pass the calling object.
     * It will be passed back when the callback is invoked so that the context
     * can be retrieved.
     */
    void (*register_event_callback)(const struct hdmi_cec_device* dev,
            event_callback_t callback, void* arg);

    /*
     * (*get_version)() returns the CEC version supported by underlying
     * hardware. The version this HAL interface is based on is 0x04,
     * which corresponds to 1.3a.
     */
    void (*get_version)(const struct hdmi_cec_device* dev, int* version);

    /*
     * (*get_vendor_id)() returns the identifier of the vendor. It is
     * the 24-bit unique company ID obtained from the IEEE Registration
     * Authority Committee (RAC).
     */
    void (*get_vendor_id)(const struct hdmi_cec_device* dev, uint32_t* vendor_id);

    /* Reserved for future use to maximum 16 functions. Must be NULL. */
    void* reserved[16 - 6];
} hdmi_cec_device_t;

/** convenience API for opening and closing a device */

static inline int hdmi_cec_open(const struct hw_module_t* module,
        struct hdmi_cec_device** device) {
    return module->methods->open(module,
            HDMI_CEC_HARDWARE_INTERFACE, (struct hw_device_t**)device);
}

static inline int hdmi_cec_close(struct hdmi_cec_device* device) {
    return device->common.close(&device->common);
}

__END_DECLS

#endif /* ANDROID_INCLUDE_HARDWARE_HDMI_CEC_H */
