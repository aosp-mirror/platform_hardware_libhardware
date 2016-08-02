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

#ifndef V4L2_CAMERA_HAL_METADATA_OPTIONED_CONTROL_H_
#define V4L2_CAMERA_HAL_METADATA_OPTIONED_CONTROL_H_

#include <algorithm>
#include <vector>

#include <gtest/gtest_prod.h>

#include "../common.h"
#include "control.h"

namespace v4l2_camera_hal {

// An OptionedControl is a Control with a fixed list of options that can be
// selected from.
template <typename T>
class OptionedControl : public Control<T> {
 public:
  OptionedControl(int32_t control_tag, int32_t options_tag,
                  std::vector<T> options);

  virtual int PopulateStaticFields(
      android::CameraMetadata* metadata) const override;

 protected:
  // Simpler access to tag.
  inline int32_t OptionsTag() const { return Control<T>::StaticTags()[0]; }

  // Child classes still need to implement Get/Set from Control.
  virtual bool IsSupported(const T& val) const override;

 private:
  const std::vector<T> options_;

  FRIEND_TEST(OptionedControlTest, IsSupported);

  DISALLOW_COPY_AND_ASSIGN(OptionedControl);
};

// -----------------------------------------------------------------------------

template <typename T>
OptionedControl<T>::OptionedControl(int32_t control_tag, int32_t options_tag,
                                    std::vector<T> options)
    : Control<T>(control_tag, {options_tag}), options_(std::move(options)) {
  HAL_LOG_ENTER();
}

template <typename T>
int OptionedControl<T>::PopulateStaticFields(
    android::CameraMetadata* metadata) const {
  HAL_LOG_ENTER();

  // Populate the available options.
  return Control<T>::UpdateMetadata(metadata, OptionsTag(), options_);
}

template <typename T>
bool OptionedControl<T>::IsSupported(const T& value) const {
  return (std::find(options_.begin(), options_.end(), value) != options_.end());
}

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_OPTIONED_CONTROL_H_
