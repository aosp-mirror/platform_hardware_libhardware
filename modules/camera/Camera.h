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

#ifndef CAMERA_H_
#define CAMERA_H_

#include <pthread.h>
#include <hardware/camera2.h>

namespace default_camera_hal {
// Camera represents a physical camera on a device.
// This is constructed when the HAL module is loaded, one per physical camera.
// It is opened by the framework, and must be closed before it can be opened
// again.
// Also, the framework can query for camera metadata with getCameraInfo.
// For the first query, the metadata must first be allocated and initialized,
// but once done it is used for all future calls.
// It is protected by @mMutex, and functions that modify the Camera object hold
// this lock when performing modifications.  Currently these functions are:
// @open, @close, and @init.
class Camera {
    public:
        // id is used to distinguish cameras. 0 <= id < NUM_CAMERAS.
        // module is a handle to the HAL module, used when the device is opened.
        Camera(int id, const hw_module_t* module);
        ~Camera();
        // Open this camera, preparing it for use. Returns nonzero on failure.
        int open();
        // Close this camera. Returns nonzero on failure.
        int close();
        // Query for camera metadata, filling info struct. Returns nonzero if
        // allocation or initialization failed.
        int getCameraInfo(struct camera_info* info);
        // Handle to this device passed to framework in response to open().
        camera2_device_t mDevice;
    private:
        // One-time initialization of camera metadata.
        void init();
        // Identifier used by framework to distinguish cameras
        int mId;
        // True indicates camera is already open.
        bool mBusy;
        // Camera characteristics. NULL means it has not been allocated yet.
        camera_metadata_t* mMetadata;
        // Lock protecting the Camera object for modifications
        pthread_mutex_t mMutex;
};
} // namespace default_camera_hal

#endif // CAMERA_H_
