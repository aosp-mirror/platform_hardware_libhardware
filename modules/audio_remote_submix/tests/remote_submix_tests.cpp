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

static status_t load_audio_interface(const char* if_name, audio_hw_device_t **dev)
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

    void GenerateData(char* buffer, size_t bufferSize);
    void OpenInputStream(
            const char* address, bool mono, uint32_t sampleRate, audio_stream_in_t** streamIn);
    void OpenOutputStream(
            const char* address, bool mono, uint32_t sampleRate, audio_stream_out_t** streamOut);
    void ReadFromStream(audio_stream_in_t* streamIn, char* buffer, size_t bufferSize);
    void VerifyBufferAllZeroes(char* buffer, size_t bufferSize);
    void VerifyBufferNotZeroes(char* buffer, size_t bufferSize);
    void VerifyOutputInput(
        audio_stream_out_t* streamOut, size_t outBufferSize,
        audio_stream_in_t* streamIn, size_t inBufferSize, size_t repeats);
    void WriteIntoStream(audio_stream_out_t* streamOut, const char* buffer, size_t bufferSize);
    void WriteSomethingIntoStream(audio_stream_out_t* streamOut, size_t bufferSize, size_t repeats);

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

void RemoteSubmixTest::GenerateData(char* buffer, size_t bufferSize) {
    for (size_t i = 0; i < bufferSize; ++i) {
        buffer[i] = static_cast<char>(i & 0x7f);
    }
}

void RemoteSubmixTest::OpenInputStream(
        const char* address, bool mono, uint32_t sampleRate, audio_stream_in_t** streamIn) {
    *streamIn = nullptr;
    struct audio_config configIn = {};
    configIn.channel_mask = mono ? AUDIO_CHANNEL_IN_MONO : AUDIO_CHANNEL_IN_STEREO;
    configIn.sample_rate = sampleRate;
    status_t result = mDev->open_input_stream(mDev,
            AUDIO_IO_HANDLE_NONE, AUDIO_DEVICE_NONE, &configIn,
            streamIn, AUDIO_INPUT_FLAG_NONE, address, AUDIO_SOURCE_DEFAULT);
    ASSERT_EQ(OK, result);
    ASSERT_NE(nullptr, *streamIn);
}

void RemoteSubmixTest::OpenOutputStream(
        const char* address, bool mono, uint32_t sampleRate, audio_stream_out_t** streamOut) {
    *streamOut = nullptr;
    struct audio_config configOut = {};
    configOut.channel_mask = mono ? AUDIO_CHANNEL_OUT_MONO : AUDIO_CHANNEL_OUT_STEREO;
    configOut.sample_rate = sampleRate;
    status_t result = mDev->open_output_stream(mDev,
            AUDIO_IO_HANDLE_NONE, AUDIO_DEVICE_NONE, AUDIO_OUTPUT_FLAG_NONE,
            &configOut, streamOut, address);
    ASSERT_EQ(OK, result);
    ASSERT_NE(nullptr, *streamOut);
}

void RemoteSubmixTest::ReadFromStream(
        audio_stream_in_t* streamIn, char* buffer, size_t bufferSize) {
    ssize_t result = streamIn->read(streamIn, buffer, bufferSize);
    EXPECT_EQ(bufferSize, static_cast<size_t>(result));
}

void RemoteSubmixTest::VerifyBufferAllZeroes(char* buffer, size_t bufferSize) {
    for (size_t i = 0; i < bufferSize; ++i) {
        if (buffer[i]) {
            ADD_FAILURE();
            return;
        }
    }
}

void RemoteSubmixTest::VerifyBufferNotZeroes(char* buffer, size_t bufferSize) {
    for (size_t i = 0; i < bufferSize; ++i) {
        if (buffer[i]) return;
    }
    ADD_FAILURE();
}

void RemoteSubmixTest::VerifyOutputInput(
        audio_stream_out_t* streamOut, size_t outBufferSize,
        audio_stream_in_t* streamIn, size_t inBufferSize,
        size_t repeats) {
    std::unique_ptr<char[]> outBuffer(new char[outBufferSize]), inBuffer(new char[inBufferSize]);
    GenerateData(outBuffer.get(), outBufferSize);
    for (size_t i = 0; i < repeats; ++i) {
        WriteIntoStream(streamOut, outBuffer.get(), outBufferSize);
        memset(inBuffer.get(), 0, inBufferSize);
        ReadFromStream(streamIn, inBuffer.get(), inBufferSize);
        if (inBufferSize == outBufferSize) {
            ASSERT_EQ(0, memcmp(outBuffer.get(), inBuffer.get(), inBufferSize));
        } else {
            VerifyBufferNotZeroes(inBuffer.get(), inBufferSize);
        }
    }
}

