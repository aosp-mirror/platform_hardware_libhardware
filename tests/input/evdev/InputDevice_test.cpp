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

#define LOG_TAG "InputHub_test"
//#define LOG_NDEBUG 0

#include <linux/input.h>

#include <gtest/gtest.h>

#include <utils/Timers.h>

#include "InputDevice.h"
#include "InputHub.h"

// # of milliseconds to allow for timing measurements
#define TIMING_TOLERANCE_MS 25

#define MSC_ANDROID_TIME_SEC  0x6
#define MSC_ANDROID_TIME_USEC 0x7

namespace android {
namespace tests {

class MockInputDeviceNode : public InputDeviceNode {
    virtual const std::string& getPath() const override { return mPath; }

    virtual const std::string& getName() const override { return mName; }
    virtual const std::string& getLocation() const override { return mLocation; }
    virtual const std::string& getUniqueId() const override { return mUniqueId; }

    virtual uint16_t getBusType() const override { return 0; }
    virtual uint16_t getVendorId() const override { return 0; }
    virtual uint16_t getProductId() const override { return 0; }
    virtual uint16_t getVersion() const override { return 0; }

    virtual bool hasKey(int32_t key) const { return false; }
    virtual bool hasRelativeAxis(int axis) const { return false; }
    virtual bool hasInputProperty(int property) const { return false; }

    virtual int32_t getKeyState(int32_t key) const { return 0; }
    virtual int32_t getSwitchState(int32_t sw) const { return 0; }
    virtual const AbsoluteAxisInfo* getAbsoluteAxisInfo(int32_t axis) const { return nullptr; }
    virtual status_t getAbsoluteAxisValue(int32_t axis, int32_t* outValue) const { return 0; }

    virtual void vibrate(nsecs_t duration) {}
    virtual void cancelVibrate(int32_t deviceId) {}

    virtual void disableDriverKeyRepeat() {}

private:
    std::string mPath = "/test";
    std::string mName = "Test Device";
    std::string mLocation = "test/0";
    std::string mUniqueId = "test-id";
};

TEST(EvdevDeviceTest, testOverrideTime) {
    auto node = std::make_shared<MockInputDeviceNode>();
    auto device = std::make_unique<EvdevDevice>(node);
    ASSERT_TRUE(device != nullptr);

    // Send two timestamp override events before an input event.
    nsecs_t when = 2ULL;
    InputEvent msc1 = { when, EV_MSC, MSC_ANDROID_TIME_SEC, 1 };
    InputEvent msc2 = { when, EV_MSC, MSC_ANDROID_TIME_USEC, 900000 };

    // Send a key down and syn. Should get the overridden timestamp.
    InputEvent keyDown = { when, EV_KEY, KEY_HOME, 1 };
    InputEvent syn = { when, EV_SYN, SYN_REPORT, 0 };

    // Send a key up, which should be at the reported timestamp.
    InputEvent keyUp = { when, EV_KEY, KEY_HOME, 0 };

    device->processInput(msc1, when);
    device->processInput(msc2, when);
    device->processInput(keyDown, when);
    device->processInput(syn, when);
    device->processInput(keyUp, when);

    nsecs_t expectedWhen = s2ns(1) + us2ns(900000);
    EXPECT_EQ(expectedWhen, keyDown.when);
    EXPECT_EQ(expectedWhen, syn.when);
    EXPECT_EQ(when, keyUp.when);
}

TEST(EvdevDeviceTest, testWrongClockCorrection) {
    auto node = std::make_shared<MockInputDeviceNode>();
    auto device = std::make_unique<EvdevDevice>(node);
    ASSERT_TRUE(device != nullptr);

    auto now = systemTime(SYSTEM_TIME_MONOTONIC);

    // Input event that supposedly comes from 1 minute in the future. In
    // reality, the timestamps would be much further off.
    InputEvent event = { now + s2ns(60), EV_KEY, KEY_HOME, 1 };

    device->processInput(event, now);

    EXPECT_NEAR(now, event.when, ms2ns(TIMING_TOLERANCE_MS));
}

TEST(EvdevDeviceTest, testClockCorrectionOk) {
    auto node = std::make_shared<MockInputDeviceNode>();
    auto device = std::make_unique<EvdevDevice>(node);
    ASSERT_TRUE(device != nullptr);

    auto now = systemTime(SYSTEM_TIME_MONOTONIC);

    // Input event from now, but will be reported as if it came early.
    InputEvent event = { now, EV_KEY, KEY_HOME, 1 };

    // event_time parameter is 11 seconds in the past, so it looks like we used
    // the wrong clock.
    device->processInput(event, now - s2ns(11));

    EXPECT_NEAR(now, event.when, ms2ns(TIMING_TOLERANCE_MS));
}

}  // namespace tests
}  // namespace android
