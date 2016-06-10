/*
 * Copyright 2016 The Android Open Source Project
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

#include "V4L2Camera.h"

#include <camera/CameraMetadata.h>
#include <hardware/camera3.h>

#include "Common.h"

namespace v4l2_camera_hal {

V4L2Camera::V4L2Camera(int id, std::string path)
    : default_camera_hal::Camera(id), mDevicePath(std::move(path)) {
  HAL_LOG_ENTER();
}

V4L2Camera::~V4L2Camera() {
  HAL_LOG_ENTER();
}

camera_metadata_t* V4L2Camera::initStaticInfo() {
  HAL_LOG_ENTER();

  android::CameraMetadata metadata(1);
  // TODO(b/29214516): fill this in.
  uint8_t scene_mode = ANDROID_CONTROL_SCENE_MODE_DISABLED;
  metadata.update(ANDROID_CONTROL_AVAILABLE_SCENE_MODES, &scene_mode, 1);

  return metadata.release();
}

void V4L2Camera::initDeviceInfo(camera_info_t* info) {
  HAL_LOG_ENTER();

  // For now, just constants.
  info->facing = CAMERA_FACING_EXTERNAL;
  info->orientation = 0;
  info->resource_cost = 100;
  info->conflicting_devices = nullptr;
  info->conflicting_devices_length = 0;
}

int V4L2Camera::initDevice() {
  HAL_LOG_ENTER();

  // TODO(b/29221795): fill in templates, etc.
  return 0;
}

bool V4L2Camera::isValidCaptureSettings(const camera_metadata_t* settings) {
  HAL_LOG_ENTER();

  // TODO(b): reject capture settings this camera isn't capable of.
  return true;
}

} // namespace v4l2_camera_hal
