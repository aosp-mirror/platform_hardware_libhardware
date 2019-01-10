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

#ifndef V4L2_CAMERA_HAL_METADATA_PARTIAL_METADATA_INTERFACE_H_
#define V4L2_CAMERA_HAL_METADATA_PARTIAL_METADATA_INTERFACE_H_

#include <vector>

#include <camera/CameraMetadata.h>

namespace v4l2_camera_hal {

// A subset of metadata.
class PartialMetadataInterface {
 public:
  virtual ~PartialMetadataInterface(){};

  // The metadata tags this partial metadata is responsible for.
  // See system/media/camera/docs/docs.html for descriptions of each tag.
  virtual std::vector<int32_t> StaticTags() const = 0;
  virtual std::vector<int32_t> ControlTags() const = 0;
  virtual std::vector<int32_t> DynamicTags() const = 0;

  // Add all the static properties this partial metadata
  // is responsible for to |metadata|.
  virtual int PopulateStaticFields(android::CameraMetadata* metadata) const = 0;
  // Add all the dynamic states this partial metadata
  // is responsible for to |metadata|.
  virtual int PopulateDynamicFields(
      android::CameraMetadata* metadata) const = 0;
  // Add default request values for a given template type for all the controls
  // this partial metadata owns.
  virtual int PopulateTemplateRequest(
      int template_type, android::CameraMetadata* metadata) const = 0;
  // Check if the requested control values from |metadata| (for controls
  // this partial metadata owns) are supported. Empty/null values for owned
  // control tags indicate no change, and are thus inherently supported.
  // If |metadata| is empty all controls are implicitly supported.
  virtual bool SupportsRequestValues(
      const android::CameraMetadata& metadata) const = 0;
  // Set all the controls this partial metadata
  // is responsible for from |metadata|. Empty/null values for owned control
  // tags indicate no change. If |metadata| is empty no controls should
  // be changed.
  virtual int SetRequestValues(const android::CameraMetadata& metadata) = 0;
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_PARTIAL_METADATA_INTERFACE_H_
