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

#ifndef V4L2_CAMERA_HAL_METADATA_CONTROL_H_
#define V4L2_CAMERA_HAL_METADATA_CONTROL_H_

#include <vector>

#include <system/camera_metadata.h>

#include "../common.h"
#include "menu_control_options.h"
#include "metadata_common.h"
#include "no_effect_control_delegate.h"
#include "partial_metadata_interface.h"
#include "tagged_control_delegate.h"
#include "tagged_control_options.h"

namespace v4l2_camera_hal {

// A Control is a PartialMetadata with values that can be gotten/set.
template <typename T>
class Control : public PartialMetadataInterface {
 public:
  // Options are optional (i.e. nullable), delegate is not.
  Control(std::unique_ptr<TaggedControlDelegate<T>> delegate,
          std::unique_ptr<TaggedControlOptions<T>> options = nullptr);

  virtual std::vector<int32_t> StaticTags() const override;
  virtual std::vector<int32_t> ControlTags() const override;
  virtual std::vector<int32_t> DynamicTags() const override;

  virtual int PopulateStaticFields(
      android::CameraMetadata* metadata) const override;
  virtual int PopulateDynamicFields(
      android::CameraMetadata* metadata) const override;
  virtual bool SupportsRequestValues(
      const android::CameraMetadata& metadata) const override;
  virtual int SetRequestValues(
      const android::CameraMetadata& metadata) override;

  // Factory methods for some common combinations of delegates & options.
  // NoEffectMenuControl: Some menu options, but they have no effect.
  // The default value will be the first element of |options|.
  static std::unique_ptr<Control<T>> NoEffectMenuControl(
      int32_t delegate_tag, int32_t options_tag, const std::vector<T>& options);

 protected:
  std::unique_ptr<TaggedControlDelegate<T>> delegate_;
  std::unique_ptr<TaggedControlOptions<T>> options_;

  DISALLOW_COPY_AND_ASSIGN(Control);
};

// -----------------------------------------------------------------------------

template <typename T>
Control<T>::Control(std::unique_ptr<TaggedControlDelegate<T>> delegate,
                    std::unique_ptr<TaggedControlOptions<T>> options)
    : delegate_(std::move(delegate)), options_(std::move(options)) {
  HAL_LOG_ENTER();
}

template <typename T>
std::vector<int32_t> Control<T>::StaticTags() const {
  std::vector<int32_t> result;
  if (options_) {
    result.push_back(options_->tag());
  }
  return result;
}

template <typename T>
std::vector<int32_t> Control<T>::ControlTags() const {
  return {delegate_->tag()};
}

template <typename T>
std::vector<int32_t> Control<T>::DynamicTags() const {
  return {delegate_->tag()};
}

template <typename T>
int Control<T>::PopulateStaticFields(android::CameraMetadata* metadata) const {
  HAL_LOG_ENTER();

  if (!options_) {
    HAL_LOGV("No options for control, nothing to populate.");
    return 0;
  }

  return UpdateMetadata(
      metadata, options_->tag(), options_->MetadataRepresentation());
}

template <typename T>
int Control<T>::PopulateDynamicFields(android::CameraMetadata* metadata) const {
  HAL_LOG_ENTER();

  // Populate the current setting.
  T value;
  int res = delegate_->GetValue(&value);
  if (res) {
    return res;
  }
  return UpdateMetadata(metadata, delegate_->tag(), value);
}

template <typename T>
bool Control<T>::SupportsRequestValues(
    const android::CameraMetadata& metadata) const {
  HAL_LOG_ENTER();
  if (metadata.isEmpty()) {
    // Implicitly supported.
    return true;
  }

  // Get the requested setting for this control.
  T requested;
  int res = SingleTagValue(metadata, delegate_->tag(), &requested);
  if (res == -ENOENT) {
    // Nothing requested of this control, that's fine.
    return true;
  } else if (res) {
    HAL_LOGE("Failure while searching for request value for tag %d",
             delegate_->tag());
    return false;
  }

  // Check that the requested setting is in the supported options.
  if (!options_) {
    HAL_LOGV("No options for control %d; request implicitly supported.",
             delegate_->tag());
    return true;
  }
  return options_->IsSupported(requested);
}

template <typename T>
int Control<T>::SetRequestValues(const android::CameraMetadata& metadata) {
  HAL_LOG_ENTER();
  if (metadata.isEmpty()) {
    // No changes necessary.
    return 0;
  }

  // Get the requested value.
  T requested;
  int res = SingleTagValue(metadata, delegate_->tag(), &requested);
  if (res == -ENOENT) {
    // Nothing requested of this control, nothing to do.
    return 0;
  } else if (res) {
    HAL_LOGE("Failure while searching for request value for tag %d",
             delegate_->tag());
    return res;
  }

  // Check that the value is supported.
  if (options_ && !options_->IsSupported(requested)) {
    HAL_LOGE("Unsupported value requested for control %d.", delegate_->tag());
    return -EINVAL;
  }

  return delegate_->SetValue(requested);
}

template <typename T>
std::unique_ptr<Control<T>> Control<T>::NoEffectMenuControl(
    int32_t delegate_tag, int32_t options_tag, const std::vector<T>& options) {
  HAL_LOG_ENTER();

  if (options.empty()) {
    HAL_LOGE("At least one option must be provided.");
    return nullptr;
  }

  return std::make_unique<Control<T>>(
      std::make_unique<TaggedControlDelegate<T>>(
          delegate_tag,
          std::make_unique<NoEffectControlDelegate<T>>(options[0])),
      std::make_unique<TaggedControlOptions<T>>(
          options_tag, std::make_unique<MenuControlOptions<T>>(options)));
}

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_CONTROL_H_
