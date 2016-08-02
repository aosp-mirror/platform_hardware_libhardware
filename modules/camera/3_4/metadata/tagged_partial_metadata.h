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

#ifndef V4L2_CAMERA_HAL_METADATA_TAGGED_PARTIAL_METADATA_H_
#define V4L2_CAMERA_HAL_METADATA_TAGGED_PARTIAL_METADATA_H_

#include "partial_metadata_interface.h"

namespace v4l2_camera_hal {

// A simple base class for PartialMetadataInterfaces that tracks tags.
class TaggedPartialMetadata : public PartialMetadataInterface {
 public:
  TaggedPartialMetadata(std::vector<int32_t> static_tags,
                        std::vector<int32_t> control_tags,
                        std::vector<int32_t> dynamic_tags)
      : static_tags_(std::move(static_tags)),
        control_tags_(std::move(control_tags)),
        dynamic_tags_(std::move(dynamic_tags)){};
  virtual ~TaggedPartialMetadata(){};

  virtual const std::vector<int32_t>& StaticTags() const override {
    return static_tags_;
  };

  virtual const std::vector<int32_t>& ControlTags() const override {
    return control_tags_;
  };

  virtual const std::vector<int32_t>& DynamicTags() const override {
    return dynamic_tags_;
  };

  // Child classes still need to override the Populate/Supported/Set methods.

 private:
  const std::vector<int32_t> static_tags_;
  const std::vector<int32_t> control_tags_;
  const std::vector<int32_t> dynamic_tags_;
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_TAGGED_PARTIAL_METADATA_H_
