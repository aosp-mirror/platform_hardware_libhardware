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

#ifndef V4L2_CAMERA_HAL_METADATA_STATE_H_
#define V4L2_CAMERA_HAL_METADATA_STATE_H_

#include "common.h"
#include "metadata_common.h"
#include "partial_metadata_interface.h"
#include "state_delegate_interface.h"

namespace v4l2_camera_hal {

// A State is a PartialMetadata that only has a single dynamic value.
template <typename T>
class State : public PartialMetadataInterface {
 public:
  State(int32_t tag, std::unique_ptr<StateDelegateInterface<T>> delegate)
      : tag_(tag), delegate_(std::move(delegate)){};

  virtual std::vector<int32_t> StaticTags() const override { return {}; };
  virtual std::vector<int32_t> ControlTags() const override { return {}; };
  virtual std::vector<int32_t> DynamicTags() const override { return {tag_}; };

  virtual int PopulateStaticFields(
      android::CameraMetadata* metadata) const override;
  virtual int PopulateDynamicFields(
      android::CameraMetadata* metadata) const override;
  virtual int PopulateTemplateRequest(
      int template_type, android::CameraMetadata* metadata) const override;
  virtual bool SupportsRequestValues(
      const android::CameraMetadata& metadata) const override;
  virtual int SetRequestValues(
      const android::CameraMetadata& metadata) override;

 private:
  int32_t tag_;
  std::unique_ptr<StateDelegateInterface<T>> delegate_;
};

// -----------------------------------------------------------------------------

template <typename T>
int State<T>::PopulateStaticFields(android::CameraMetadata* /*metadata*/) const {
  HAL_LOG_ENTER();
  return 0;
}

template <typename T>
int State<T>::PopulateDynamicFields(android::CameraMetadata* metadata) const {
  HAL_LOG_ENTER();

  T value;
  int res = delegate_->GetValue(&value);
  if (res) {
    return res;
  }
  return UpdateMetadata(metadata, tag_, value);
};

template <typename T>
int State<T>::PopulateTemplateRequest(int /*template_type*/,
                                      android::CameraMetadata* /*metadata*/) const {
  HAL_LOG_ENTER();
  return 0;
};

template <typename T>
bool State<T>::SupportsRequestValues(
    const android::CameraMetadata& /*metadata*/) const {
  HAL_LOG_ENTER();
  return true;
};

template <typename T>
int State<T>::SetRequestValues(const android::CameraMetadata& /*metadata*/) {
  HAL_LOG_ENTER();
  return 0;
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_STATE_H_
