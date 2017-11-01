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

#ifndef V4L2_CAMERA_HAL_METADATA_TAGGED_CONTROL_DELEGATE_H_
#define V4L2_CAMERA_HAL_METADATA_TAGGED_CONTROL_DELEGATE_H_

#include <memory>

#include "control_delegate_interface.h"

namespace v4l2_camera_hal {

// A TaggedControlDelegate wraps a ControlDelegate and adds a tag.
template <typename T>
class TaggedControlDelegate : public ControlDelegateInterface<T> {
 public:
  TaggedControlDelegate(int32_t tag,
                        std::unique_ptr<ControlDelegateInterface<T>> delegate)
      : tag_(tag), delegate_(std::move(delegate)){};

  int32_t tag() { return tag_; };

  virtual int GetValue(T* value) override {
    return delegate_->GetValue(value);
  };
  virtual int SetValue(const T& value) override {
    return delegate_->SetValue(value);
  };

 private:
  const int32_t tag_;
  std::unique_ptr<ControlDelegateInterface<T>> delegate_;
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_CONTROL_DELEGATE_INTERFACE_H_
