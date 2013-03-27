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

#ifndef __ANDROID_HAL_CAMERA2_TESTS_MODULE_FIXTURE__
#define __ANDROID_HAL_CAMERA2_TESTS_MODULE_FIXTURE__

#include <gtest/gtest.h>

#include "hardware/hardware.h"
#include "hardware/camera2.h"

#include "Camera2Device.h"
#include "camera2_utils.h"
#include "TestExtensions.h"

namespace android {
namespace camera2 {
namespace tests {

template <bool InfoQuirk = false>
struct CameraModuleFixture {

    CameraModuleFixture(int CameraID = -1) {
        TEST_EXTENSION_FORKING_CONSTRUCTOR;

        mCameraID = CameraID;
    }

    ~CameraModuleFixture() {
        TEST_EXTENSION_FORKING_DESTRUCTOR;
    }

    camera_metadata_ro_entry GetStaticEntry(uint32_t tag) const {
        const CameraMetadata& staticInfo = mDevice->info();
        camera_metadata_ro_entry entry = staticInfo.find(tag);
        return entry;
    }

    void SetUp() {
        TEST_EXTENSION_FORKING_SET_UP;

        ASSERT_LE(0, hw_get_module(CAMERA_HARDWARE_MODULE_ID,
            (const hw_module_t **)&mModule)) << "Could not load camera module";
        ASSERT_NE((void*)0, mModule);

        mNumberOfCameras = mModule->get_number_of_cameras();
        ASSERT_LE(0, mNumberOfCameras);

        ASSERT_LE(
            CAMERA_MODULE_API_VERSION_2_0, mModule->common.module_api_version)
            << "Wrong module API version";

        /* For using this fixture in other tests only */
        SetUpMixin();
    }

    void TearDown() {
        TEST_EXTENSION_FORKING_TEAR_DOWN;

        TearDownMixin();

        /* important: device must be destructed before closing module,
           since it calls back into HAL */
        mDevice.clear();

        if (!TEST_EXTENSION_FORKING_ENABLED) {
            ASSERT_EQ(0, HWModuleHelpers::closeModule(&mModule->common))
                << "Failed to close camera HAL module";
        }
    }

private:

    void SetUpMixin() {
        /* For using this fixture in other tests only */
        if (mCameraID != -1) {
            EXPECT_LE(0, mCameraID);
            EXPECT_LT(mCameraID, mNumberOfCameras);

            /* HALBUG (Exynos5); crashes if trying to initialize
               before calling get_camera_info */
            if (InfoQuirk) {
                struct camera_info info;
                ASSERT_EQ(OK, mModule->get_camera_info(mCameraID, &info));
            }

            mDevice = new Camera2Device(mCameraID);
            ASSERT_EQ(OK, mDevice->initialize(mModule))
                << "Failed to initialize device " << mCameraID;
        }
    }

    void TearDownMixin() {

    }

protected:
    int mNumberOfCameras;
    camera_module_t *mModule;
    sp<Camera2Device> mDevice;

private:
    int mCameraID;
};


}
}
}

#endif
