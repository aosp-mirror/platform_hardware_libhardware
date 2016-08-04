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

#include "InputHub.h"

#include <chrono>
#include <memory>
#include <mutex>

#include <linux/input.h>

#include <gtest/gtest.h>

#include <utils/StopWatch.h>
#include <utils/Timers.h>

#include "TestHelpers.h"

// # of milliseconds to fudge stopwatch measurements
#define TIMING_TOLERANCE_MS 25
#define NO_TIMEOUT (-1)

namespace android {
namespace tests {

using namespace std::literals::chrono_literals;

using InputCbFunc = std::function<void(const std::shared_ptr<InputDeviceNode>&, InputEvent&, nsecs_t)>;
using DeviceCbFunc = std::function<void(const std::shared_ptr<InputDeviceNode>&)>;

static const InputCbFunc kNoopInputCb = [](const std::shared_ptr<InputDeviceNode>&, InputEvent&, nsecs_t){};
static const DeviceCbFunc kNoopDeviceCb = [](const std::shared_ptr<InputDeviceNode>&){};

class TestInputCallback : public InputCallbackInterface {
public:
    TestInputCallback() :
        mInputCb(kNoopInputCb), mDeviceAddedCb(kNoopDeviceCb), mDeviceRemovedCb(kNoopDeviceCb) {}
    virtual ~TestInputCallback() = default;

    void setInputCallback(const InputCbFunc& cb) { mInputCb = cb; }
    void setDeviceAddedCallback(const DeviceCbFunc& cb) { mDeviceAddedCb = cb; }
    void setDeviceRemovedCallback(const DeviceCbFunc& cb) { mDeviceRemovedCb = cb; }

    virtual void onInputEvent(const std::shared_ptr<InputDeviceNode>& node, InputEvent& event,
            nsecs_t event_time) override {
        mInputCb(node, event, event_time);
    }
    virtual void onDeviceAdded(const std::shared_ptr<InputDeviceNode>& node) override {
        mDeviceAddedCb(node);
    }
    virtual void onDeviceRemoved(const std::shared_ptr<InputDeviceNode>& node) override {
        mDeviceRemovedCb(node);
    }

private:
    InputCbFunc mInputCb;
    DeviceCbFunc mDeviceAddedCb;
    DeviceCbFunc mDeviceRemovedCb;
};

class InputHubTest : public ::testing::Test {
 protected:
     virtual void SetUp() {
         mCallback = std::make_shared<TestInputCallback>();
         mInputHub = std::make_shared<InputHub>(mCallback);
     }

