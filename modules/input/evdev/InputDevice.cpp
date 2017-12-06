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
//#define LOG_NDEBUG 0

// Enables debug output for processing input events
#define DEBUG_INPUT_EVENTS 0

#include "InputDevice.h"

#include <linux/input.h>

#define __STDC_FORMAT_MACROS
#include <cinttypes>
#include <cstdlib>
#include <string>

#include <utils/Log.h>
#include <utils/Timers.h>

#include "InputHost.h"
#include "InputHub.h"
#include "MouseInputMapper.h"
#include "SwitchInputMapper.h"

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

EvdevDevice::EvdevDevice(InputHostInterface* host, const std::shared_ptr<InputDeviceNode>& node) :
    mHost(host), mDeviceNode(node), mDeviceDefinition(mHost->createDeviceDefinition()) {

    InputBus bus = getInputBus(node);
    mInputId = mHost->createDeviceIdentifier(
            node->getName().c_str(),
            node->getProductId(),
            node->getVendorId(),
            bus,
            node->getUniqueId().c_str());

    createMappers();
    configureDevice();

    // If we found a need for at least one mapper, register the device with the
    // host. If there were no mappers, this device is effectively ignored, as
    // the host won't know about it.
    if (mMappers.size() > 0) {
        mDeviceHandle = mHost->registerDevice(mInputId, mDeviceDefinition);
        for (const auto& mapper : mMappers) {
            mapper->setDeviceHandle(mDeviceHandle);
        }
    }
}

void EvdevDevice::createMappers() {
    // See if this is a cursor device such as a trackball or mouse.
    if (mDeviceNode->hasKey(BTN_MOUSE)
            && mDeviceNode->hasRelativeAxis(REL_X)
            && mDeviceNode->hasRelativeAxis(REL_Y)) {
        mClasses |= INPUT_DEVICE_CLASS_CURSOR;
        mMappers.push_back(std::make_unique<MouseInputMapper>());
    }

    bool isStylus = false;
    bool haveGamepadButtons = mDeviceNode->hasKeyInRange(BTN_MISC, BTN_MOUSE) ||
            mDeviceNode->hasKeyInRange(BTN_JOYSTICK, BTN_DIGI);

    // See if this is a touch pad or stylus.
    // Is this a new modern multi-touch driver?
    if (mDeviceNode->hasAbsoluteAxis(ABS_MT_POSITION_X)
            && mDeviceNode->hasAbsoluteAxis(ABS_MT_POSITION_Y)) {
        // Some joysticks such as the PS3 controller report axes that conflict
        // with the ABS_MT range. Try to confirm that the device really is a
        // touch screen.
        if (mDeviceNode->hasKey(BTN_TOUCH) || !haveGamepadButtons) {
            mClasses |= INPUT_DEVICE_CLASS_TOUCH | INPUT_DEVICE_CLASS_TOUCH_MT;
            //mMappers.push_back(std::make_unique<MultiTouchInputMapper>());
        }
    // Is this an old style single-touch driver?
    } else if (mDeviceNode->hasKey(BTN_TOUCH)
            && mDeviceNode->hasAbsoluteAxis(ABS_X)
            && mDeviceNode->hasAbsoluteAxis(ABS_Y)) {
        mClasses |= INPUT_DEVICE_CLASS_TOUCH;
        //mMappers.push_back(std::make_unique<SingleTouchInputMapper>());
    // Is this a BT stylus?
    } else if ((mDeviceNode->hasAbsoluteAxis(ABS_PRESSURE) || mDeviceNode->hasKey(BTN_TOUCH))
            && !mDeviceNode->hasAbsoluteAxis(ABS_X) && !mDeviceNode->hasAbsoluteAxis(ABS_Y)) {
        mClasses |= INPUT_DEVICE_CLASS_EXTERNAL_STYLUS;
        //mMappers.push_back(std::make_unique<ExternalStylusInputMapper>());
        isStylus = true;
        mClasses &= ~INPUT_DEVICE_CLASS_KEYBOARD;
    }

    // See if this is a keyboard. Ignore everything in the button range except
    // for joystick and gamepad buttons which are handled like keyboards for the
    // most part.
    // Keyboard will try to claim some of the stylus buttons but we really want
    // to reserve those so we can fuse it with the touch screen data. Note this
    // means an external stylus cannot also be a keyboard device.
    if (!isStylus) {
        bool haveKeyboardKeys = mDeviceNode->hasKeyInRange(0, BTN_MISC) ||
            mDeviceNode->hasKeyInRange(KEY_OK, KEY_CNT);
        if (haveKeyboardKeys || haveGamepadButtons) {
            mClasses |= INPUT_DEVICE_CLASS_KEYBOARD;
            //mMappers.push_back(std::make_unique<KeyboardInputMapper>());
        }
    }

    // See if this device is a joystick.
    // Assumes that joysticks always have gamepad buttons in order to
    // distinguish them from other devices such as accelerometers that also have
    // absolute axes.
    if (haveGamepadButtons) {
        uint32_t assumedClasses = mClasses | INPUT_DEVICE_CLASS_JOYSTICK;
        for (int i = 0; i < ABS_CNT; ++i) {
            if (mDeviceNode->hasAbsoluteAxis(i)
                    && getAbsAxisUsage(i, assumedClasses) == INPUT_DEVICE_CLASS_JOYSTICK) {
                mClasses = assumedClasses;
                //mMappers.push_back(std::make_unique<JoystickInputMapper>());
                break;
            }
        }
    }

    // Check whether this device has switches.
    for (int i = 0; i < SW_CNT; ++i) {
        if (mDeviceNode->hasSwitch(i)) {
            mClasses |= INPUT_DEVICE_CLASS_SWITCH;
            mMappers.push_back(std::make_unique<SwitchInputMapper>());
            break;
        }
    }

    // Check whether this device supports the vibrator.
    // TODO: decide if this is necessary.
    if (mDeviceNode->hasForceFeedback(FF_RUMBLE)) {
        mClasses |= INPUT_DEVICE_CLASS_VIBRATOR;
        //mMappers.push_back(std::make_unique<VibratorInputMapper>());
    }

    ALOGD("device %s classes=0x%x %zu mappers", mDeviceNode->getPath().c_str(), mClasses,
            mMappers.size());
}

void EvdevDevice::configureDevice() {
    for (const auto& mapper : mMappers) {
        auto reportDef = mHost->createInputReportDefinition();
        if (mapper->configureInputReport(mDeviceNode.get(), reportDef)) {
            mDeviceDefinition->addReport(reportDef);
        } else {
            mHost->freeReportDefinition(reportDef);
        }

        reportDef = mHost->createOutputReportDefinition();
        if (mapper->configureOutputReport(mDeviceNode.get(), reportDef)) {
            mDeviceDefinition->addReport(reportDef);
        } else {
            mHost->freeReportDefinition(reportDef);
        }
    }
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

    for (size_t i = 0; i < mMappers.size(); ++i) {
        mMappers[i]->process(event);
    }
}

}  // namespace android
