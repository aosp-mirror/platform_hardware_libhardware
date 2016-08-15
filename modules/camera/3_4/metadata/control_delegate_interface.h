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

#ifndef V4L2_CAMERA_HAL_METADATA_CONTROL_DELEGATE_INTERFACE_H_
#define V4L2_CAMERA_HAL_METADATA_CONTROL_DELEGATE_INTERFACE_H_

#include "state_delegate_interface.h"

namespace v4l2_camera_hal {

// A ControlDelegate extends StateDelegate with a setter method.
template <typename T>
class ControlDelegateInterface : public StateDelegateInterface<T> {
 public:
  virtual ~ControlDelegateInterface(){};

  // ControlDelegates are allowed to be unreliable, so SetValue is best-effort;
  // GetValue immediately after may not match (SetValue may, for example,
  // automatically replace invalid values with valid ones,
  // or have a delay before setting the requested value).
  // Returns 0 on success, error code on failure.
  virtual int SetValue(const T& value) = 0;
  // Children must also override GetValue from StateDelegateInterface.
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_CONTROL_DELEGATE_INTERFACE_H_
