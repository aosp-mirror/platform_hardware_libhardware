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

#ifndef V4L2_CAMERA_HAL_METADATA_V4L2_CONTROL_DELEGATE_H_
#define V4L2_CAMERA_HAL_METADATA_V4L2_CONTROL_DELEGATE_H_

#include "control_delegate_interface.h"
#include "converter_interface.h"
#include "v4l2_wrapper.h"

namespace v4l2_camera_hal {

// A V4L2ControlDelegate routes getting and setting through V4L2
template <typename TMetadata, typename TV4L2 = int32_t>
class V4L2ControlDelegate : public ControlDelegateInterface<TMetadata> {
 public:
  V4L2ControlDelegate(
      std::shared_ptr<V4L2Wrapper> device,
      int control_id,
      std::shared_ptr<ConverterInterface<TMetadata, TV4L2>> converter)
      : device_(std::move(device)),
        control_id_(control_id),
        converter_(std::move(converter)){};

  int GetValue(TMetadata* value) override {
    TV4L2 v4l2_value;
    int res = device_->GetControl(control_id_, &v4l2_value);
    if (res) {
      HAL_LOGE("Failed to get device value for control %d.", control_id_);
      return res;
    }
    return converter_->V4L2ToMetadata(v4l2_value, value);
  };

  int SetValue(const TMetadata& value) override {
    TV4L2 v4l2_value;
    int res = converter_->MetadataToV4L2(value, &v4l2_value);
    if (res) {
      HAL_LOGE("Failed to convert metadata value to V4L2.");
      return res;
    }
    return device_->SetControl(control_id_, v4l2_value);
  };

 private:
  std::shared_ptr<V4L2Wrapper> device_;
  int control_id_;
  std::shared_ptr<ConverterInterface<TMetadata, TV4L2>> converter_;
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_V4L2_CONTROL_DELEGATE_H_
