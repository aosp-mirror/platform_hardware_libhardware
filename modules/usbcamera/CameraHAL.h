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

#ifndef CAMERA_HAL_H_
#define CAMERA_HAL_H_

#include <hardware/hardware.h>
#include <hardware/camera_common.h>
#include <utils/Vector.h>
#include <utils/Mutex.h>
#include "HotplugThread.h"
#include "Camera.h"

namespace usb_camera_hal {

class HotplugThread;

/**
 * CameraHAL contains all module state that isn't specific to an individual camera device
 */
class CameraHAL {
    public:
        CameraHAL();
        ~CameraHAL();

        // Camera Module Interface (see <hardware/camera_common.h>)
        int getNumberOfCameras();
        int getCameraInfo(int camera_id, struct camera_info *info);
        int setCallbacks(const camera_module_callbacks_t *callbacks);
        void getVendorTagOps(vendor_tag_ops_t* ops);

        // Hardware Module Interface (see <hardware/hardware.h>)
        int open(const hw_module_t* mod, const char* name, hw_device_t** dev);

    private:
        // Callback handle
        const camera_module_callbacks_t *mCallbacks;
        android::Vector<Camera*> mCameras;
        // Lock to protect the module method calls.
        android::Mutex mModuleLock;
        // Hot plug thread managing camera hot plug.
        HotplugThread *mHotplugThread;

};
} // namespace usb_camera_hal

#endif // CAMERA_HAL_H_
