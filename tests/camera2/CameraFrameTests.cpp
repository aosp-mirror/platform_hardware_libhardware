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

#define LOG_TAG "CameraFrameTest"
//#define LOG_NDEBUG 0
#include <utils/Log.h>

#include "hardware/hardware.h"
#include "hardware/camera2.h"

#include "Camera2Device.h"
#include "utils/StrongPointer.h"

#include <gui/CpuConsumer.h>
#include <gui/SurfaceTextureClient.h>

#include <unistd.h>

#include "CameraStreamFixture.h"

#define CAMERA_FRAME_TIMEOUT    1000000000 //nsecs (1 secs)
#define CAMERA_HEAP_COUNT       2 //HALBUG: 1 means registerBuffers fails
#define CAMERA_FRAME_DEBUGGING  0

using namespace android;
using namespace android::camera2;

namespace android {
namespace camera2 {
namespace tests {

static CameraStreamParams STREAM_PARAMETERS = {
    /*mCameraId*/   0,
    /*mFormat*/     HAL_PIXEL_FORMAT_YCrCb_420_SP,
    /*mHeapCount*/  CAMERA_HEAP_COUNT
};

class CameraFrameTest
    : public ::testing::TestWithParam<int>,
      public CameraStreamFixture {

public:
    CameraFrameTest() : CameraStreamFixture(STREAM_PARAMETERS) {
        if (!HasFatalFailure()) {
            CreateStream();
        }
    }

    ~CameraFrameTest() {
        if (mDevice.get()) {
            mDevice->waitUntilDrained();
        }
        DeleteStream();
    }

    virtual void SetUp() {
    }
    virtual void TearDown() {
    }

protected:

};

TEST_P(CameraFrameTest, GetFrame) {

    if (HasFatalFailure()) {
        return;
    }

    /* Submit a PREVIEW type request, then wait until we get the frame back */
    CameraMetadata previewRequest;
    ASSERT_EQ(OK, mDevice->createDefaultRequest(CAMERA2_TEMPLATE_PREVIEW,
                                                &previewRequest));
    {
        Vector<uint8_t> outputStreamIds;
        outputStreamIds.push(mStreamId);
        ASSERT_EQ(OK, previewRequest.update(ANDROID_REQUEST_OUTPUT_STREAMS,
                                            outputStreamIds));
        if (CAMERA_FRAME_DEBUGGING) {
            int frameCount = 0;
            ASSERT_EQ(OK, previewRequest.update(ANDROID_REQUEST_FRAME_COUNT,
                                                &frameCount, 1));
        }
    }

    if (CAMERA_FRAME_DEBUGGING) {
        previewRequest.dump(STDOUT_FILENO);
    }

    for (int i = 0; i < GetParam(); ++i) {
        ALOGV("Submitting capture request");
        CameraMetadata tmpRequest = previewRequest;
        ASSERT_EQ(OK, mDevice->capture(tmpRequest));
    }

    for (int i = 0; i < GetParam(); ++i) {
        ASSERT_EQ(OK, mDevice->waitForNextFrame(CAMERA_FRAME_TIMEOUT));
        CameraMetadata frameMetadata;
        ASSERT_EQ(OK, mDevice->getNextFrame(&frameMetadata));
    }

}

//FIXME: dont hardcode stream params, and also test multistream
INSTANTIATE_TEST_CASE_P(FrameParameterCombinations, CameraFrameTest,
    testing::Range(1, 10));


}
}
}

