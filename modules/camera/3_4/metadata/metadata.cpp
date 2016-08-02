/*
 * Copyright 2016 The Android Open Source Project
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

#include "metadata.h"

#include <camera/CameraMetadata.h>

#include "../common.h"

namespace v4l2_camera_hal {

Metadata::Metadata() { HAL_LOG_ENTER(); }

Metadata::~Metadata() { HAL_LOG_ENTER(); }

void Metadata::AddComponent(
    std::unique_ptr<PartialMetadataInterface> component) {
  HAL_LOG_ENTER();

  components_.push_back(std::move(component));
}

int Metadata::FillStaticMetadata(camera_metadata_t** metadata) {
  HAL_LOG_ENTER();

  std::vector<int32_t> static_tags;
  std::vector<int32_t> control_tags;
  std::vector<int32_t> dynamic_tags;
  int res = 0;

  for (auto& component : components_) {
    // Populate the fields.
    res = component->PopulateStaticFields(metadata);
    if (res) {
      // Exit on error.
      HAL_LOGE("Failed to get all static properties.");
      return res;
    }

    // Note what tags the component adds.
    const std::vector<int32_t>* tags = &component->StaticTags();
    static_tags.insert(static_tags.end(), tags->begin(), tags->end());
    tags = &component->ControlTags();
    control_tags.insert(control_tags.end(), tags->begin(), tags->end());
    tags = &component->DynamicTags();
    dynamic_tags.insert(dynamic_tags.end(), tags->begin(), tags->end());
  }

  // Populate the meta fields.
  android::CameraMetadata metadata_wrapper(*metadata);
  static_tags.push_back(ANDROID_REQUEST_AVAILABLE_REQUEST_KEYS);
  res = metadata_wrapper.update(ANDROID_REQUEST_AVAILABLE_REQUEST_KEYS,
                                control_tags.data(), control_tags.size());
  if (res != android::OK) {
    HAL_LOGE("Failed to add request keys meta key.");
    return -ENODEV;
  }
  static_tags.push_back(ANDROID_REQUEST_AVAILABLE_RESULT_KEYS);
  res = metadata_wrapper.update(ANDROID_REQUEST_AVAILABLE_RESULT_KEYS,
                                dynamic_tags.data(), dynamic_tags.size());
  if (res != android::OK) {
    HAL_LOGE("Failed to add result keys meta key.");
    return -ENODEV;
  }
  static_tags.push_back(ANDROID_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS);
  res = metadata_wrapper.update(ANDROID_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS,
                                static_tags.data(), static_tags.size());
  if (res != android::OK) {
    HAL_LOGE("Failed to add characteristics keys meta key.");
    return -ENODEV;
  }

  *metadata = metadata_wrapper.release();
  return 0;
}

bool Metadata::IsValidRequest(const camera_metadata_t* metadata) {
  HAL_LOG_ENTER();

  // Null means "use previous settings", which are inherently valid.
  if (metadata == nullptr) return true;

  for (auto& component : components_) {
    // Check that all components support the values requested of them.
    bool valid_request = component->SupportsRequestValues(metadata);
    if (!valid_request) {
      // Exit early if possible.
      return false;
    }
  }

  return true;
}

int Metadata::SetRequestSettings(const camera_metadata_t* metadata) {
  HAL_LOG_ENTER();

  // Null means "use previous settings".
  if (metadata == nullptr) return 0;

  for (auto& component : components_) {
    int res = component->SetRequestValues(metadata);
    if (res) {
      // Exit early if possible.
      HAL_LOGE("Failed to set all requested settings.");
      return res;
    }
  }

  return 0;
}

int Metadata::FillResultMetadata(camera_metadata_t** metadata) {
  for (auto& component : components_) {
    int res = component->PopulateDynamicFields(metadata);
    if (res) {
      // Exit early if possible.
      HAL_LOGE("Failed to get all dynamic result fields.");
      return res;
    }
  }

  return 0;
}

}  // namespace v4l2_camera_hal
