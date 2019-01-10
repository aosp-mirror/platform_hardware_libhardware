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

#include "metadata_common.h"
#include "partial_metadata_interface.h"

namespace v4l2_camera_hal {

// A Property is a PartialMetadata that only has a single static tag.
template <typename T>
class Property : public PartialMetadataInterface {
 public:
  Property(int32_t tag, T value) : tag_(tag), value_(std::move(value)){};

  virtual std::vector<int32_t> StaticTags() const override { return {tag_}; };

  virtual std::vector<int32_t> ControlTags() const override { return {}; };

  virtual std::vector<int32_t> DynamicTags() const override { return {}; };

  virtual int PopulateStaticFields(
      android::CameraMetadata* metadata) const override {
    return UpdateMetadata(metadata, tag_, value_);
  };

  virtual int PopulateDynamicFields(
      android::CameraMetadata* /*metadata*/) const override {
    return 0;
  };

  virtual int PopulateTemplateRequest(
      int /*template_type*/, android::CameraMetadata* /*metadata*/) const override {
    return 0;
  };

  virtual bool SupportsRequestValues(
      const android::CameraMetadata& /*metadata*/) const override {
    return true;
  };

  virtual int SetRequestValues(
      const android::CameraMetadata& /*metadata*/) override {
    return 0;
  };

 private:
  int32_t tag_;
  T value_;
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_PROPERTY_H_
