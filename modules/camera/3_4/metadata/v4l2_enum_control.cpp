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

#include "v4l2_enum_control.h"

#include "../common.h"

namespace v4l2_camera_hal {

V4L2EnumControl* V4L2EnumControl::NewV4L2EnumControl(
    std::shared_ptr<V4L2Wrapper> device, int v4l2_control, int32_t control_tag,
    int32_t options_tag, const std::map<int32_t, uint8_t>& v4l2_to_metadata) {
  HAL_LOG_ENTER();

  // Query the device.
  v4l2_query_ext_ctrl control_query;
  int res = device->QueryControl(v4l2_control, &control_query);
  if (res) {
    HAL_LOGE("Failed to query control %d.", v4l2_control);
    return nullptr;
  }
  if (control_query.type != V4L2_CTRL_TYPE_MENU &&
      control_query.type != V4L2_CTRL_TYPE_BOOLEAN) {
    HAL_LOGE(
        "Enum controls can only be constructed from V4L2 menu and boolean "
        "controls (%d is of type %d)",
        v4l2_control, control_query.type);
    return nullptr;
  }

  // Convert device options to metadata options.
  std::vector<uint8_t> options;
  int32_t control_max = static_cast<int32_t>(control_query.maximum);
  // Query maximum is inclusive.
  for (int32_t i = static_cast<int32_t>(control_query.minimum);
       i <= control_max; i += control_query.step) {
    auto map_entry = v4l2_to_metadata.find(i);
    if (map_entry == v4l2_to_metadata.end()) {
      HAL_LOGW("Control %d has unknown option %d.", v4l2_control, i);
    } else {
      options.push_back(map_entry->second);
    }
  }
  if (options.empty()) {
    HAL_LOGE("No supported options for control %d.", v4l2_control);
    return nullptr;
  }

  // Construct the device.
  return new V4L2EnumControl(device, v4l2_control, control_tag, options_tag,
                             std::move(v4l2_to_metadata), std::move(options));
}

V4L2EnumControl::V4L2EnumControl(
    std::shared_ptr<V4L2Wrapper> device, int v4l2_control, int32_t control_tag,
    int32_t options_tag, const std::map<int32_t, uint8_t> v4l2_to_metadata,
    std::vector<uint8_t> options)
    : OptionedControl<uint8_t>(control_tag, options_tag, std::move(options)),
      device_(device),
      v4l2_control_(v4l2_control),
      v4l2_to_metadata_(std::move(v4l2_to_metadata)) {
  HAL_LOG_ENTER();
}

int V4L2EnumControl::GetValue(uint8_t* value) const {
  HAL_LOG_ENTER();

  // Query the device for V4L2 value.
  int32_t v4l2_value = 0;
  int res = device_->GetControl(v4l2_control_, &v4l2_value);
  if (res) {
    HAL_LOGE("Failed to get value for control %d from device.", v4l2_control_);
    return res;
  }

  // Convert to metadata value.
  auto metadata_element = v4l2_to_metadata_.find(v4l2_value);
  if (metadata_element == v4l2_to_metadata_.end()) {
    HAL_LOGE("Unknown value %d for control %d.", v4l2_value, v4l2_control_);
    return -ENODEV;
  }
  *value = metadata_element->second;
  return 0;
}

int V4L2EnumControl::SetValue(const uint8_t& value) {
  HAL_LOG_ENTER();

  if (!IsSupported(value)) {
    HAL_LOGE("Invalid control value %d.", value);
    return -EINVAL;
  }

  // Convert to V4L2 value by doing an inverse lookup in the map.
  bool found = false;
  int32_t v4l2_value = -1;
  for (auto kv : v4l2_to_metadata_) {
    if (kv.second == value) {
      v4l2_value = kv.first;
      found = true;
      break;
    }
  }
  if (!found) {
    HAL_LOGE("Couldn't find V4L2 conversion of valid control value %d.", value);
    return -ENODEV;
  }

  return device_->SetControl(v4l2_control_, v4l2_value);
}

}  // namespace v4l2_camera_hal
