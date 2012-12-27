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

#include <cstdlib>
#include <pthread.h>

//#define LOG_NDEBUG 0
#define LOG_TAG "Camera"
#include <cutils/log.h>

#define ATRACE_TAG (ATRACE_TAG_CAMERA | ATRACE_TAG_HAL)
#include <cutils/trace.h>

#include "Camera.h"

namespace default_camera_hal {

extern "C" {
// Shim passed to the framework to close an opened device.
static int close_device(hw_device_t* dev)
{
    camera2_device_t* cam_dev = reinterpret_cast<camera2_device_t*>(dev);
    Camera* cam = static_cast<Camera*>(cam_dev->priv);
    return cam->close();
}
} // extern "C"

Camera::Camera(int id, const hw_module_t* module)
    : mId(id),
      mBusy(false),
      mMetadata(NULL)
{
    pthread_mutex_init(&mMutex,
                       NULL); // No pthread mutex attributes.

    // TODO: fill in device operations function pointers
    mDevice.common.tag    = HARDWARE_DEVICE_TAG;
    mDevice.common.module = const_cast<hw_module_t*>(module);
    mDevice.common.close  = close_device;
    mDevice.priv          = this;
}

Camera::~Camera()
{
}

int Camera::open()
{
    ALOGV("%s: camera id %d", __func__, mId);
    ATRACE_BEGIN("open");
    pthread_mutex_lock(&mMutex);
    if (mBusy) {
        pthread_mutex_unlock(&mMutex);
        ATRACE_END();
        ALOGE("%s:id%d: Error, device already in use.", __func__, mId);
        return -EBUSY;
    }

    // TODO: open camera dev nodes, etc
    mBusy = true;

    pthread_mutex_unlock(&mMutex);
    ATRACE_END();
    return 0;
}

int Camera::close()
{
    ALOGV("%s: camera id %d", __func__, mId);
    ATRACE_BEGIN("close");
    pthread_mutex_lock(&mMutex);
    if (!mBusy) {
        pthread_mutex_unlock(&mMutex);
        ATRACE_END();
        ALOGE("%s:id%d: Error, close() on not open device.", __func__, mId);
        return -EINVAL;
    }

    // TODO: close camera dev nodes, etc
    mBusy = false;

    pthread_mutex_unlock(&mMutex);
    ATRACE_END();
    return 0;
}

int Camera::getCameraInfo(struct camera_info* info)
{
    ALOGV("%s: camera id %d: info=%p", __func__, mId, info);
    init();
    if (mMetadata == NULL) {
        ALOGE("%s:id%d: unable to initialize metadata.", __func__, mId);
        return -ENOENT;
    }

    // TODO: fill remainder of info struct
    info->static_camera_characteristics = mMetadata;

    return 0;
}

void Camera::init()
{
    ATRACE_BEGIN("init");
    pthread_mutex_lock(&mMutex);
    if (mMetadata != NULL) {
        pthread_mutex_unlock(&mMutex);
        ATRACE_END();
        return;
    }

    // TODO: init metadata, dummy value for now
    mMetadata = allocate_camera_metadata(1,1);

    pthread_mutex_unlock(&mMutex);
    ATRACE_END();
}

} // namespace default_camera_hal
