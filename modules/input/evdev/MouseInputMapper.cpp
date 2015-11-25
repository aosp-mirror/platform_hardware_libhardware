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

#define LOG_TAG "MouseInputMapper"
//#define LOG_NDEBUG 0

#include "MouseInputMapper.h"

#include <linux/input.h>
#include <hardware/input.h>
#include <utils/Log.h>
#include <utils/misc.h>

#include "InputHost.h"
#include "InputHub.h"


namespace android {

// Map scancodes to input HAL usages.
// The order of these definitions MUST remain in sync with the order they are
// defined in linux/input.h.
static struct {
    int32_t scancode;
    InputUsage usage;
} codeMap[] = {
    {BTN_LEFT, INPUT_USAGE_BUTTON_PRIMARY},
    {BTN_RIGHT, INPUT_USAGE_BUTTON_SECONDARY},
    {BTN_MIDDLE, INPUT_USAGE_BUTTON_TERTIARY},
    {BTN_SIDE, INPUT_USAGE_BUTTON_UNKNOWN},
    {BTN_EXTRA, INPUT_USAGE_BUTTON_UNKNOWN},
    {BTN_FORWARD, INPUT_USAGE_BUTTON_FORWARD},
    {BTN_BACK, INPUT_USAGE_BUTTON_BACK},
    {BTN_TASK, INPUT_USAGE_BUTTON_UNKNOWN},
};


bool MouseInputMapper::configureInputReport(InputDeviceNode* devNode,
        InputReportDefinition* report) {
    setInputReportDefinition(report);
    getInputReportDefinition()->addCollection(INPUT_COLLECTION_ID_MOUSE, 1);

    // Configure mouse axes
    if (!devNode->hasRelativeAxis(REL_X) || !devNode->hasRelativeAxis(REL_Y)) {
        ALOGE("Device %s is missing a relative x or y axis. Device cannot be configured.",
                devNode->getPath().c_str());
        return false;
    }
    getInputReportDefinition()->declareUsage(INPUT_COLLECTION_ID_MOUSE, INPUT_USAGE_AXIS_X,
            INT32_MIN, INT32_MAX, 1.0f);
    getInputReportDefinition()->declareUsage(INPUT_COLLECTION_ID_MOUSE, INPUT_USAGE_AXIS_Y,
            INT32_MIN, INT32_MAX, 1.0f);
    if (devNode->hasRelativeAxis(REL_WHEEL)) {
        getInputReportDefinition()->declareUsage(INPUT_COLLECTION_ID_MOUSE,
                INPUT_USAGE_AXIS_VSCROLL, -1, 1, 0.0f);
    }
    if (devNode->hasRelativeAxis(REL_HWHEEL)) {
        getInputReportDefinition()->declareUsage(INPUT_COLLECTION_ID_MOUSE,
                INPUT_USAGE_AXIS_HSCROLL, -1, 1, 0.0f);
    }

    // Configure mouse buttons
    InputUsage usages[NELEM(codeMap)];
    int numUsages = 0;
    for (int32_t i = 0; i < NELEM(codeMap); ++i) {
        if (devNode->hasKey(codeMap[i].scancode)) {
            usages[numUsages++] = codeMap[i].usage;
        }
    }
    if (numUsages == 0) {
        ALOGW("MouseInputMapper found no buttons for %s", devNode->getPath().c_str());
    }
    getInputReportDefinition()->declareUsages(INPUT_COLLECTION_ID_MOUSE, usages, numUsages);
    return true;
}

void MouseInputMapper::process(const InputEvent& event) {
    ALOGV("processing mouse event. type=%d code=%d value=%d",
            event.type, event.code, event.value);
    switch (event.type) {
        case EV_KEY:
            processButton(event.code, event.value);
            break;
        case EV_REL:
            processMotion(event.code, event.value);
            break;
        case EV_SYN:
            if (event.code == SYN_REPORT) {
                sync(event.when);
            }
            break;
        default:
            ALOGV("unknown mouse event type: %d", event.type);
    }
}

void MouseInputMapper::processMotion(int32_t code, int32_t value) {
    switch (code) {
        case REL_X:
            mRelX = value;
            break;
        case REL_Y:
            mRelY = value;
            break;
        case REL_WHEEL:
            mRelWheel = value;
            break;
        case REL_HWHEEL:
            mRelHWheel = value;
            break;
        default:
            // Unknown code. Ignore.
            break;
    }
}

// Map evdev button codes to bit indices. This function assumes code >=
// BTN_MOUSE.
uint32_t buttonToBit(int32_t code) {
    return static_cast<uint32_t>(code - BTN_MOUSE);
}

void MouseInputMapper::processButton(int32_t code, int32_t value) {
    // Mouse buttons start at BTN_MOUSE and end before BTN_JOYSTICK. There isn't
    // really enough room after the mouse buttons for another button class, so
    // the risk of a button type being inserted after mouse is low.
    if (code >= BTN_MOUSE && code < BTN_JOYSTICK) {
        if (value) {
            mButtonValues.markBit(buttonToBit(code));
        } else {
            mButtonValues.clearBit(buttonToBit(code));
        }
        mUpdatedButtonMask.markBit(buttonToBit(code));
    }
}

void MouseInputMapper::sync(nsecs_t when) {
    // Process updated button states.
    while (!mUpdatedButtonMask.isEmpty()) {
        auto bit = mUpdatedButtonMask.clearFirstMarkedBit();
        getInputReport()->setBoolUsage(INPUT_COLLECTION_ID_MOUSE, codeMap[bit].usage,
                mButtonValues.hasBit(bit), 0);
    }

    // Process motion and scroll changes.
    if (mRelX != 0) {
        getInputReport()->setIntUsage(INPUT_COLLECTION_ID_MOUSE, INPUT_USAGE_AXIS_X, mRelX, 0);
    }
    if (mRelY != 0) {
        getInputReport()->setIntUsage(INPUT_COLLECTION_ID_MOUSE, INPUT_USAGE_AXIS_Y, mRelY, 0);
    }
    if (mRelWheel != 0) {
        getInputReport()->setIntUsage(INPUT_COLLECTION_ID_MOUSE, INPUT_USAGE_AXIS_VSCROLL,
                mRelWheel, 0);
    }
    if (mRelHWheel != 0) {
        getInputReport()->setIntUsage(INPUT_COLLECTION_ID_MOUSE, INPUT_USAGE_AXIS_HSCROLL,
                mRelHWheel, 0);
    }

    // Report and reset.
    getInputReport()->reportEvent(getDeviceHandle());
    mUpdatedButtonMask.clear();
    mButtonValues.clear();
    mRelX = 0;
    mRelY = 0;
    mRelWheel = 0;
    mRelHWheel = 0;
}

}  // namespace android