void RemoteSubmixTest::WriteIntoStream(
        audio_stream_out_t* streamOut, const char* buffer, size_t bufferSize) {
    ssize_t result = streamOut->write(streamOut, buffer, bufferSize);
    EXPECT_EQ(bufferSize, static_cast<size_t>(result));
}

void RemoteSubmixTest::WriteSomethingIntoStream(
        audio_stream_out_t* streamOut, size_t bufferSize, size_t repeats) {
    std::unique_ptr<char[]> buffer(new char[bufferSize]);
    GenerateData(buffer.get(), bufferSize);
    for (size_t i = 0; i < repeats; ++i) {
        WriteIntoStream(streamOut, buffer.get(), bufferSize);
    }
}

TEST_F(RemoteSubmixTest, InitSuccess) {
    // SetUp must finish with no assertions.
}

// Verifies that when no input was opened, writing into an output stream does not block.
TEST_F(RemoteSubmixTest, OutputDoesNotBlockWhenNoInput) {
    const char* address = "1";
    audio_stream_out_t* streamOut;
    OpenOutputStream(address, true /*mono*/, 48000, &streamOut);
    WriteSomethingIntoStream(streamOut, 1024, 16);
    mDev->close_output_stream(mDev, streamOut);
}

// Verifies that when input is opened but not reading, writing into an output stream does not block.
// !!! Currently does not finish because requires setting a parameter from another thread !!!
// TEST_F(RemoteSubmixTest, OutputDoesNotBlockWhenInputStuck) {
//     const char* address = "1";
//     audio_stream_out_t* streamOut;
//     OpenOutputStream(address, true /*mono*/, 48000, &streamOut);
//     audio_stream_in_t* streamIn;
//     OpenInputStream(address, true /*mono*/, 48000, &streamIn);
//     WriteSomethingIntoStream(streamOut, 1024, 16);
//     mDev->close_input_stream(mDev, streamIn);
//     mDev->close_output_stream(mDev, streamOut);
// }

TEST_F(RemoteSubmixTest, OutputAndInput) {
    const char* address = "1";
    audio_stream_out_t* streamOut;
    OpenOutputStream(address, true /*mono*/, 48000, &streamOut);
    audio_stream_in_t* streamIn;
    OpenInputStream(address, true /*mono*/, 48000, &streamIn);
    const size_t bufferSize = 1024;
    VerifyOutputInput(streamOut, bufferSize, streamIn, bufferSize, 16);
    mDev->close_input_stream(mDev, streamIn);
    mDev->close_output_stream(mDev, streamOut);
}

// Verifies that reading and writing into a closed stream fails gracefully.
TEST_F(RemoteSubmixTest, OutputAndInputAfterClose) {
    const char* address = "1";
    audio_stream_out_t* streamOut;
    OpenOutputStream(address, true /*mono*/, 48000, &streamOut);
    audio_stream_in_t* streamIn;
    OpenInputStream(address, true /*mono*/, 48000, &streamIn);
    mDev->close_input_stream(mDev, streamIn);
    mDev->close_output_stream(mDev, streamOut);
    const size_t bufferSize = 1024;
    std::unique_ptr<char[]> buffer(new char[bufferSize]);
    memset(buffer.get(), 0, bufferSize);
    ASSERT_EQ(0, streamOut->write(streamOut, buffer.get(), bufferSize));
    ASSERT_EQ(static_cast<ssize_t>(bufferSize), streamIn->read(streamIn, buffer.get(), bufferSize));
    VerifyBufferAllZeroes(buffer.get(), bufferSize);
}

