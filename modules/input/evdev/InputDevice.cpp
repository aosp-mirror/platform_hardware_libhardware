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

#include <linux/input.h>

#define __STDC_FORMAT_MACROS
#include <cinttypes>
#include <string>

#include <utils/Log.h>
#include <utils/Timers.h>

#include "InputHub.h"
#include "InputDevice.h"

#define MSC_ANDROID_TIME_SEC  0x6
#define MSC_ANDROID_TIME_USEC 0x7

namespace android {

EvdevDevice::EvdevDevice(std::shared_ptr<InputDeviceNode> node) :
    mDeviceNode(node) {}

void EvdevDevice::processInput(InputEvent& event, nsecs_t currentTime) {
    std::string log;
    log.append("---InputEvent for device %s---\n");
    log.append("   when:  %" PRId64 "\n");
    log.append("   type:  %d\n");
    log.append("   code:  %d\n");
    log.append("   value: %d\n");
    ALOGV(log.c_str(), mDeviceNode->getPath().c_str(), event.when, event.type, event.code,
            event.value);

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
