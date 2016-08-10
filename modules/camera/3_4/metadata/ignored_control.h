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

#ifndef V4L2_CAMERA_HAL_METADATA_IGNORED_CONTROL_H_
#define V4L2_CAMERA_HAL_METADATA_IGNORED_CONTROL_H_

#include <vector>

#include <gtest/gtest_prod.h>

#include "../common.h"
#include "optioned_control.h"

namespace v4l2_camera_hal {

// A IgnoredControl is a PartialMetadata with a few static options that can
// be chosen from, but they do nothing.
template <typename T>
class IgnoredControl : public OptionedControl<T> {
 public:
  IgnoredControl(int32_t control_tag,
                 int32_t options_tag,
                 std::vector<T> options,
                 T default_option)
      : OptionedControl<T>(control_tag, options_tag, options),
        // Note: default option is not enforced as being in |options|,
        // but it may be confusing if it isn't.
        current_setting_(default_option) {
    HAL_LOG_ENTER();
  };

 protected:
  virtual int GetValue(T* value) const override {
    HAL_LOG_ENTER();
    *value = current_setting_;
    return 0;
  };
  virtual int SetValue(const T& value) override {
    HAL_LOG_ENTER();
    if (!OptionedControl<T>::IsSupported(value)) {
      return -EINVAL;
    }
    current_setting_ = value;
    return 0;
  };

 private:
  T current_setting_;

  FRIEND_TEST(IgnoredControlTest, GetDefaultValue);
  FRIEND_TEST(IgnoredControlTest, SetAndGetValue);
  FRIEND_TEST(IgnoredControlTest, SetAndGetBadValue);
  DISALLOW_COPY_AND_ASSIGN(IgnoredControl);
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_IGNORED_CONTROL_H_
