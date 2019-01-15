/*
 * Copyright (C) 2016 The Android Open Source Project
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

// Modified from hardware/libhardware/modules/camera/Camera.h

#ifndef DEFAULT_CAMERA_HAL_CAMERA_H_
#define DEFAULT_CAMERA_HAL_CAMERA_H_

#include <camera/CameraMetadata.h>
#include <hardware/hardware.h>
#include <hardware/camera3.h>
#include <utils/Mutex.h>

#include "capture_request.h"
#include "metadata/metadata.h"
#include "request_tracker.h"
#include "static_properties.h"

namespace default_camera_hal {
// Camera represents a physical camera on a device.
// This is constructed when the HAL module is loaded, one per physical camera.
// TODO(b/29185945): Support hotplugging.
// It is opened by the framework, and must be closed before it can be opened
// again.
// This is an abstract class, containing all logic and data shared between all
// camera devices (front, back, etc) and common to the ISP.
class Camera {
    public:
        // id is used to distinguish cameras. 0 <= id < NUM_CAMERAS.
        // module is a handle to the HAL module, used when the device is opened.
        Camera(int id);
        virtual ~Camera();

        // Common Camera Device Operations (see <hardware/camera_common.h>)
        int openDevice(const hw_module_t *module, hw_device_t **device);
        int getInfo(struct camera_info *info);
        int close();

        // Camera v3 Device Operations (see <hardware/camera3.h>)
        int initialize(const camera3_callback_ops_t *callback_ops);
        int configureStreams(camera3_stream_configuration_t *stream_list);
        const camera_metadata_t *constructDefaultRequestSettings(int type);
        int processCaptureRequest(camera3_capture_request_t *temp_request);
        void dump(int fd);
        int flush();

    protected:
        // Connect to the device: open dev nodes, etc.
        virtual int connect() = 0;
        // Disconnect from the device: close dev nodes, etc.
        virtual void disconnect() = 0;
        // Initialize static camera characteristics for individual device
        virtual int initStaticInfo(android::CameraMetadata* out) = 0;
        // Initialize a template of the given type
        virtual int initTemplate(int type, android::CameraMetadata* out) = 0;
        // Initialize device info: resource cost and conflicting devices
        // (/conflicting devices length)
        virtual void initDeviceInfo(struct camera_info *info) = 0;
        // Separate initialization method for individual devices when opened
        virtual int initDevice() = 0;
        // Verify stream configuration dataspaces and rotation values
        virtual bool validateDataspacesAndRotations(
            const camera3_stream_configuration_t* stream_config) = 0;
        // Set up the streams, including seting usage & max_buffers
        virtual int setupStreams(
            camera3_stream_configuration_t* stream_config) = 0;
        // Verify settings are valid for a capture or reprocessing
        virtual bool isValidRequestSettings(
            const android::CameraMetadata& settings) = 0;
        // Enqueue a request to receive data from the camera
        virtual int enqueueRequest(
            std::shared_ptr<CaptureRequest> request) = 0;
        // Flush in flight buffers.
        virtual int flushBuffers() = 0;


        // Callback for when the device has filled in the requested data.
        // Fills in the result struct, validates the data, sends appropriate
        // notifications, and returns the result to the framework.
        void completeRequest(
            std::shared_ptr<CaptureRequest> request, int err);
        // Prettyprint template names
        const char* templateToString(int type);

    private:
        // Camera device handle returned to framework for use
        camera3_device_t mDevice;
        // Get static info from the device and store it in mStaticInfo.
        int loadStaticInfo();
        // Confirm that a stream configuration is valid.
        int validateStreamConfiguration(
            const camera3_stream_configuration_t* stream_config);
        // Verify settings are valid for reprocessing an input buffer
        bool isValidReprocessSettings(const camera_metadata_t *settings);
        // Pre-process an output buffer
        int preprocessCaptureBuffer(camera3_stream_buffer_t *buffer);
        // Send a shutter notify message with start of exposure time
        void notifyShutter(uint32_t frame_number, uint64_t timestamp);
        // Send an error message and return the errored out result.
        void completeRequestWithError(std::shared_ptr<CaptureRequest> request);
        // Send a capture result for a request.
        void sendResult(std::shared_ptr<CaptureRequest> request);
        // Is type a valid template type (and valid index into mTemplates)
        bool isValidTemplateType(int type);

        // Identifier used by framework to distinguish cameras
        const int mId;
        // CameraMetadata containing static characteristics
        std::unique_ptr<StaticProperties> mStaticInfo;
        // Flag indicating if settings have been set since
        // the last configure_streams() call.
        bool mSettingsSet;
        // Busy flag indicates camera is in use
        bool mBusy;
        // Camera device operations handle shared by all devices
        const static camera3_device_ops_t sOps;
        // Methods used to call back into the framework
        const camera3_callback_ops_t *mCallbackOps;
        // Lock protecting the Camera object for modifications
        android::Mutex mDeviceLock;
        // Lock protecting only static camera characteristics, which may
        // be accessed without the camera device open
        android::Mutex mStaticInfoLock;
        // Standard camera settings templates
        std::unique_ptr<const android::CameraMetadata> mTemplates[CAMERA3_TEMPLATE_COUNT];
        // Track in flight requests.
        std::unique_ptr<RequestTracker> mInFlightTracker;
        android::Mutex mInFlightTrackerLock;
};
}  // namespace default_camera_hal

#endif  // DEFAULT_CAMERA_HAL_CAMERA_H_
