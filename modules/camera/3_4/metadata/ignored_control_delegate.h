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

#ifndef V4L2_CAMERA_HAL_METADATA_IGNORED_CONTROL_DELEGATE_H_
#define V4L2_CAMERA_HAL_METADATA_IGNORED_CONTROL_DELEGATE_H_

#include "control_delegate_interface.h"

namespace v4l2_camera_hal {

// An IgnoredControlDelegate, as the name implies,
// has a fixed value and ignores all requests to set it.
template <typename T>
class IgnoredControlDelegate : public ControlDelegateInterface<T> {
 public:
  IgnoredControlDelegate(T value) : value_(value){};

  int GetValue(T* value) override {
    *value = value_;
    return 0;
  };
  int SetValue(const T& /*value*/) override { return 0; };

 private:
  const T value_;
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_IGNORED_CONTROL_DELEGATE_H_
