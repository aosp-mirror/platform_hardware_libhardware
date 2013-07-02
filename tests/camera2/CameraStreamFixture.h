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

#ifndef __ANDROID_HAL_CAMERA2_TESTS_STREAM_FIXTURE__
#define __ANDROID_HAL_CAMERA2_TESTS_STREAM_FIXTURE__

#include <gtest/gtest.h>
#include <iostream>

#include <gui/CpuConsumer.h>
#include <gui/Surface.h>
#include <utils/Condition.h>
#include <utils/Mutex.h>
#include <system/camera_metadata.h>

#include "CameraModuleFixture.h"
#include "TestExtensions.h"

namespace android {
namespace camera2 {
namespace tests {

// Format specifier for picking the best format for CPU reading the given device
// version
#define CAMERA_STREAM_AUTO_CPU_FORMAT (-1)

struct CameraStreamParams;

void PrintTo(const CameraStreamParams& p, ::std::ostream* os);

struct CameraStreamParams {
    int mFormat;
    int mHeapCount;

};

inline ::std::ostream& operator<<(::std::ostream& os, const CameraStreamParams &p) {
    PrintTo(p, &os);
    return os;
}

inline void PrintTo(const CameraStreamParams& p, ::std::ostream* os) {
    char fmt[100];
    camera_metadata_enum_snprint(
        ANDROID_SCALER_AVAILABLE_FORMATS, p.mFormat, fmt, sizeof(fmt));

    *os <<  "{ ";
    *os <<  "Format: 0x"  << std::hex << p.mFormat    << ", ";
    *os <<  "Format name: " << fmt << ", ";
    *os <<  "HeapCount: " <<             p.mHeapCount;
    *os << " }";
}

class CameraStreamFixture
    : public CameraModuleFixture</*InfoQuirk*/true> {

public:
    CameraStreamFixture(CameraStreamParams p)
    : CameraModuleFixture(TestSettings::DeviceId()) {
        TEST_EXTENSION_FORKING_CONSTRUCTOR;

        mParam = p;

        SetUp();
    }

    ~CameraStreamFixture() {
        TEST_EXTENSION_FORKING_DESTRUCTOR;

        TearDown();
    }

private:

    void SetUp() {
        TEST_EXTENSION_FORKING_SET_UP;

        CameraModuleFixture::SetUp();

        CameraStreamParams p = mParam;
        sp<CameraDeviceBase> device = mDevice;

        /* use an arbitrary w,h */
        {
            const int tag = ANDROID_SCALER_AVAILABLE_PROCESSED_SIZES;

            const CameraMetadata& staticInfo = device->info();
            camera_metadata_ro_entry entry = staticInfo.find(tag);
            ASSERT_NE(0u, entry.count)
                << "Missing tag android.scaler.availableProcessedSizes";

            ASSERT_LE(2u, entry.count);
            /* this seems like it would always be the smallest w,h
               but we actually make no contract that it's sorted asc */;
            mWidth = entry.data.i32[0];
            mHeight = entry.data.i32[1];
        }
    }
    void TearDown() {
        TEST_EXTENSION_FORKING_TEAR_DOWN;

        // important: shut down HAL before releasing streams
        CameraModuleFixture::TearDown();

        mNativeWindow.clear();
        mCpuConsumer.clear();
        mFrameListener.clear();
    }

protected:
    struct FrameListener : public ConsumerBase::FrameAvailableListener {

        FrameListener() {
            mPendingFrames = 0;
        }

        // CpuConsumer::FrameAvailableListener implementation
        virtual void onFrameAvailable() {
            ALOGV("Frame now available (start)");

            Mutex::Autolock lock(mMutex);
            mPendingFrames++;
            mCondition.signal();

            ALOGV("Frame now available (end)");
        }

        status_t waitForFrame(nsecs_t timeout) {
            status_t res;
            Mutex::Autolock lock(mMutex);
            while (mPendingFrames == 0) {
                res = mCondition.waitRelative(mMutex, timeout);
                if (res != OK) return res;
            }
            mPendingFrames--;
            return OK;
        }

    private:
        Mutex mMutex;
        Condition mCondition;
        int mPendingFrames;
    };

    void CreateStream() {
        sp<CameraDeviceBase> device = mDevice;
        CameraStreamParams p = mParam;

        mCpuConsumer = new CpuConsumer(p.mHeapCount);
        mCpuConsumer->setName(String8("CameraStreamTest::mCpuConsumer"));

        mNativeWindow = new Surface(
            mCpuConsumer->getProducerInterface());

        int format = MapAutoFormat(p.mFormat);

        ASSERT_EQ(OK,
            device->createStream(mNativeWindow,
                mWidth, mHeight, format, /*size (for jpegs)*/0,
                &mStreamId));

        ASSERT_NE(-1, mStreamId);

        // do not make 'this' a FrameListener or the lifetime policy will clash
        mFrameListener = new FrameListener();
        mCpuConsumer->setFrameAvailableListener(mFrameListener);
    }

    void DeleteStream() {
        ASSERT_EQ(OK, mDevice->deleteStream(mStreamId));
    }

    int MapAutoFormat(int format) {
        if (format == CAMERA_STREAM_AUTO_CPU_FORMAT) {
            if (getDeviceVersion() >= CAMERA_DEVICE_API_VERSION_3_0) {
                format = HAL_PIXEL_FORMAT_YCbCr_420_888;
            } else {
                format = HAL_PIXEL_FORMAT_YCrCb_420_SP;
            }
        }
        return format;
    }

    int mWidth;
    int mHeight;

    int mStreamId;

    android::sp<FrameListener>       mFrameListener;
    android::sp<CpuConsumer>         mCpuConsumer;
    android::sp<ANativeWindow>       mNativeWindow;


private:
    CameraStreamParams mParam;
};

}
}
}

#endif
