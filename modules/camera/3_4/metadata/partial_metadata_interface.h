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

#ifndef V4L2_CAMERA_HAL_METADATA_PARTIAL_METADATA_INTERFACE_H_
#define V4L2_CAMERA_HAL_METADATA_PARTIAL_METADATA_INTERFACE_H_

#include <array>
#include <vector>

#include <camera/CameraMetadata.h>

#include "../common.h"
#include "array_vector.h"

namespace v4l2_camera_hal {

// A subset of metadata.
class PartialMetadataInterface {
 public:
  virtual ~PartialMetadataInterface(){};

  // The metadata tags this partial metadata is responsible for.
  // See system/media/camera/docs/docs.html for descriptions of each tag.
  virtual const std::vector<int32_t>& StaticTags() const = 0;
  virtual const std::vector<int32_t>& ControlTags() const = 0;
  virtual const std::vector<int32_t>& DynamicTags() const = 0;

  // Add all the static properties this partial metadata
  // is responsible for to |metadata|.
  virtual int PopulateStaticFields(android::CameraMetadata* metadata) const = 0;
  // Add all the dynamic states this partial metadata
  // is responsible for to |metadata|.
  virtual int PopulateDynamicFields(
      android::CameraMetadata* metadata) const = 0;
  // Check if the requested control values from |metadata| (for controls
  // this partial metadata owns) are supported. Empty/null values for owned
  // control tags indicate no change, and are thus inherently supported.
  // If |metadata| is empty all controls are implicitly supported.
  virtual bool SupportsRequestValues(
      const android::CameraMetadata& metadata) const = 0;
  // Set all the controls this partial metadata
  // is responsible for from |metadata|. Empty/null values for owned control
  // tags indicate no change. If |metadata| is empty no controls should
  // be changed.
  virtual int SetRequestValues(const android::CameraMetadata& metadata) = 0;

 protected:
  // Templated helper function to update a single entry of some metadata.
  // Will cause a compile-time error if CameraMetadata doesn't support
  // updating the given type. An error will be returned if the data type
  // does not match the expected type for the tag. No errors are given
  // for updating a metadata entry with the incorrect amount of data,
  // as this information is not encoded in the type associated with the tag
  // by get_camera_metadata_tag_type (from <system/camera_metadata.h>).
  // Generic (pointer & size).
  template <typename T>
  static int UpdateMetadata(android::CameraMetadata* metadata, int32_t tag,
                            const T* data, size_t count) {
    int res = metadata->update(tag, data, count);
    if (res) {
      HAL_LOGE("Failed to update metadata tag %d", tag);
      return -ENODEV;
    }
    return 0;
  }

  // Generic (single item reference).
  template <typename T>
  static int UpdateMetadata(android::CameraMetadata* metadata, int32_t tag,
                            const T& val) {
    return UpdateMetadata(metadata, tag, &val, 1);
  }

  // Specialization for vectors.
  template <typename T>
  static int UpdateMetadata(android::CameraMetadata* metadata, int32_t tag,
                            const std::vector<T>& val) {
    return UpdateMetadata(metadata, tag, val.data(), val.size());
  }

  // Specialization for arrays.
  template <typename T, size_t N>
  static int UpdateMetadata(android::CameraMetadata* metadata, int32_t tag,
                            const std::array<T, N>& val) {
    return UpdateMetadata(metadata, tag, val.data(), N);
  }

  // Specialization for ArrayVectors.
  template <typename T, size_t N>
  static int UpdateMetadata(android::CameraMetadata* metadata, int32_t tag,
                            const ArrayVector<T, N>& val) {
    return UpdateMetadata(metadata, tag, val.data(), val.total_num_elements());
  }
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_PARTIAL_METADATA_INTERFACE_H_
