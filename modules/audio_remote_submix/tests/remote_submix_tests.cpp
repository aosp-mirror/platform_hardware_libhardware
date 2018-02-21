/*
 * Copyright (C) 2018 The Android Open Source Project
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

// To run this test (as root):
// 1) Build it
// 2) adb push to /vendor/bin
// 3) adb shell /vendor/bin/r_submix_tests

#define LOG_TAG "RemoteSubmixTest"

#include <memory>

#include <gtest/gtest.h>
#include <hardware/audio.h>
#include <utils/Errors.h>
#include <utils/Log.h>

using namespace android;

static status_t load_audio_interface(const char *if_name, audio_hw_device_t **dev)
{
    const hw_module_t *mod;
    int rc;

    rc = hw_get_module_by_class(AUDIO_HARDWARE_MODULE_ID, if_name, &mod);
    if (rc) {
        ALOGE("%s couldn't load audio hw module %s.%s (%s)", __func__,
                AUDIO_HARDWARE_MODULE_ID, if_name, strerror(-rc));
        goto out;
    }
    rc = audio_hw_device_open(mod, dev);
    if (rc) {
        ALOGE("%s couldn't open audio hw device in %s.%s (%s)", __func__,
                AUDIO_HARDWARE_MODULE_ID, if_name, strerror(-rc));
        goto out;
    }
    if ((*dev)->common.version < AUDIO_DEVICE_API_VERSION_MIN) {
        ALOGE("%s wrong audio hw device version %04x", __func__, (*dev)->common.version);
        rc = BAD_VALUE;
        audio_hw_device_close(*dev);
        goto out;
    }
    return OK;

out:
    *dev = NULL;
    return rc;
}

class RemoteSubmixTest : public testing::Test {
  protected:
    void SetUp() override;
    void TearDown() override;

    void OpenInputStream(const char *address, audio_stream_in_t** streamIn);
    void OpenOutputStream(const char *address, audio_stream_out_t** streamOut);
    void WriteIntoStream(audio_stream_out_t* streamOut, size_t bufferSize, size_t repeats);

    audio_hw_device_t* mDev;
};

void RemoteSubmixTest::SetUp() {
    mDev = nullptr;
    ASSERT_EQ(OK, load_audio_interface(AUDIO_HARDWARE_MODULE_ID_REMOTE_SUBMIX, &mDev));
    ASSERT_NE(nullptr, mDev);
}

void RemoteSubmixTest::TearDown() {
    if (mDev != nullptr) {
        int status = audio_hw_device_close(mDev);
        mDev = nullptr;
        ALOGE_IF(status, "Error closing audio hw device %p: %s", mDev, strerror(-status));
        ASSERT_EQ(0, status);
    }
}

void RemoteSubmixTest::OpenInputStream(const char *address, audio_stream_in_t** streamIn) {
    *streamIn = nullptr;
    struct audio_config configIn = {};
    configIn.channel_mask = AUDIO_CHANNEL_IN_MONO;
    configIn.sample_rate = 48000;
    status_t result = mDev->open_input_stream(mDev,
            AUDIO_IO_HANDLE_NONE, AUDIO_DEVICE_NONE, &configIn,
            streamIn, AUDIO_INPUT_FLAG_NONE, address, AUDIO_SOURCE_DEFAULT);
    ASSERT_EQ(OK, result);
    ASSERT_NE(nullptr, *streamIn);
}

void RemoteSubmixTest::OpenOutputStream(const char *address, audio_stream_out_t** streamOut) {
    *streamOut = nullptr;
    struct audio_config configOut = {};
    configOut.channel_mask = AUDIO_CHANNEL_OUT_MONO;
    configOut.sample_rate = 48000;
    status_t result = mDev->open_output_stream(mDev,
            AUDIO_IO_HANDLE_NONE, AUDIO_DEVICE_NONE, AUDIO_OUTPUT_FLAG_NONE,
            &configOut, streamOut, address);
    ASSERT_EQ(OK, result);
    ASSERT_NE(nullptr, *streamOut);
}

void RemoteSubmixTest::WriteIntoStream(
        audio_stream_out_t* streamOut, size_t bufferSize, size_t repeats) {
    std::unique_ptr<char[]> buffer(new char[bufferSize]);
    for (size_t i = 0; i < repeats; ++i) {
        ssize_t result = streamOut->write(streamOut, buffer.get(), bufferSize);
        EXPECT_EQ(bufferSize, static_cast<size_t>(result));
    }
}

TEST_F(RemoteSubmixTest, InitSuccess) {
    // SetUp must finish with no assertions.
}

// Verifies that when no input was opened, writing into an output stream does not block.
TEST_F(RemoteSubmixTest, OutputDoesNotBlockWhenNoInput) {
    const char *address = "1";
    audio_stream_out_t* streamOut;
    OpenOutputStream(address, &streamOut);
    WriteIntoStream(streamOut, 1024, 16);
    mDev->close_output_stream(mDev, streamOut);
}

// Verifies that when input is opened but not reading, writing into an output stream does not block.
// !!! Currently does not finish because requires setting a parameter from another thread !!!
TEST_F(RemoteSubmixTest, OutputDoesNotBlockWhenInputStuck) {
    const char *address = "1";
    audio_stream_out_t* streamOut;
    OpenOutputStream(address, &streamOut);
    audio_stream_in_t* streamIn;
    OpenInputStream(address, &streamIn);
    WriteIntoStream(streamOut, 1024, 16);
    mDev->close_input_stream(mDev, streamIn);
    mDev->close_output_stream(mDev, streamOut);
}
