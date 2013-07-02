/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include <gtest/gtest.h>

#define LOG_TAG "CameraBurstTest"
//#define LOG_NDEBUG 0
#include <utils/Log.h>

#include <cmath>

#include "CameraStreamFixture.h"
#include "TestExtensions.h"

#define CAMERA_FRAME_TIMEOUT    1000000000 //nsecs (1 secs)
#define CAMERA_HEAP_COUNT       2 //HALBUG: 1 means registerBuffers fails
#define CAMERA_BURST_DEBUGGING  0
#define CAMERA_FRAME_BURST_COUNT 10

/* constants for the exposure test */
#define CAMERA_EXPOSURE_DOUBLE  2
#define CAMERA_EXPOSURE_DOUBLING_THRESHOLD 1.0f
#define CAMERA_EXPOSURE_DOUBLING_COUNT 4
#define CAMERA_EXPOSURE_FORMAT CAMERA_STREAM_AUTO_CPU_FORMAT
#define CAMERA_EXPOSURE_STARTING 100000 // 1/10ms, up to 51.2ms with 10 steps

#if CAMERA_BURST_DEBUGGING
#define dout std::cout
#else
#define dout if (0) std::cout
#endif

using namespace android;
using namespace android::camera2;

namespace android {
namespace camera2 {
namespace tests {

static CameraStreamParams STREAM_PARAMETERS = {
    /*mFormat*/     CAMERA_EXPOSURE_FORMAT,
    /*mHeapCount*/  CAMERA_HEAP_COUNT
};

class CameraBurstTest
    : public ::testing::Test,
      public CameraStreamFixture {

public:
    CameraBurstTest() : CameraStreamFixture(STREAM_PARAMETERS) {
        TEST_EXTENSION_FORKING_CONSTRUCTOR;

        if (HasFatalFailure()) {
            return;
        }

        CreateStream();
    }

    ~CameraBurstTest() {
        TEST_EXTENSION_FORKING_DESTRUCTOR;

        if (mDevice.get()) {
            mDevice->waitUntilDrained();
        }
        DeleteStream();
    }

    virtual void SetUp() {
        TEST_EXTENSION_FORKING_SET_UP;
    }
    virtual void TearDown() {
        TEST_EXTENSION_FORKING_TEAR_DOWN;
    }

    /* this assumes the format is YUV420sp or flexible YUV */
    long long TotalBrightness(const CpuConsumer::LockedBuffer& imgBuffer,
                              int *underexposed,
                              int *overexposed) const {

        const uint8_t* buf = imgBuffer.data;
        size_t stride = imgBuffer.stride;

        /* iterate over the Y plane only */
        long long acc = 0;

        *underexposed = 0;
        *overexposed = 0;

        for (size_t y = 0; y < imgBuffer.height; ++y) {
            for (size_t x = 0; x < imgBuffer.width; ++x) {
                const uint8_t p = buf[y * stride + x];

                if (p == 0) {
                    if (underexposed) {
                        ++*underexposed;
                    }
                    continue;
                } else if (p == 255) {
                    if (overexposed) {
                        ++*overexposed;
                    }
                    continue;
                }

                acc += p;
            }
        }

        return acc;
    }
};

TEST_F(CameraBurstTest, ManualExposureControl) {

    TEST_EXTENSION_FORKING_INIT;

    // Range of valid exposure times, in nanoseconds
    int64_t minExp, maxExp;
    {
        camera_metadata_ro_entry exposureTimeRange =
            GetStaticEntry(ANDROID_SENSOR_INFO_EXPOSURE_TIME_RANGE);

        ASSERT_EQ(2u, exposureTimeRange.count);
        minExp = exposureTimeRange.data.i64[0];
        maxExp = exposureTimeRange.data.i64[1];
    }

    dout << "Min exposure is " << minExp;
    dout << " max exposure is " << maxExp << std::endl;

    // Calculate some set of valid exposure times for each request
    int64_t exposures[CAMERA_FRAME_BURST_COUNT];
    exposures[0] = CAMERA_EXPOSURE_STARTING;
    for (int i = 1; i < CAMERA_FRAME_BURST_COUNT; ++i) {
        exposures[i] = exposures[i-1] * CAMERA_EXPOSURE_DOUBLE;
    }
    // Our calculated exposure times should be in [minExp, maxExp]
    EXPECT_LE(minExp, exposures[0])
        << "Minimum exposure range is too high, wanted at most "
        << exposures[0] << "ns";
    EXPECT_GE(maxExp, exposures[CAMERA_FRAME_BURST_COUNT-1])
        << "Maximum exposure range is too low, wanted at least "
        << exposures[CAMERA_FRAME_BURST_COUNT-1] << "ns";

    // Create a preview request, turning off all 3A
    CameraMetadata previewRequest;
    ASSERT_EQ(OK, mDevice->createDefaultRequest(CAMERA2_TEMPLATE_PREVIEW,
                                                &previewRequest));
    {
        Vector<uint8_t> outputStreamIds;
        outputStreamIds.push(mStreamId);
        ASSERT_EQ(OK, previewRequest.update(ANDROID_REQUEST_OUTPUT_STREAMS,
                                            outputStreamIds));

        // Disable all 3A routines
        uint8_t cmOff = static_cast<uint8_t>(ANDROID_CONTROL_MODE_OFF);
        ASSERT_EQ(OK, previewRequest.update(ANDROID_CONTROL_MODE,
                                            &cmOff, 1));

        int requestId = 1;
        ASSERT_EQ(OK, previewRequest.update(ANDROID_REQUEST_ID,
                                            &requestId, 1));

        if (CAMERA_BURST_DEBUGGING) {
            int frameCount = 0;
            ASSERT_EQ(OK, previewRequest.update(ANDROID_REQUEST_FRAME_COUNT,
                                                &frameCount, 1));
        }
    }

    if (CAMERA_BURST_DEBUGGING) {
        previewRequest.dump(STDOUT_FILENO);
    }

    // Submit capture requests
    for (int i = 0; i < CAMERA_FRAME_BURST_COUNT; ++i) {
        CameraMetadata tmpRequest = previewRequest;
        ASSERT_EQ(OK, tmpRequest.update(ANDROID_SENSOR_EXPOSURE_TIME,
                                        &exposures[i], 1));
        ALOGV("Submitting capture request %d with exposure %lld", i,
            exposures[i]);
        dout << "Capture request " << i << " exposure is "
             << (exposures[i]/1e6f) << std::endl;
        ASSERT_EQ(OK, mDevice->capture(tmpRequest));
    }

    dout << "Buffer dimensions " << mWidth << "x" << mHeight << std::endl;

    float brightnesses[CAMERA_FRAME_BURST_COUNT];
    // Get each frame (metadata) and then the buffer. Calculate brightness.
    for (int i = 0; i < CAMERA_FRAME_BURST_COUNT; ++i) {
        ALOGV("Reading capture request %d with exposure %lld", i, exposures[i]);
        ASSERT_EQ(OK, mDevice->waitForNextFrame(CAMERA_FRAME_TIMEOUT));
        ALOGV("Reading capture request-1 %d", i);
        CameraMetadata frameMetadata;
        ASSERT_EQ(OK, mDevice->getNextFrame(&frameMetadata));
        ALOGV("Reading capture request-2 %d", i);

        ASSERT_EQ(OK, mFrameListener->waitForFrame(CAMERA_FRAME_TIMEOUT));
        ALOGV("We got the frame now");

        CpuConsumer::LockedBuffer imgBuffer;
        ASSERT_EQ(OK, mCpuConsumer->lockNextBuffer(&imgBuffer));

        int underexposed, overexposed;
        long long brightness = TotalBrightness(imgBuffer, &underexposed,
                                               &overexposed);
        float avgBrightness = brightness * 1.0f /
                              (mWidth * mHeight - (underexposed + overexposed));
        ALOGV("Total brightness for frame %d was %lld (underexposed %d, "
              "overexposed %d), avg %f", i, brightness, underexposed,
              overexposed, avgBrightness);
        dout << "Average brightness (frame " << i << ") was " << avgBrightness
             << " (underexposed " << underexposed << ", overexposed "
             << overexposed << ")" << std::endl;

        ASSERT_EQ(OK, mCpuConsumer->unlockBuffer(imgBuffer));

        brightnesses[i] = avgBrightness;
    }

    // Calculate max consecutive frame exposure doubling
    float prev = brightnesses[0];
    int doubling_count = 1;
    int max_doubling_count = 0;
    for (int i = 1; i < CAMERA_FRAME_BURST_COUNT; ++i) {
        if (fabs(brightnesses[i] - prev*CAMERA_EXPOSURE_DOUBLE)
            <= CAMERA_EXPOSURE_DOUBLING_THRESHOLD) {
            doubling_count++;
        }
        else {
            max_doubling_count = std::max(max_doubling_count, doubling_count);
            doubling_count = 1;
        }
        prev = brightnesses[i];
    }

    dout << "max doubling count: " << max_doubling_count << std::endl;

    EXPECT_LE(CAMERA_EXPOSURE_DOUBLING_COUNT, max_doubling_count)
      << "average brightness should double at least "
      << CAMERA_EXPOSURE_DOUBLING_COUNT
      << " times over each consecutive frame as the exposure is doubled";
}

}
}
}
