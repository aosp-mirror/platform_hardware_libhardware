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

#ifndef V4L2_CAMERA_HAL_METADATA_TAGGED_CONTROL_OPTIONS_H_
#define V4L2_CAMERA_HAL_METADATA_TAGGED_CONTROL_OPTIONS_H_

#include <memory>

#include "control_options_interface.h"

namespace v4l2_camera_hal {

// A constant tag with a value not used as a real tag
// (since all real tags are unsigned),  to indicate options
// that should not be reported.
// Any class working with TaggedControlOptions should check
// the tag against this value before using it.
static int32_t DO_NOT_REPORT_OPTIONS = -1;

// A TaggedControlOptions wraps a ControlOptions and adds a tag.
template <typename T>
class TaggedControlOptions : public ControlOptionsInterface<T> {
 public:
  TaggedControlOptions(int32_t tag,
                       std::unique_ptr<ControlOptionsInterface<T>> options)
      : tag_(tag), options_(std::move(options)){};

  int32_t tag() { return tag_; };

  virtual std::vector<T> MetadataRepresentation() override {
    return options_->MetadataRepresentation();
  };
  virtual bool IsSupported(const T& value) override {
    return options_->IsSupported(value);
  };
  virtual int DefaultValueForTemplate(int template_type,
                                      T* default_value) override {
    return options_->DefaultValueForTemplate(template_type, default_value);
  }

 private:
  const int32_t tag_;
  std::unique_ptr<ControlOptionsInterface<T>> options_;
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_CONTROL_OPTIONS_INTERFACE_H_
