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
#include "tagged_partial_metadata.h"

namespace v4l2_camera_hal {

// A Control is a PartialMetadata with values that can be gotten/set.
template <typename T>
class Control : public TaggedPartialMetadata {
 public:
  Control(int32_t control_tag, std::vector<int32_t> static_tags = {});

  // Child classes still need to implement PopulateStaticFields.
  virtual int PopulateDynamicFields(
      android::CameraMetadata* metadata) const override;
  virtual bool SupportsRequestValues(
      const android::CameraMetadata& metadata) const override;
  virtual int SetRequestValues(
      const android::CameraMetadata& metadata) override;

 protected:
  // Simpler access to tag.
  inline int32_t ControlTag() const { return ControlTags()[0]; }

  // Get/Set the control value. Return non-0 on failure.
  virtual int GetValue(T* value) const = 0;
  virtual int SetValue(const T& value) = 0;
  // Helper to check if val is supported by this control.
  virtual bool IsSupported(const T& val) const = 0;

  DISALLOW_COPY_AND_ASSIGN(Control);
};

// -----------------------------------------------------------------------------

template <typename T>
Control<T>::Control(int32_t control_tag, std::vector<int32_t> static_tags)
    // Controls use the same tag for setting the control
    // and getting the dynamic value.
    : TaggedPartialMetadata(static_tags, {control_tag}, {control_tag}) {
  HAL_LOG_ENTER();
}

template <typename T>
int Control<T>::PopulateDynamicFields(android::CameraMetadata* metadata) const {
  HAL_LOG_ENTER();

  // Populate the current setting.
  T value;
  int res = GetValue(&value);
  if (res) {
    return res;
  }
  return UpdateMetadata(metadata, ControlTag(), value);
}

template <typename T>
bool Control<T>::SupportsRequestValues(
    const android::CameraMetadata& metadata) const {
  HAL_LOG_ENTER();
  if (metadata.isEmpty()) {
    // Implicitly supported.
    return true;
  }

  // Check that the requested setting is in the supported options.
  T requested;
  int res = SingleTagValue(metadata, ControlTag(), &requested);
  if (res == -ENOENT) {
    // Nothing requested of this control, that's fine.
    return true;
  } else if (res) {
    HAL_LOGE("Failure while searching for request value for tag %d",
             ControlTag());
    return false;
  }
  return IsSupported(requested);
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
  int res = SingleTagValue(metadata, ControlTag(), &requested);
  if (res == -ENOENT) {
    // Nothing requested of this control, nothing to do.
    return 0;
  } else if (res) {
    HAL_LOGE("Failure while searching for request value for tag %d",
             ControlTag());
    return res;
  }

  return SetValue(requested);
}

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_CONTROL_H_
