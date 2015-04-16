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

#include <memory>

#include <linux/input.h>

#include <gtest/gtest.h>

#include <utils/Timers.h>

#include "InputDevice.h"
#include "InputHost.h"
#include "InputHub.h"
#include "InputMocks.h"

// # of milliseconds to allow for timing measurements
#define TIMING_TOLERANCE_MS 25

#define MSC_ANDROID_TIME_SEC  0x6
#define MSC_ANDROID_TIME_USEC 0x7

namespace android {
namespace tests {

class EvdevDeviceTest : public ::testing::Test {
protected:
     virtual void SetUp() override {
         mMockHost.reset(new MockInputHost());
     }

     virtual void TearDown() override {
        ASSERT_TRUE(mMockHost->checkAllocations());
     }

    std::unique_ptr<MockInputHost> mMockHost;
};

TEST_F(EvdevDeviceTest, testOverrideTime) {
    InputHost host = {mMockHost.get(), kTestCallbacks};
    auto node = std::make_shared<MockInputDeviceNode>();
    auto device = std::make_unique<EvdevDevice>(host, node);
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

TEST_F(EvdevDeviceTest, testWrongClockCorrection) {
    InputHost host = {mMockHost.get(), kTestCallbacks};
    auto node = std::make_shared<MockInputDeviceNode>();
    auto device = std::make_unique<EvdevDevice>(host, node);
    ASSERT_TRUE(device != nullptr);

    auto now = systemTime(SYSTEM_TIME_MONOTONIC);

    // Input event that supposedly comes from 1 minute in the future. In
    // reality, the timestamps would be much further off.
    InputEvent event = { now + s2ns(60), EV_KEY, KEY_HOME, 1 };

    device->processInput(event, now);

    EXPECT_NEAR(now, event.when, ms2ns(TIMING_TOLERANCE_MS));
}

TEST_F(EvdevDeviceTest, testClockCorrectionOk) {
    InputHost host = {mMockHost.get(), kTestCallbacks};
    auto node = std::make_shared<MockInputDeviceNode>();
    auto device = std::make_unique<EvdevDevice>(host, node);
    ASSERT_TRUE(device != nullptr);

    auto now = systemTime(SYSTEM_TIME_MONOTONIC);

    // Input event from now, but will be reported as if it came early.
    InputEvent event = { now, EV_KEY, KEY_HOME, 1 };

    // event_time parameter is 11 seconds in the past, so it looks like we used
    // the wrong clock.
    device->processInput(event, now - s2ns(11));

    EXPECT_NEAR(now, event.when, ms2ns(TIMING_TOLERANCE_MS));
}

TEST_F(EvdevDeviceTest, testN7v2Touchscreen) {
    InputHost host = {mMockHost.get(), kTestCallbacks};
    auto node = std::shared_ptr<MockInputDeviceNode>(MockNexus7v2::getElanTouchscreen());
    auto device = std::make_unique<EvdevDevice>(host, node);
    EXPECT_EQ(INPUT_DEVICE_CLASS_TOUCH|INPUT_DEVICE_CLASS_TOUCH_MT,
            device->getInputClasses());
}

TEST_F(EvdevDeviceTest, testN7v2ButtonJack) {
    InputHost host = {mMockHost.get(), kTestCallbacks};
    auto node = std::shared_ptr<MockInputDeviceNode>(MockNexus7v2::getButtonJack());
    auto device = std::make_unique<EvdevDevice>(host, node);
    EXPECT_EQ(INPUT_DEVICE_CLASS_KEYBOARD, device->getInputClasses());
}

TEST_F(EvdevDeviceTest, testN7v2HeadsetJack) {
    InputHost host = {mMockHost.get(), kTestCallbacks};
    auto node = std::shared_ptr<MockInputDeviceNode>(MockNexus7v2::getHeadsetJack());
    auto device = std::make_unique<EvdevDevice>(host, node);
    EXPECT_EQ(INPUT_DEVICE_CLASS_SWITCH, device->getInputClasses());
}

TEST_F(EvdevDeviceTest, testN7v2H2wButton) {
    InputHost host = {mMockHost.get(), kTestCallbacks};
    auto node = std::shared_ptr<MockInputDeviceNode>(MockNexus7v2::getH2wButton());
    auto device = std::make_unique<EvdevDevice>(host, node);
    EXPECT_EQ(INPUT_DEVICE_CLASS_KEYBOARD, device->getInputClasses());
}

TEST_F(EvdevDeviceTest, testN7v2GpioKeys) {
    InputHost host = {mMockHost.get(), kTestCallbacks};
    auto node = std::shared_ptr<MockInputDeviceNode>(MockNexus7v2::getGpioKeys());
    auto device = std::make_unique<EvdevDevice>(host, node);
    EXPECT_EQ(INPUT_DEVICE_CLASS_KEYBOARD, device->getInputClasses());
}

TEST_F(EvdevDeviceTest, testNexusPlayerGpioKeys) {
    InputHost host = {mMockHost.get(), kTestCallbacks};
    auto node = std::shared_ptr<MockInputDeviceNode>(MockNexusPlayer::getGpioKeys());
    auto device = std::make_unique<EvdevDevice>(host, node);
    EXPECT_EQ(INPUT_DEVICE_CLASS_KEYBOARD, device->getInputClasses());
}

TEST_F(EvdevDeviceTest, testNexusPlayerMidPowerBtn) {
    InputHost host = {mMockHost.get(), kTestCallbacks};
    auto node = std::shared_ptr<MockInputDeviceNode>(MockNexusPlayer::getMidPowerBtn());
    auto device = std::make_unique<EvdevDevice>(host, node);
    EXPECT_EQ(INPUT_DEVICE_CLASS_KEYBOARD, device->getInputClasses());
}

TEST_F(EvdevDeviceTest, testNexusRemote) {
    InputHost host = {mMockHost.get(), kTestCallbacks};
    auto node = std::shared_ptr<MockInputDeviceNode>(MockNexusPlayer::getNexusRemote());
    auto device = std::make_unique<EvdevDevice>(host, node);
    EXPECT_EQ(INPUT_DEVICE_CLASS_KEYBOARD, device->getInputClasses());
}

TEST_F(EvdevDeviceTest, testAsusGamepad) {
    InputHost host = {mMockHost.get(), kTestCallbacks};
    auto node = std::shared_ptr<MockInputDeviceNode>(MockNexusPlayer::getAsusGamepad());
    auto device = std::make_unique<EvdevDevice>(host, node);
    EXPECT_EQ(INPUT_DEVICE_CLASS_JOYSTICK|INPUT_DEVICE_CLASS_KEYBOARD, device->getInputClasses());
}

}  // namespace tests
}  // namespace android
