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

#include "metadata_reader.h"

// #define LOG_NDEBUG 0
#define LOG_TAG "MetadataReader"
#include <cutils/log.h>
#include <system/camera.h>

#include "metadata_common.h"

namespace default_camera_hal {

MetadataReader::MetadataReader(
    std::unique_ptr<const android::CameraMetadata> metadata)
    : metadata_(std::move(metadata)) {}

MetadataReader::~MetadataReader() {}

int MetadataReader::Facing(int* facing) const {
  uint8_t metadata_facing = 0;
  int res = v4l2_camera_hal::SingleTagValue(
      *metadata_, ANDROID_LENS_FACING, &metadata_facing);
  if (res) {
    ALOGE("%s: Failed to get facing from static metadata.", __func__);
    return res;
  }

  switch (metadata_facing) {
    case (ANDROID_LENS_FACING_FRONT):
      *facing = CAMERA_FACING_FRONT;
      break;
    case (ANDROID_LENS_FACING_BACK):
      *facing = CAMERA_FACING_BACK;
      break;
    case (ANDROID_LENS_FACING_EXTERNAL):
      *facing = CAMERA_FACING_EXTERNAL;
      break;
    default:
      ALOGE("%s: Invalid facing from static metadata: %d.",
            __func__,
            metadata_facing);
      return -EINVAL;
  }
  return 0;
}

int MetadataReader::Orientation(int* orientation) const {
  int32_t metadata_orientation = 0;
  int res = v4l2_camera_hal::SingleTagValue(
      *metadata_, ANDROID_SENSOR_ORIENTATION, &metadata_orientation);
  if (res) {
    ALOGE("%s: Failed to get orientation from static metadata.", __func__);
    return res;
  }

  // Orientation must be 0, 90, 180, or 270.
  if (metadata_orientation < 0 || metadata_orientation > 270 ||
      metadata_orientation % 90 != 0) {
    ALOGE(
        "%s: Invalid orientation %d "
        "(must be a 90-degree increment in [0, 360)).",
        __func__,
        metadata_orientation);
    return -EINVAL;
  }

  *orientation = static_cast<int>(metadata_orientation);
  return 0;
}

}  // namespace default_camera_hal
