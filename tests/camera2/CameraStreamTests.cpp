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

#define LOG_TAG "CameraStreamTest"
#define LOG_NDEBUG 0
#include <utils/Log.h>

#include "hardware/hardware.h"
#include "hardware/camera2.h"

#include "Camera2Device.h"
#include "utils/StrongPointer.h"

#include <gui/CpuConsumer.h>
#include <gui/SurfaceTextureClient.h>

#include "CameraStreamFixture.h"
#include "TestExtensions.h"

using namespace android;
using namespace android::camera2;

namespace android {
namespace camera2 {
namespace tests {

class CameraStreamTest
    : public ::testing::TestWithParam<CameraStreamParams>,
      public CameraStreamFixture {

public:
    CameraStreamTest() : CameraStreamFixture(GetParam()) {
        TEST_EXTENSION_FORKING_CONSTRUCTOR;
    }

    ~CameraStreamTest() {
        TEST_EXTENSION_FORKING_DESTRUCTOR;
    }

    virtual void SetUp() {
        TEST_EXTENSION_FORKING_SET_UP;
    }
    virtual void TearDown() {
        TEST_EXTENSION_FORKING_TEAR_DOWN;
    }

protected:

};

TEST_P(CameraStreamTest, CreateStream) {

    TEST_EXTENSION_FORKING_INIT;

    ASSERT_NO_FATAL_FAILURE(CreateStream());
    ASSERT_NO_FATAL_FAILURE(DeleteStream());
}

//TODO: use a combinatoric generator
static CameraStreamParams TestParameters[] = {
    {
        /*mFormat*/    HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
        /*mHeapCount*/ 1
    },
    {
        /*mFormat*/    HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
        /*mHeapCount*/ 2
    },
    {
        /*mFormat*/    HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
        /*mHeapCount*/ 3
    },
    {
        /*mFormat*/    HAL_PIXEL_FORMAT_YCrCb_420_SP, // NV21
        /*mHeapCount*/ 1
    },
    {
        /*mFormat*/    HAL_PIXEL_FORMAT_YCrCb_420_SP,
        /*mHeapCount*/ 2
    },
    {
        /*mFormat*/    HAL_PIXEL_FORMAT_YCrCb_420_SP,
        /*mHeapCount*/ 3
    },
    {
        /*mFormat*/    HAL_PIXEL_FORMAT_YV12,
        /*mHeapCount*/ 1
    },
    {
        /*mFormat*/    HAL_PIXEL_FORMAT_YV12,
        /*mHeapCount*/ 2
    },
    {
        /*mFormat*/    HAL_PIXEL_FORMAT_YV12,
        /*mHeapCount*/ 3
    },
    {
        /*mFormat*/    HAL_PIXEL_FORMAT_RAW_SENSOR,
        /*mHeapCount*/ 1
    },
    {
        /*mFormat*/    HAL_PIXEL_FORMAT_RAW_SENSOR,
        /*mHeapCount*/ 2
    },
    {
        /*mFormat*/    HAL_PIXEL_FORMAT_RAW_SENSOR,
        /*mHeapCount*/ 3
    },
};

INSTANTIATE_TEST_CASE_P(StreamParameterCombinations, CameraStreamTest,
    testing::ValuesIn(TestParameters));


}
}
}

