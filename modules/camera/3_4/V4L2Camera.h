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

#ifndef V4L2_CAMERA_H
#define V4L2_CAMERA_H

#include <string>

#include <nativehelper/ScopedFd.h>
#include <system/camera_metadata.h>

#include "Camera.h"
#include "Common.h"

namespace v4l2_camera_hal {
// V4L2Camera is a specific V4L2-supported camera device. The Camera object
// contains all logic common between all cameras (e.g. front and back cameras),
// while a specific camera device (e.g. V4L2Camera) holds all specific
// metadata and logic about that device.
class V4L2Camera : public default_camera_hal::Camera {
public:
  V4L2Camera(int id, const std::string path);
  ~V4L2Camera();

private:
  // default_camera_hal::Camera virtual methods.
  // Connect to the device: open dev nodes, etc.
  int connect() override;
  // Disconnect from the device: close dev nodes, etc.
  void disconnect() override;
  // Initialize static camera characteristics for individual device.
  int initStaticInfo(camera_metadata_t** out) override;
  // Initialize device info: facing, orientation, resource cost,
  // and conflicting devices (/conflicting devices length).
  void initDeviceInfo(camera_info_t* info) override;
  // Initialize whole device (templates/etc) when opened.
  int initDevice() override;
  // Verify settings are valid for a capture with this device.
  bool isValidCaptureSettings(const camera_metadata_t* settings) override;

  // The camera device path. For example, /dev/video0.
  const std::string mDevicePath;
  // The opened device fd.
  ScopedFd mDeviceFd;

  DISALLOW_COPY_AND_ASSIGN(V4L2Camera);
};

} // namespace v4l2_camera_hal

#endif // V4L2_CAMERA_H
