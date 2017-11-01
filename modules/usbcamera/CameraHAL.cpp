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

//#define LOG_NDEBUG 0
#define LOG_TAG "UsbCameraHAL"

#include <cstdlib>

#include <log/log.h>
#include <utils/Mutex.h>

#include <hardware/camera_common.h>
#include <hardware/hardware.h>

#include "CameraHAL.h"
#include "UsbCamera.h"

/*
 * This file serves as the entry point to the HAL.  It contains the module
 * structure and functions used by the framework to load and interface to this
 * HAL, as well as the handles to the individual camera devices.
 */

namespace usb_camera_hal {

static CameraHAL gCameraHAL;

CameraHAL::CameraHAL()
  : mCallbacks(NULL) {
    // Should not allocate the camera devices for now, as it is unclear if the device is plugged.

    // Start hotplug thread
    mHotplugThread = new HotplugThread(this);
    mHotplugThread->run("usb-camera-hotplug");
}

CameraHAL::~CameraHAL() {
    // Stop hotplug thread
    {
        android::Mutex::Autolock al(mModuleLock);
        if (mHotplugThread != NULL) {
            mHotplugThread->requestExit();
        }

        // Delete camera device from mCameras
    }

    // Joining done without holding mLock, otherwise deadlocks may ensue
    // as the threads try to access parent states.
    if (mHotplugThread != NULL) {
        mHotplugThread->join();
    }

    delete mHotplugThread;
}

int CameraHAL::getNumberOfCameras() {
    android::Mutex::Autolock al(mModuleLock);
    ALOGV("%s: %zu", __func__, mCameras.size());
    return static_cast<int>(mCameras.size());
}

int CameraHAL::getCameraInfo(int id, struct camera_info* info) {
    android::Mutex::Autolock al(mModuleLock);
    ALOGV("%s: camera id %d: info=%p", __func__, id, info);
    if (id < 0 || id >= static_cast<int>(mCameras.size())) {
        ALOGE("%s: Invalid camera id %d", __func__, id);
        return -ENODEV;
    }

    return mCameras[id]->getInfo(info);
}

int CameraHAL::setCallbacks(const camera_module_callbacks_t *callbacks) {
    ALOGV("%s : callbacks=%p", __func__, callbacks);
    mCallbacks = callbacks;
    return 0;
}

int CameraHAL::open(const hw_module_t* mod, const char* name, hw_device_t** dev) {
    int id;
    char *nameEnd;

    android::Mutex::Autolock al(mModuleLock);
    ALOGV("%s: module=%p, name=%s, device=%p", __func__, mod, name, dev);
    if (*name == '\0') {
        ALOGE("%s: Invalid camera id name is NULL", __func__);
        return -EINVAL;
    }
    id = strtol(name, &nameEnd, 10);
    if (*nameEnd != '\0') {
        ALOGE("%s: Invalid camera id name %s", __func__, name);
        return -EINVAL;
    } else if (id < 0 || id >= static_cast<int>(mCameras.size())) {
        ALOGE("%s: Invalid camera id %d", __func__, id);
        return -ENODEV;
    }
    return mCameras[id]->open(mod, dev);
}

extern "C" {

static int get_number_of_cameras() {
    return gCameraHAL.getNumberOfCameras();
}

static int get_camera_info(int id, struct camera_info* info) {
    return gCameraHAL.getCameraInfo(id, info);
}

static int set_callbacks(const camera_module_callbacks_t *callbacks) {
    return gCameraHAL.setCallbacks(callbacks);
}

static int open_dev(const hw_module_t* mod, const char* name, hw_device_t** dev) {
    return gCameraHAL.open(mod, name, dev);
}

static hw_module_methods_t gCameraModuleMethods = {
    .open = open_dev
};

camera_module_t HAL_MODULE_INFO_SYM __attribute__ ((visibility("default"))) = {
    .common = {
        .tag                = HARDWARE_MODULE_TAG,
        .module_api_version = CAMERA_MODULE_API_VERSION_2_4,
        .hal_api_version    = HARDWARE_HAL_API_VERSION,
        .id                 = CAMERA_HARDWARE_MODULE_ID,
        .name               = "Default USB Camera HAL",
        .author             = "The Android Open Source Project",
        .methods            = &gCameraModuleMethods,
        .dso                = NULL,
        .reserved           = {0},
    },
    .get_number_of_cameras = get_number_of_cameras,
    .get_camera_info       = get_camera_info,
    .set_callbacks         = set_callbacks,
    .get_vendor_tag_ops    = NULL,
    .open_legacy           = NULL,
    .set_torch_mode        = NULL,
    .init                  = NULL,
    .reserved              = {0},
};
} // extern "C"

} // namespace usb_camera_hal
