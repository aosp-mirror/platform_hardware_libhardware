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

#define LOG_TAG "SwitchInputMapper"
//#define LOG_NDEBUG 0

#include "SwitchInputMapper.h"

#include <inttypes.h>
#include <linux/input.h>
#include <hardware/input.h>
#include <utils/Log.h>

#include "InputHost.h"
#include "InputHub.h"

namespace android {

static struct {
    int32_t scancode;
    InputUsage usage;
} codeMap[] = {
    {SW_LID, INPUT_USAGE_SWITCH_LID},
    {SW_TABLET_MODE, INPUT_USAGE_SWITCH_UNKNOWN},
    {SW_HEADPHONE_INSERT, INPUT_USAGE_SWITCH_HEADPHONE_INSERT},
    {SW_RFKILL_ALL, INPUT_USAGE_SWITCH_UNKNOWN},
    {SW_MICROPHONE_INSERT, INPUT_USAGE_SWITCH_MICROPHONE_INSERT},
    {SW_DOCK, INPUT_USAGE_SWITCH_UNKNOWN},
    {SW_LINEOUT_INSERT, INPUT_USAGE_SWITCH_LINEOUT_INSERT},
    {SW_JACK_PHYSICAL_INSERT, INPUT_USAGE_SWITCH_UNKNOWN},
    {SW_VIDEOOUT_INSERT, INPUT_USAGE_SWITCH_UNKNOWN},
    {SW_CAMERA_LENS_COVER, INPUT_USAGE_SWITCH_CAMERA_LENS_COVER},
    {SW_KEYPAD_SLIDE, INPUT_USAGE_SWITCH_KEYPAD_SLIDE},
    {SW_FRONT_PROXIMITY, INPUT_USAGE_SWITCH_UNKNOWN},
    {SW_ROTATE_LOCK, INPUT_USAGE_SWITCH_UNKNOWN},
    {SW_LINEIN_INSERT, INPUT_USAGE_SWITCH_UNKNOWN},
    {SW_MUTE_DEVICE, INPUT_USAGE_SWITCH_UNKNOWN},
    {SW_PEN_INSERTED, INPUT_USAGE_SWITCH_UNKNOWN},
    {SW_MACHINE_COVER, INPUT_USAGE_SWITCH_UNKNOWN},
    {0x11 /* unused */, INPUT_USAGE_SWITCH_UNKNOWN},
    {0x12 /* unused */, INPUT_USAGE_SWITCH_UNKNOWN},
    {0x13 /* unused */, INPUT_USAGE_SWITCH_UNKNOWN},
    {0x14 /* unused */, INPUT_USAGE_SWITCH_UNKNOWN},
    {0x15 /* unused */, INPUT_USAGE_SWITCH_UNKNOWN},
    {0x16 /* unused */, INPUT_USAGE_SWITCH_UNKNOWN},
    {0x17 /* unused */, INPUT_USAGE_SWITCH_UNKNOWN},
    {0x18 /* unused */, INPUT_USAGE_SWITCH_UNKNOWN},
    {0x19 /* unused */, INPUT_USAGE_SWITCH_UNKNOWN},
    {0x1a /* unused */, INPUT_USAGE_SWITCH_UNKNOWN},
    {0x1b /* unused */, INPUT_USAGE_SWITCH_UNKNOWN},
    {0x1c /* unused */, INPUT_USAGE_SWITCH_UNKNOWN},
    {0x1d /* unused */, INPUT_USAGE_SWITCH_UNKNOWN},
    {0x1e /* unused */, INPUT_USAGE_SWITCH_UNKNOWN},
    {0x1f /* unused */, INPUT_USAGE_SWITCH_UNKNOWN},
    {0x20 /* unused */, INPUT_USAGE_SWITCH_UNKNOWN},
};

static_assert(SW_MAX == SW_MACHINE_COVER, "SW_MAX is not SW_MACHINE_COVER");

// This is the max value that any kernel has ever used. The v5.4 kernels
// increased SW_MAX to 0x20, while v5.8 decreased the value to 0x10.
static constexpr int32_t kMaxNumInputCodes = 0x21;

SwitchInputMapper::SwitchInputMapper()
    : InputMapper() {
    // If this gets larger than 64, then the mSwitchValues and mUpdatedSwitchMask
    // variables need to be changed to support more than 64 bits.
    static_assert(SW_CNT <= 64, "More than 64 switches defined in linux/input.h");
}

bool SwitchInputMapper::configureInputReport(InputDeviceNode* devNode,
        InputReportDefinition* report) {
    InputUsage usages[kMaxNumInputCodes];
    int numUsages = 0;
    for (int32_t i = 0; i < kMaxNumInputCodes; ++i) {
        if (devNode->hasSwitch(codeMap[i].scancode)) {
            usages[numUsages++] = codeMap[i].usage;
        }
    }
    if (numUsages == 0) {
        ALOGE("SwitchInputMapper found no switches for %s!", devNode->getPath().c_str());
        return false;
    }
    setInputReportDefinition(report);
    getInputReportDefinition()->addCollection(INPUT_COLLECTION_ID_SWITCH, 1);
    getInputReportDefinition()->declareUsages(INPUT_COLLECTION_ID_SWITCH, usages, numUsages);
    return true;
}

void SwitchInputMapper::process(const InputEvent& event) {
    switch (event.type) {
        case EV_SW:
            processSwitch(event.code, event.value);
            break;
        case EV_SYN:
            if (event.code == SYN_REPORT) {
                sync(event.when);
            }
            break;
        default:
            ALOGV("unknown switch event type: %d", event.type);
    }
}

void SwitchInputMapper::processSwitch(int32_t switchCode, int32_t switchValue) {
    ALOGV("processing switch event. code=%" PRId32 ", value=%" PRId32, switchCode, switchValue);
    if (switchCode >= 0 && switchCode < kMaxNumInputCodes) {
        if (switchValue) {
            mSwitchValues.markBit(switchCode);
        } else {
            mSwitchValues.clearBit(switchCode);
        }
        mUpdatedSwitchMask.markBit(switchCode);
    }
}

void SwitchInputMapper::sync(nsecs_t when) {
    if (mUpdatedSwitchMask.isEmpty()) {
        // Clear the values just in case.
        mSwitchValues.clear();
        return;
    }

    while (!mUpdatedSwitchMask.isEmpty()) {
        auto bit = mUpdatedSwitchMask.firstMarkedBit();
        getInputReport()->setBoolUsage(INPUT_COLLECTION_ID_SWITCH, codeMap[bit].usage,
                mSwitchValues.hasBit(bit), 0);
        mUpdatedSwitchMask.clearBit(bit);
    }
    getInputReport()->reportEvent(getDeviceHandle());
    mUpdatedSwitchMask.clear();
    mSwitchValues.clear();
}

}  // namespace android
