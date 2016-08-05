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

#ifndef V4L2_CAMERA_HAL_METADATA_PROPERTY_H_
#define V4L2_CAMERA_HAL_METADATA_PROPERTY_H_

#include "../common.h"
#include "metadata_common.h"
#include "tagged_partial_metadata.h"

namespace v4l2_camera_hal {

// A Property is a PartialMetadata that only has a single static tag.
template <typename T>
class Property : public TaggedPartialMetadata {
 public:
  Property(int32_t tag) : TaggedPartialMetadata({tag}, {}, {}){};

  virtual int PopulateStaticFields(
      android::CameraMetadata* metadata) const override {
    HAL_LOG_ENTER();
    return UpdateMetadata(metadata, Tag(), Value());
  };

  virtual int PopulateDynamicFields(
      android::CameraMetadata* metadata) const override {
    HAL_LOG_ENTER();
    return 0;
  };

  virtual bool SupportsRequestValues(
      const android::CameraMetadata& metadata) const override {
    HAL_LOG_ENTER();
    return true;
  };

  virtual int SetRequestValues(
      const android::CameraMetadata& metadata) override {
    HAL_LOG_ENTER();
    return 0;
  };

 protected:
  // Simpler access to tag.
  inline int32_t Tag() const { return StaticTags()[0]; }
  // Get the value associated with this property.
  virtual const T& Value() const = 0;
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_PROPERTY_H_
