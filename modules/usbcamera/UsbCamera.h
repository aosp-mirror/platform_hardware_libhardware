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

#ifndef EXAMPLE_CAMERA_H_
#define EXAMPLE_CAMERA_H_

#include <system/camera_metadata.h>
#include "Camera.h"

namespace usb_camera_hal {
/**
 * UsbCamera is an example for a specific camera device. The Camera instance contains
 * a specific camera device (e.g. UsbCamera) holds all specific metadata and logic about
 * that device.
 */
class UsbCamera : public Camera {
    public:
        explicit UsbCamera(int id);
        ~UsbCamera();

    private:
        // Initialize static camera characteristics for individual device
        int initStaticInfo();
        int openDevice();
        // Initialize whole device (templates/etc) when opened
        int initDevice();
        int flushDevice();
        int closeDevice();
        int processCaptureBuffer(const camera3_stream_buffer_t *in, camera3_stream_buffer_t *out);
        // Initialize each template metadata controls
        int initPreviewTemplate(Metadata m);
        int initStillTemplate(Metadata m);
        int initRecordTemplate(Metadata m);
        int initSnapshotTemplate(Metadata m);
        int initZslTemplate(Metadata m);
        int initManualTemplate(Metadata m);
        // Verify settings are valid for a capture with this device
        bool isValidCaptureSettings(const camera_metadata_t* settings);
};
} // namespace usb_camera_hal

#endif // CAMERA_H_
