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

#define LOG_TAG "InputDevice"
#define LOG_NDEBUG 0

// Enables debug output for processing input events
#define DEBUG_INPUT_EVENTS 0

#include <linux/input.h>

#define __STDC_FORMAT_MACROS
#include <cinttypes>
#include <cstdlib>
#include <string>

#include <utils/Log.h>
#include <utils/Timers.h>

#include "InputHub.h"
#include "InputDevice.h"

#define MSC_ANDROID_TIME_SEC  0x6
#define MSC_ANDROID_TIME_USEC 0x7

namespace android {

static InputBus getInputBus(const std::shared_ptr<InputDeviceNode>& node) {
    switch (node->getBusType()) {
        case BUS_USB:
            return INPUT_BUS_USB;
        case BUS_BLUETOOTH:
            return INPUT_BUS_BT;
        case BUS_RS232:
            return INPUT_BUS_SERIAL;
        default:
            // TODO: check for other linux bus types that might not be built-in
            return INPUT_BUS_BUILTIN;
    }
}

static uint32_t getAbsAxisUsage(int32_t axis, uint32_t deviceClasses) {
    // Touch devices get dibs on touch-related axes.
    if (deviceClasses & INPUT_DEVICE_CLASS_TOUCH) {
        switch (axis) {
            case ABS_X:
            case ABS_Y:
            case ABS_PRESSURE:
            case ABS_TOOL_WIDTH:
            case ABS_DISTANCE:
            case ABS_TILT_X:
            case ABS_TILT_Y:
            case ABS_MT_SLOT:
            case ABS_MT_TOUCH_MAJOR:
            case ABS_MT_TOUCH_MINOR:
            case ABS_MT_WIDTH_MAJOR:
            case ABS_MT_WIDTH_MINOR:
            case ABS_MT_ORIENTATION:
            case ABS_MT_POSITION_X:
            case ABS_MT_POSITION_Y:
            case ABS_MT_TOOL_TYPE:
            case ABS_MT_BLOB_ID:
            case ABS_MT_TRACKING_ID:
            case ABS_MT_PRESSURE:
            case ABS_MT_DISTANCE:
                return INPUT_DEVICE_CLASS_TOUCH;
        }
    }

    // External stylus gets the pressure axis
    if (deviceClasses & INPUT_DEVICE_CLASS_EXTERNAL_STYLUS) {
        if (axis == ABS_PRESSURE) {
            return INPUT_DEVICE_CLASS_EXTERNAL_STYLUS;
        }
    }

    // Joystick devices get the rest.
    return INPUT_DEVICE_CLASS_JOYSTICK;
}

static bool getBooleanProperty(const InputProperty& prop) {
    const char* propValue = prop.getValue();
    if (propValue == nullptr) return false;

    char* end;
    int value = std::strtol(propValue, &end, 10);
    if (*end != '\0') {
        ALOGW("Expected boolean for property %s; value=%s", prop.getKey(), propValue);
        return false;
    }
    return value;
}

static void setDeviceClasses(const InputDeviceNode* node, uint32_t* classes) {
    // See if this is a keyboard. Ignore everything in the button range except
    // for joystick and gamepad buttons which are handled like keyboards for the
    // most part.
    bool haveKeyboardKeys = node->hasKeyInRange(0, BTN_MISC) ||
        node->hasKeyInRange(KEY_OK, KEY_CNT);
    bool haveGamepadButtons = node->hasKeyInRange(BTN_MISC, BTN_MOUSE) ||
        node->hasKeyInRange(BTN_JOYSTICK, BTN_DIGI);
    if (haveKeyboardKeys || haveGamepadButtons) {
        *classes |= INPUT_DEVICE_CLASS_KEYBOARD;
    }

    // See if this is a cursor device such as a trackball or mouse.
    if (node->hasKey(BTN_MOUSE)
            && node->hasRelativeAxis(REL_X)
            && node->hasRelativeAxis(REL_Y)) {
        *classes |= INPUT_DEVICE_CLASS_CURSOR;
    }

    // See if this is a touch pad.
    // Is this a new modern multi-touch driver?
    if (node->hasAbsoluteAxis(ABS_MT_POSITION_X)
            && node->hasAbsoluteAxis(ABS_MT_POSITION_Y)) {
        // Some joysticks such as the PS3 controller report axes that conflict
        // with the ABS_MT range. Try to confirm that the device really is a
        // touch screen.
        if (node->hasKey(BTN_TOUCH) || !haveGamepadButtons) {
            *classes |= INPUT_DEVICE_CLASS_TOUCH | INPUT_DEVICE_CLASS_TOUCH_MT;
        }
    // Is this an old style single-touch driver?
    } else if (node->hasKey(BTN_TOUCH)
            && node->hasAbsoluteAxis(ABS_X)
            && node->hasAbsoluteAxis(ABS_Y)) {
        *classes != INPUT_DEVICE_CLASS_TOUCH;
    // Is this a BT stylus?
    } else if ((node->hasAbsoluteAxis(ABS_PRESSURE) || node->hasKey(BTN_TOUCH))
            && !node->hasAbsoluteAxis(ABS_X) && !node->hasAbsoluteAxis(ABS_Y)) {
        *classes |= INPUT_DEVICE_CLASS_EXTERNAL_STYLUS;
        // Keyboard will try to claim some of the buttons but we really want to
        // reserve those so we can fuse it with the touch screen data, so just
        // take them back. Note this means an external stylus cannot also be a
        // keyboard device.
        *classes &= ~INPUT_DEVICE_CLASS_KEYBOARD;
    }

    // See if this device is a joystick.
    // Assumes that joysticks always have gamepad buttons in order to
    // distinguish them from other devices such as accelerometers that also have
    // absolute axes.
    if (haveGamepadButtons) {
        uint32_t assumedClasses = *classes | INPUT_DEVICE_CLASS_JOYSTICK;
        for (int i = 0; i < ABS_CNT; ++i) {
            if (node->hasAbsoluteAxis(i)
                    && getAbsAxisUsage(i, assumedClasses) == INPUT_DEVICE_CLASS_JOYSTICK) {
                *classes = assumedClasses;
                break;
            }
        }
    }

    // Check whether this device has switches.
    for (int i = 0; i < SW_CNT; ++i) {
        if (node->hasSwitch(i)) {
            *classes |= INPUT_DEVICE_CLASS_SWITCH;
            break;
        }
    }

    // Check whether this device supports the vibrator.
    if (node->hasForceFeedback(FF_RUMBLE)) {
        *classes |= INPUT_DEVICE_CLASS_VIBRATOR;
    }

    // If the device isn't recognized as something we handle, don't monitor it.
    // TODO

    ALOGD("device %s classes=0x%x", node->getPath().c_str(), *classes);
}

EvdevDevice::EvdevDevice(InputHost host, const std::shared_ptr<InputDeviceNode>& node) :
    mHost(host), mDeviceNode(node) {

    InputBus bus = getInputBus(node);
    mInputId = mHost.createDeviceIdentifier(
            node->getName().c_str(),
            node->getProductId(),
            node->getVendorId(),
            bus,
            node->getUniqueId().c_str());

    InputPropertyMap propMap = mHost.getDevicePropertyMap(mInputId);
    setDeviceClasses(mDeviceNode.get(), &mClasses);
}

void EvdevDevice::processInput(InputEvent& event, nsecs_t currentTime) {
#if DEBUG_INPUT_EVENTS
    std::string log;
    log.append("---InputEvent for device %s---\n");
    log.append("   when:  %" PRId64 "\n");
    log.append("   type:  %d\n");
    log.append("   code:  %d\n");
    log.append("   value: %d\n");
    ALOGD(log.c_str(), mDeviceNode->getPath().c_str(), event.when, event.type, event.code,
            event.value);
#endif

    if (event.type == EV_MSC) {
        if (event.code == MSC_ANDROID_TIME_SEC) {
            mOverrideSec = event.value;
        } else if (event.code == MSC_ANDROID_TIME_USEC) {
            mOverrideUsec = event.value;
        }
        return;
    }

    if (mOverrideSec || mOverrideUsec) {
        event.when = s2ns(mOverrideSec) + us2ns(mOverrideUsec);
        ALOGV("applied override time %d.%06d", mOverrideSec, mOverrideUsec);

        if (event.type == EV_SYN && event.code == SYN_REPORT) {
            mOverrideSec = 0;
            mOverrideUsec = 0;
        }
    }

    // Bug 7291243: Add a guard in case the kernel generates timestamps
    // that appear to be far into the future because they were generated
    // using the wrong clock source.
    //
    // This can happen because when the input device is initially opened
    // it has a default clock source of CLOCK_REALTIME.  Any input events
    // enqueued right after the device is opened will have timestamps
    // generated using CLOCK_REALTIME.  We later set the clock source
    // to CLOCK_MONOTONIC but it is already too late.
    //
    // Invalid input event timestamps can result in ANRs, crashes and
    // and other issues that are hard to track down.  We must not let them
    // propagate through the system.
    //
    // Log a warning so that we notice the problem and recover gracefully.
    if (event.when >= currentTime + s2ns(10)) {
        // Double-check. Time may have moved on.
        auto time = systemTime(SYSTEM_TIME_MONOTONIC);
        if (event.when > time) {
            ALOGW("An input event from %s has a timestamp that appears to have "
                    "been generated using the wrong clock source (expected "
                    "CLOCK_MONOTONIC): event time %" PRId64 ", current time %" PRId64
                    ", call time %" PRId64 ". Using current time instead.",
                    mDeviceNode->getPath().c_str(), event.when, time, currentTime);
            event.when = time;
        } else {
            ALOGV("Event time is ok but failed the fast path and required an extra "
                    "call to systemTime: event time %" PRId64 ", current time %" PRId64
                    ", call time %" PRId64 ".", event.when, time, currentTime);
        }
    }
}

}  // namespace android
