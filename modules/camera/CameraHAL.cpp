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
#include <hardware/camera2.h>

//#define LOG_NDEBUG 0
#define LOG_TAG "CameraHAL"
#include <cutils/log.h>

#include "Camera.h"

/*
 * This file serves as the entry point to the HAL.  It contains the module
 * structure and functions used by the framework to load and interface to this
 * HAL, as well as the handles to the individual camera devices.
 */

namespace default_camera_hal {

enum camera_id_t {
    CAMERA_A,
    CAMERA_B,
    NUM_CAMERAS
};

// Symbol used by the framework when loading the HAL, defined below.
extern "C" camera_module_t HAL_MODULE_INFO_SYM;

// Camera devices created when the module is loaded. See Camera.h
static Camera gCameras[NUM_CAMERAS] = {
    Camera(CAMERA_A, &HAL_MODULE_INFO_SYM.common),
    Camera(CAMERA_B, &HAL_MODULE_INFO_SYM.common)
};

extern "C" {

static int get_number_of_cameras()
{
    ALOGV("%s", __func__);
    return NUM_CAMERAS;
}

static int get_camera_info(int id, struct camera_info* info)
{
    ALOGV("%s: camera id %d: info=%p", __func__, id, info);
    if (id < 0 || id >= NUM_CAMERAS) {
        ALOGE("%s: Invalid camera id %d", __func__, id);
        return -ENODEV;
    }
    return gCameras[id].getCameraInfo(info);
}

static int open_device(const hw_module_t* module,
                       const char* name,
                       hw_device_t** device)
{
    ALOGV("%s: module=%p, name=%s, device=%p", __func__, module, name, device);
    char *nameEnd;
    int id;

    id = strtol(name, &nameEnd, 10);
    if (nameEnd != NULL) {
        ALOGE("%s: Invalid camera id name %s", __func__, name);
        return -EINVAL;
    } else if (id < 0 || id >= NUM_CAMERAS) {
        ALOGE("%s: Invalid camera id %d", __func__, id);
        return -ENODEV;
    }
    *device = &gCameras[id].mDevice.common;
    return gCameras[id].open();
}

static hw_module_methods_t gCameraModuleMethods = {
    open : open_device
};

camera_module_t HAL_MODULE_INFO_SYM = {
    common : {
        tag                : HARDWARE_MODULE_TAG,
        module_api_version : CAMERA_MODULE_API_VERSION_2_0,
        hal_api_version    : HARDWARE_HAL_API_VERSION,
        id                 : CAMERA_HARDWARE_MODULE_ID,
        name               : "Reference Camera v2 HAL",
        author             : "The Android Open Source Project",
        methods            : &gCameraModuleMethods,
        dso                : NULL,
        reserved           : {0},
    },
    get_number_of_cameras : get_number_of_cameras,
    get_camera_info       : get_camera_info
};

} // extern "C"
} // namespace default_camera_hal
