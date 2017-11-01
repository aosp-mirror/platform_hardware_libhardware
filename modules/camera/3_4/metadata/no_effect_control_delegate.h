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

#ifndef V4L2_CAMERA_HAL_METADATA_NO_EFFECT_CONTROL_DELEGATE_H_
#define V4L2_CAMERA_HAL_METADATA_NO_EFFECT_CONTROL_DELEGATE_H_

#include "control_delegate_interface.h"

namespace v4l2_camera_hal {

// A NoEffectControlDelegate, as the name implies, has no effect.
// The value can be gotten and set, but it does nothing.
template <typename T>
class NoEffectControlDelegate : public ControlDelegateInterface<T> {
 public:
  NoEffectControlDelegate(T default_value) : value_(default_value){};

  int GetValue(T* value) override {
    *value = value_;
    return 0;
  };
  int SetValue(const T& value) override {
    value_ = value;
    return 0;
  };

 private:
  T value_;
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_NO_EFFECT_CONTROL_DELEGATE_H_
