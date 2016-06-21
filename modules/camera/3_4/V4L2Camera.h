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

#include <array>
#include <string>
#include <vector>

#include <nativehelper/ScopedFd.h>
#include <system/camera_metadata.h>

#include "ArrayVector.h"
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

  bool mTemplatesInitialized;
  int initTemplates();

  // Camera characteristics.
  bool mCharacteristicsInitialized;  // If false, characteristics are invalid.
  // Fixed characteristics.
  float mAperture;
  float mFilterDensity;
  float mFocalLength;
  int32_t mOrientation;
  std::array<float, 2> mPhysicalSize;  // {width, height}, in mm.
  std::array<int32_t, 4> mPixelArraySize;  // {xmin, ymin, width, height}.
  uint8_t mCropType;
  float mMaxZoom;
  std::array<int32_t, 2> mAeCompensationRange;  // {min, max}.
  camera_metadata_rational mAeCompensationStep;
  uint8_t mAeLockAvailable;
  uint8_t mAwbLockAvailable;
  uint8_t mFlashAvailable;
  float mFocusDistance;
  // Variable characteristics available options.
  std::vector<uint8_t> mAeModes;
  std::vector<uint8_t> mAeAntibandingModes;
  std::vector<uint8_t> mAfModes;
  std::vector<uint8_t> mAwbModes;
  std::vector<uint8_t> mSceneModes;
  std::vector<uint8_t> mControlModes;
  std::vector<uint8_t> mEffects;
  std::vector<uint8_t> mLeds;
  std::vector<uint8_t> mOpticalStabilizationModes;
  std::vector<uint8_t> mVideoStabilizationModes;
  // {format, width, height, direction} (input or output).
  ArrayVector<int32_t, 4> mStreamConfigs;
  // {format, width, height, duration} (duration in ns).
  ArrayVector<int64_t, 4> mMinFrameDurations;
  int64_t mMaxFrameDuration;
  // {format, width, height, duration} (duration in ns).
  ArrayVector<int64_t, 4> mStallDurations;
  // {min, max} (in fps).
  ArrayVector<int32_t, 2> mFpsRanges;

  // Initialize characteristics and set mCharacteristicsInitialized to True.
  int initCharacteristics();

  DISALLOW_COPY_AND_ASSIGN(V4L2Camera);
};

} // namespace v4l2_camera_hal

#endif // V4L2_CAMERA_H