TEST_F(RemoteSubmixTest, PresentationPosition) {
    const char* address = "1";
    audio_stream_out_t* streamOut;
    OpenOutputStream(address, true /*mono*/, 48000, &streamOut);
    uint64_t frames;
    struct timespec timestamp;
    EXPECT_EQ(0, streamOut->get_presentation_position(streamOut, &frames, &timestamp));
    EXPECT_EQ(uint64_t{0}, frames);
    uint64_t prevFrames = frames;
    for (size_t i = 0; i < 16; ++i) {
        WriteSomethingIntoStream(streamOut, 1024, 1);
        EXPECT_EQ(0, streamOut->get_presentation_position(streamOut, &frames, &timestamp));
        EXPECT_LE(prevFrames, frames);
        prevFrames = frames;
    }
    mDev->close_output_stream(mDev, streamOut);
}

TEST_F(RemoteSubmixTest, RenderPosition) {
    const char* address = "1";
    audio_stream_out_t* streamOut;
    OpenOutputStream(address, true /*mono*/, 48000, &streamOut);
    uint32_t frames;
    EXPECT_EQ(0, streamOut->get_render_position(streamOut, &frames));
    EXPECT_EQ(0U, frames);
    uint32_t prevFrames = frames;
    for (size_t i = 0; i < 16; ++i) {
        WriteSomethingIntoStream(streamOut, 1024, 1);
        EXPECT_EQ(0, streamOut->get_render_position(streamOut, &frames));
        EXPECT_LE(prevFrames, frames);
        prevFrames = frames;
    }
    mDev->close_output_stream(mDev, streamOut);
}

// This requires ENABLE_CHANNEL_CONVERSION to be set in the HAL module
TEST_F(RemoteSubmixTest, MonoToStereoConversion) {
    const char* address = "1";
    audio_stream_out_t* streamOut;
    OpenOutputStream(address, true /*mono*/, 48000, &streamOut);
    audio_stream_in_t* streamIn;
    OpenInputStream(address, false /*mono*/, 48000, &streamIn);
    const size_t bufferSize = 1024;
    VerifyOutputInput(streamOut, bufferSize, streamIn, bufferSize * 2, 16);
    mDev->close_input_stream(mDev, streamIn);
    mDev->close_output_stream(mDev, streamOut);
}

// This requires ENABLE_CHANNEL_CONVERSION to be set in the HAL module
TEST_F(RemoteSubmixTest, StereoToMonoConversion) {
    const char* address = "1";
    audio_stream_out_t* streamOut;
    OpenOutputStream(address, false /*mono*/, 48000, &streamOut);
    audio_stream_in_t* streamIn;
    OpenInputStream(address, true /*mono*/, 48000, &streamIn);
    const size_t bufferSize = 1024;
    VerifyOutputInput(streamOut, bufferSize * 2, streamIn, bufferSize, 16);
    mDev->close_input_stream(mDev, streamIn);
    mDev->close_output_stream(mDev, streamOut);
}

// This requires ENABLE_RESAMPLING to be set in the HAL module
TEST_F(RemoteSubmixTest, OutputAndInputResampling) {
    const char* address = "1";
    audio_stream_out_t* streamOut;
    OpenOutputStream(address, true /*mono*/, 48000, &streamOut);
    audio_stream_in_t* streamIn;
    OpenInputStream(address, true /*mono*/, 24000, &streamIn);
    const size_t bufferSize = 1024;
    VerifyOutputInput(streamOut, bufferSize * 2, streamIn, bufferSize, 16);
    mDev->close_input_stream(mDev, streamIn);
    mDev->close_output_stream(mDev, streamOut);
}

// This requires ENABLE_LEGACY_INPUT_OPEN to be set in the HAL module
TEST_F(RemoteSubmixTest, OpenInputMultipleTimes) {
    const char* address = "1";
    audio_stream_out_t* streamOut;
    OpenOutputStream(address, true /*mono*/, 48000, &streamOut);
    const size_t streamInCount = 3;
    audio_stream_in_t* streamIn[streamInCount];
    for (size_t i = 0; i < streamInCount; ++i) {
        OpenInputStream(address, true /*mono*/, 48000, &streamIn[i]);
    }
    const size_t bufferSize = 1024;
    for (size_t i = 0; i < streamInCount; ++i) {
        VerifyOutputInput(streamOut, bufferSize, streamIn[i], bufferSize, 16);
        mDev->close_input_stream(mDev, streamIn[i]);
    }
    mDev->close_output_stream(mDev, streamOut);
}
