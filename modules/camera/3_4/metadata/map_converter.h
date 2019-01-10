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

#ifndef V4L2_CAMERA_HAL_METADATA_MAP_CONVERTER_H_
#define V4L2_CAMERA_HAL_METADATA_MAP_CONVERTER_H_

#include <cerrno>
#include <map>
#include <memory>

#include <android-base/macros.h>
#include "common.h"
#include "converter_interface.h"

namespace v4l2_camera_hal {

// A MapConverter fits values converted by a wrapped converter
// to a map entry corresponding to the key with the nearest value.
template <typename TMetadata, typename TV4L2, typename TMapKey>
class MapConverter : public ConverterInterface<TMetadata, TV4L2> {
 public:
  MapConverter(
      std::shared_ptr<ConverterInterface<TMetadata, TMapKey>> wrapped_converter,
      std::map<TMapKey, TV4L2> conversion_map);

  virtual int MetadataToV4L2(TMetadata value, TV4L2* conversion) override;
  virtual int V4L2ToMetadata(TV4L2 value, TMetadata* conversion) override;

 private:
  std::shared_ptr<ConverterInterface<TMetadata, TMapKey>> wrapped_converter_;
  std::map<TMapKey, TV4L2> conversion_map_;

  DISALLOW_COPY_AND_ASSIGN(MapConverter);
};

// -----------------------------------------------------------------------------

template <typename TMetadata, typename TV4L2, typename TMapKey>
MapConverter<TMetadata, TV4L2, TMapKey>::MapConverter(
    std::shared_ptr<ConverterInterface<TMetadata, TMapKey>> wrapped_converter,
    std::map<TMapKey, TV4L2> conversion_map)
    : wrapped_converter_(std::move(wrapped_converter)),
      conversion_map_(conversion_map) {
  HAL_LOG_ENTER();
}

template <typename TMetadata, typename TV4L2, typename TMapKey>
int MapConverter<TMetadata, TV4L2, TMapKey>::MetadataToV4L2(TMetadata value,
                                                            TV4L2* conversion) {
  HAL_LOG_ENTER();

  if (conversion_map_.empty()) {
    HAL_LOGE("Empty conversion map.");
    return -EINVAL;
  }

  TMapKey raw_conversion = 0;
  int res = wrapped_converter_->MetadataToV4L2(value, &raw_conversion);
  if (res) {
    HAL_LOGE("Failed to perform underlying conversion.");
    return res;
  }

  // Find nearest key.
  auto kv = conversion_map_.lower_bound(raw_conversion);
  // lower_bound finds the first >= element.
  if (kv == conversion_map_.begin()) {
    // Searching for less than the smallest key, so that will be the nearest.
    *conversion = kv->second;
  } else if (kv == conversion_map_.end()) {
    // Searching for greater than the largest key, so that will be the nearest.
    --kv;
    *conversion = kv->second;
  } else {
    // Since kv points to the first >= element, either that or the previous
    // element will be nearest.
    *conversion = kv->second;
    TMapKey diff = kv->first - raw_conversion;

    // Now compare to the previous. This element will be < raw conversion,
    // so reverse the order of the subtraction.
    --kv;
    if (raw_conversion - kv->first < diff) {
      *conversion = kv->second;
    }
  }

  return 0;
}

template <typename TMetadata, typename TV4L2, typename TMapKey>
int MapConverter<TMetadata, TV4L2, TMapKey>::V4L2ToMetadata(
    TV4L2 value, TMetadata* conversion) {
  HAL_LOG_ENTER();

  // Unfortunately no bi-directional map lookup in C++.
  // Breaking on second, not first found so that a warning
  // can be given if there are multiple values.
  size_t count = 0;
  int res;
  for (auto kv : conversion_map_) {
    if (kv.second == value) {
      ++count;
      if (count == 1) {
        // First match.
        res = wrapped_converter_->V4L2ToMetadata(kv.first, conversion);
      } else {
        // second match.
        break;
      }
    }
  }

  if (count == 0) {
    HAL_LOGE("Couldn't find map conversion of V4L2 value %d.", value);
    return -EINVAL;
  } else if (count > 1) {
    HAL_LOGW("Multiple map conversions found for V4L2 value %d, using first.",
             value);
  }
  return res;
}

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_MAP_CONVERTER_H_