     std::shared_ptr<TestInputCallback> mCallback;
     std::shared_ptr<InputHub> mInputHub;
};

TEST_F(InputHubTest, testWake) {
    // Call wake() after 100ms.
    auto f = delay_async(100ms, [&]() { EXPECT_EQ(OK, mInputHub->wake()); });

    StopWatch stopWatch("poll");
    EXPECT_EQ(OK, mInputHub->poll());
    int32_t elapsedMillis = ns2ms(stopWatch.elapsedTime());

    EXPECT_NEAR(100, elapsedMillis, TIMING_TOLERANCE_MS);
}

TEST_F(InputHubTest, DISABLED_testDeviceAdded) {
    auto tempDir = std::make_shared<TempDir>();
    std::string pathname;
    // Expect that this callback will run and set handle and pathname.
    mCallback->setDeviceAddedCallback(
            [&](const std::shared_ptr<InputDeviceNode>& node) {
                pathname = node->getPath();
            });

    ASSERT_EQ(OK, mInputHub->registerDevicePath(tempDir->getName()));

    // Create a new file in tempDir after 100ms.
    std::unique_ptr<TempFile> tempFile;
    std::mutex tempFileMutex;
    auto f = delay_async(100ms,
            [&]() {
                std::lock_guard<std::mutex> lock(tempFileMutex);
                tempFile.reset(tempDir->newTempFile());
            });

    StopWatch stopWatch("poll");
    EXPECT_EQ(OK, mInputHub->poll());
    int32_t elapsedMillis = ns2ms(stopWatch.elapsedTime());


    EXPECT_NEAR(100, elapsedMillis, TIMING_TOLERANCE_MS);
    std::lock_guard<std::mutex> lock(tempFileMutex);
    EXPECT_EQ(tempFile->getName(), pathname);
}

TEST_F(InputHubTest, DISABLED_testDeviceRemoved) {
    // Create a temp dir and file. Save its name and handle (to be filled in
    // once InputHub scans the dir).
    auto tempDir = std::make_unique<TempDir>();
    auto deviceFile = std::unique_ptr<TempFile>(tempDir->newTempFile());
    std::string tempFileName(deviceFile->getName());

    std::shared_ptr<InputDeviceNode> tempNode;
    // Expect that these callbacks will run for the above device file.
    mCallback->setDeviceAddedCallback(
            [&](const std::shared_ptr<InputDeviceNode>& node) {
                tempNode = node;
            });
    mCallback->setDeviceRemovedCallback(
            [&](const std::shared_ptr<InputDeviceNode>& node) {
                EXPECT_EQ(tempNode, node);
            });

    ASSERT_EQ(OK, mInputHub->registerDevicePath(tempDir->getName()));
    // Ensure that tempDir was scanned to find the device.
    ASSERT_TRUE(tempNode != nullptr);

    auto f = delay_async(100ms, [&]() { deviceFile.reset(); });

    StopWatch stopWatch("poll");
    EXPECT_EQ(OK, mInputHub->poll());
    int32_t elapsedMillis = ns2ms(stopWatch.elapsedTime());

    EXPECT_NEAR(100, elapsedMillis, TIMING_TOLERANCE_MS);
}

TEST_F(InputHubTest, DISABLED_testInputEvent) {
    // Create a temp dir and file. Save its name and handle (to be filled in
    // once InputHub scans the dir.)
    auto tempDir = std::make_unique<TempDir>();
    auto deviceFile = std::unique_ptr<TempFile>(tempDir->newTempFile());
    std::string tempFileName(deviceFile->getName());

    // Send a key event corresponding to HOME.
    struct input_event iev;
    iev.time = { 1, 0 };
    iev.type = EV_KEY;
    iev.code = KEY_HOME;
    iev.value = 0x01;

    auto inputDelayMs = 100ms;
    auto f = delay_async(inputDelayMs, [&] {
                ssize_t nWrite = TEMP_FAILURE_RETRY(write(deviceFile->getFd(), &iev, sizeof(iev)));

                ASSERT_EQ(static_cast<ssize_t>(sizeof(iev)), nWrite) << "could not write to "
                    << deviceFile->getFd() << ". errno: " << errno;
            });

    // Expect this callback to run when the input event is read.
    nsecs_t expectedWhen = systemTime(CLOCK_MONOTONIC) + ms2ns(inputDelayMs.count());
    mCallback->setInputCallback(
            [&](const std::shared_ptr<InputDeviceNode>& node, InputEvent& event,
                nsecs_t event_time) {
                EXPECT_NEAR(expectedWhen, event_time, ms2ns(TIMING_TOLERANCE_MS));
                EXPECT_EQ(s2ns(1), event.when);
                EXPECT_EQ(tempFileName, node->getPath());
                EXPECT_EQ(EV_KEY, event.type);
                EXPECT_EQ(KEY_HOME, event.code);
                EXPECT_EQ(0x01, event.value);
            });
    ASSERT_EQ(OK, mInputHub->registerDevicePath(tempDir->getName()));

    StopWatch stopWatch("poll");
    EXPECT_EQ(OK, mInputHub->poll());
    int32_t elapsedMillis = ns2ms(stopWatch.elapsedTime());

    EXPECT_NEAR(100, elapsedMillis, TIMING_TOLERANCE_MS);
}

TEST_F(InputHubTest, DISABLED_testCallbackOrder) {
    // Create two "devices": one to receive input and the other to go away.
    auto tempDir = std::make_unique<TempDir>();
    auto deviceFile1 = std::unique_ptr<TempFile>(tempDir->newTempFile());
    auto deviceFile2 = std::unique_ptr<TempFile>(tempDir->newTempFile());
    std::string tempFileName(deviceFile2->getName());

    bool inputCallbackFinished = false, deviceCallbackFinished = false;

    // Setup the callback for input events. Should run before the device
    // callback.
    mCallback->setInputCallback(
            [&](const std::shared_ptr<InputDeviceNode>&, InputEvent&, nsecs_t) {
                ASSERT_FALSE(deviceCallbackFinished);
                inputCallbackFinished = true;
            });

    // Setup the callback for device removal. Should run after the input
    // callback.
    mCallback->setDeviceRemovedCallback(
            [&](const std::shared_ptr<InputDeviceNode>& node) {
                ASSERT_TRUE(inputCallbackFinished)
                    << "input callback did not run before device changed callback";
                // Make sure the correct device was removed.
                EXPECT_EQ(tempFileName, node->getPath());
                deviceCallbackFinished = true;
            });
    ASSERT_EQ(OK, mInputHub->registerDevicePath(tempDir->getName()));

    auto f = delay_async(100ms,
            [&]() {
                // Delete the second device file first.
                deviceFile2.reset();

                // Then inject an input event into the first device.
                struct input_event iev;
                iev.time = { 1, 0 };
                iev.type = EV_KEY;
                iev.code = KEY_HOME;
                iev.value = 0x01;

                ssize_t nWrite = TEMP_FAILURE_RETRY(write(deviceFile1->getFd(), &iev, sizeof(iev)));

                ASSERT_EQ(static_cast<ssize_t>(sizeof(iev)), nWrite) << "could not write to "
                    << deviceFile1->getFd() << ". errno: " << errno;
            });

    StopWatch stopWatch("poll");
    EXPECT_EQ(OK, mInputHub->poll());
    int32_t elapsedMillis = ns2ms(stopWatch.elapsedTime());

    EXPECT_NEAR(100, elapsedMillis, TIMING_TOLERANCE_MS);
    EXPECT_TRUE(inputCallbackFinished);
    EXPECT_TRUE(deviceCallbackFinished);
}

}  // namespace tests
}  // namespace android
