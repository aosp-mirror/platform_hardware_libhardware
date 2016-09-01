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

// Loosely based on hardware/libhardware/modules/camera/ExampleCamera.h

#ifndef V4L2_CAMERA_HAL_V4L2_CAMERA_H_
#define V4L2_CAMERA_HAL_V4L2_CAMERA_H_

#include <array>
#include <string>

#include <camera/CameraMetadata.h>

#include "camera.h"
#include "common.h"
#include "metadata/metadata.h"
#include "v4l2_wrapper.h"

namespace v4l2_camera_hal {
// V4L2Camera is a specific V4L2-supported camera device. The Camera object
// contains all logic common between all cameras (e.g. front and back cameras),
// while a specific camera device (e.g. V4L2Camera) holds all specific
// metadata and logic about that device.
class V4L2Camera : public default_camera_hal::Camera {
 public:
  // Use this method to create V4L2Camera objects. Functionally equivalent
  // to "new V4L2Camera", except that it may return nullptr in case of failure.
  static V4L2Camera* NewV4L2Camera(int id, const std::string path);
  ~V4L2Camera();

 private:
  // Constructor private to allow failing on bad input.
  // Use NewV4L2Camera instead.
  V4L2Camera(int id,
             std::shared_ptr<V4L2Wrapper> v4l2_wrapper,
             std::unique_ptr<Metadata> metadata);

  // default_camera_hal::Camera virtual methods.
  // Connect to the device: open dev nodes, etc.
  int connect() override;
  // Disconnect from the device: close dev nodes, etc.
  void disconnect() override;
  // Initialize static camera characteristics for individual device.
  int initStaticInfo(android::CameraMetadata* out) override;
  // Initialize a template of the given type.
  int initTemplate(int type, android::CameraMetadata* out) override;
  // Initialize device info: resource cost and conflicting devices
  // (/conflicting devices length).
  void initDeviceInfo(camera_info_t* info) override;
  // Extra initialization of device when opened.
  int initDevice() override;
  // Verify stream configuration is device-compatible.
  bool isSupportedStreamSet(default_camera_hal::Stream** streams,
                            int count,
                            uint32_t mode) override;
  // Set up the device for a stream, and get the maximum number of
  // buffers that stream can handle (max_buffers is an output parameter).
  int setupStream(default_camera_hal::Stream* stream,
                  uint32_t* max_buffers) override;
  // Verify settings are valid for a capture with this device.
  bool isValidCaptureSettings(const android::CameraMetadata& settings) override;
  // Set settings for a capture.
  int setSettings(const android::CameraMetadata& new_settings) override;
  // Enqueue a buffer to receive data from the camera.
  int enqueueBuffer(const camera3_stream_buffer_t* camera_buffer) override;
  // Get the shutter time and updated settings for the most recent frame.
  // The metadata parameter is both an input and output; frame-specific
  // result fields should be appended to what is passed in.
  int getResultSettings(android::CameraMetadata* metadata, uint64_t* timestamp);

  // V4L2 helper.
  std::shared_ptr<V4L2Wrapper> device_;
  std::unique_ptr<V4L2Wrapper::Connection> connection_;
  std::unique_ptr<Metadata> metadata_;

  int32_t max_input_streams_;
  std::array<int, 3> max_output_streams_;  // {raw, non-stalling, stalling}.

  DISALLOW_COPY_AND_ASSIGN(V4L2Camera);
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_V4L2_CAMERA_H
