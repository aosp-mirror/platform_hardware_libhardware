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

#ifndef V4L2_CAMERA_HAL_METADATA_SLIDER_CONTROL_OPTIONS_H_
#define V4L2_CAMERA_HAL_METADATA_SLIDER_CONTROL_OPTIONS_H_

#include <vector>

#include "control_options_interface.h"

namespace v4l2_camera_hal {

// SliderControlOptions offer a range of acceptable values, inclusive.
template <typename T>
class SliderControlOptions : public ControlOptionsInterface<T> {
 public:
  SliderControlOptions(T min, T max) : min_(min), max_(max) {}

  virtual std::vector<T> MetadataRepresentation() override {
    return {min_, max_};
  };
  virtual bool IsSupported(const T& option) override {
    return option >= min_ && option <= max_;
  };

 private:
  T min_;
  T max_;
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_SLIDER_CONTROL_OPTIONS_H_
