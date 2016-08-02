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

  // Specialization for vectors of arrays.
  template <typename T, size_t N>
  static int UpdateMetadata(android::CameraMetadata* metadata, int32_t tag,
                            const std::vector<std::array<T, N>>& val) {
    // Convert to array vector so we know all the elements are contiguous.
    ArrayVector<T, N> array_vector;
    for (const auto& array : val) {
      array_vector.push_back(array);
    }
    return UpdateMetadata(metadata, tag, array_vector);
  }

  // Get the data pointer of a given metadata entry. Enforces that |val| must
  // be a type supported by camera_metadata.

  static void GetDataPointer(camera_metadata_ro_entry_t& entry,
                             const uint8_t** val) {
    *val = entry.data.u8;
  }

  static void GetDataPointer(camera_metadata_ro_entry_t& entry,
                             const int32_t** val) {
    *val = entry.data.i32;
  }

  static void GetDataPointer(camera_metadata_ro_entry_t& entry,
                             const float** val) {
    *val = entry.data.f;
  }

  static void GetDataPointer(camera_metadata_ro_entry_t& entry,
                             const int64_t** val) {
    *val = entry.data.i64;
  }

  static void GetDataPointer(camera_metadata_ro_entry_t& entry,
                             const double** val) {
    *val = entry.data.d;
  }

  static void GetDataPointer(camera_metadata_ro_entry_t& entry,
                             const camera_metadata_rational_t** val) {
    *val = entry.data.r;
  }

  // Get a tag value that is expected to be a single item.
  // Returns:
  //   -ENOENT: The tag couldn't be found or was empty.
  //   -EINVAL: The tag contained more than one item.
  //   -ENODEV: The tag claims to be non-empty, but the data pointer is null.
  //   0: Success. |*val| will contain the value for |tag|.

  // Generic (one of the types supported by TagValue above).
  template <typename T>
  static int SingleTagValue(const android::CameraMetadata& metadata,
                            int32_t tag, T* val) {
    camera_metadata_ro_entry_t entry = metadata.find(tag);
    if (entry.count == 0) {
      HAL_LOGE("Metadata tag %d is empty.", tag);
      return -ENOENT;
    } else if (entry.count != 1) {
      HAL_LOGE(
          "Error: expected metadata tag %d to contain exactly 1 value "
          "(had %d).",
          tag, entry.count);
      return -EINVAL;
    }
    const T* data = nullptr;
    GetDataPointer(entry, &data);
    if (data == nullptr) {
      HAL_LOGE("Metadata tag %d is empty.", tag);
      return -ENODEV;
    }
    *val = *data;
    return 0;
  }

  // Specialization for std::array (of the types supported by TagValue above).
  template <typename T, size_t N>
  static int SingleTagValue(const android::CameraMetadata& metadata,
                            int32_t tag, std::array<T, N>* val) {
    camera_metadata_ro_entry_t entry = metadata.find(tag);
    if (entry.count == 0) {
      HAL_LOGE("Metadata tag %d is empty.", tag);
      return -ENOENT;
    } else if (entry.count != N) {
      HAL_LOGE(
          "Error: expected metadata tag %d to contain a single array of "
          "exactly %d values (had %d).",
          tag, N, entry.count);
      return -EINVAL;
    }
    const T* data = nullptr;
    GetDataPointer(entry, &data);
    if (data == nullptr) {
      HAL_LOGE("Metadata tag %d is empty.", tag);
      return -ENODEV;
    }
    // Fill in the array.
    for (size_t i = 0; i < N; ++i) {
      (*val)[i] = data[i];
    }
    return 0;
  }
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_PARTIAL_METADATA_INTERFACE_H_
