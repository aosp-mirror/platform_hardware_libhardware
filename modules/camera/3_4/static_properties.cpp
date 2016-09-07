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

#include "static_properties.h"

// #define LOG_NDEBUG 0
#define LOG_TAG "StaticProperties"
#include <cutils/log.h>
#include <system/camera.h>

#include "metadata/metadata_reader.h"

namespace default_camera_hal {

StaticProperties* StaticProperties::NewStaticProperties(
    std::unique_ptr<const MetadataReader> metadata_reader) {
  int facing = 0;
  int orientation = 0;
  // If reading any data returns an error, something is wrong.
  if (metadata_reader->Facing(&facing) ||
      metadata_reader->Orientation(&orientation)) {
    return nullptr;
  }

  return new StaticProperties(std::move(metadata_reader), facing, orientation);
}

StaticProperties::StaticProperties(
    std::unique_ptr<const MetadataReader> metadata_reader,
    int facing,
    int orientation)
    : metadata_reader_(std::move(metadata_reader)),
      facing_(facing),
      orientation_(orientation) {}

}  // namespace default_camera_hal
