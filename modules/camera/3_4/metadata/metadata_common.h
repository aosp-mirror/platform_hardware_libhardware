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

#ifndef V4L2_CAMERA_HAL_METADATA_METADATA_COMMON_H_
#define V4L2_CAMERA_HAL_METADATA_METADATA_COMMON_H_

#include <array>
#include <memory>
#include <set>
#include <vector>

#include <camera/CameraMetadata.h>
#include "array_vector.h"
#include "common.h"
#include "partial_metadata_interface.h"

namespace v4l2_camera_hal {

typedef std::set<std::unique_ptr<PartialMetadataInterface>> PartialMetadataSet;

// Templated helper functions effectively extending android::CameraMetadata.
// Will cause a compile-time errors if CameraMetadata doesn't support
// using the templated type. Templates are provided to extend this support
// to std::arrays, std::vectors, and ArrayVectors of supported types as
// appropriate.

// UpdateMetadata(metadata, tag, data):
//
// Updates the entry for |tag| in |metadata| (functionally similar to
// android::CameraMetadata::update).
//
// Args:
//   metadata: the android::CameraMetadata to update.
//   tag: the tag within |metadata| to update.
//   data: A reference to the data to update |tag| with.
//
// Returns:
//   0: Success.
//   -ENODEV: The type of |data| does not match the expected type for |tag|,
//     or another error occured. Note: no errors are given for updating a
//     metadata entry with an incorrect amount of data (e.g. filling a tag
//     that expects to have only one value with multiple values), as this
//     information is not encoded in the type associated with the tag by
//     get_camera_metadata_tag_type (from <system/camera_metadata.h>).

// Generic (pointer & size).
template <typename T>
static int UpdateMetadata(android::CameraMetadata* metadata,
                          int32_t tag,
                          const T* data,
                          size_t count) {
  int res = metadata->update(tag, data, count);
  if (res) {
    HAL_LOGE("Failed to update metadata tag %d", tag);
    return -ENODEV;
  }
  return 0;
}

// Generic (single item reference).
template <typename T>
static int UpdateMetadata(android::CameraMetadata* metadata,
                          int32_t tag,
                          const T& val) {
  return UpdateMetadata(metadata, tag, &val, 1);
}

// Specialization for vectors.
template <typename T>
static int UpdateMetadata(android::CameraMetadata* metadata,
                          int32_t tag,
                          const std::vector<T>& val) {
  return UpdateMetadata(metadata, tag, val.data(), val.size());
}

// Specialization for arrays.
template <typename T, size_t N>
static int UpdateMetadata(android::CameraMetadata* metadata,
                          int32_t tag,
                          const std::array<T, N>& val) {
  return UpdateMetadata(metadata, tag, val.data(), N);
}

// Specialization for ArrayVectors.
template <typename T, size_t N>
static int UpdateMetadata(android::CameraMetadata* metadata,
                          int32_t tag,
                          const ArrayVector<T, N>& val) {
  return UpdateMetadata(metadata, tag, val.data(), val.total_num_elements());
}

// Specialization for vectors of arrays.
template <typename T, size_t N>
static int UpdateMetadata(android::CameraMetadata* metadata,
                          int32_t tag,
                          const std::vector<std::array<T, N>>& val) {
  // Convert to array vector so we know all the elements are contiguous.
  ArrayVector<T, N> array_vector;
  for (const auto& array : val) {
    array_vector.push_back(array);
  }
  return UpdateMetadata(metadata, tag, array_vector);
}

// GetDataPointer(entry, val)
//
// A helper for other methods in this file.
// Gets the data pointer of a given metadata entry into |*val|.

template <typename T>
inline void GetDataPointer(camera_metadata_ro_entry_t&, const T**);

template <>
inline void GetDataPointer<uint8_t>(camera_metadata_ro_entry_t& entry,
                           const uint8_t** val) {
  *val = entry.data.u8;
}

template <>
inline void GetDataPointer<int32_t>(camera_metadata_ro_entry_t& entry,
                           const int32_t** val) {
  *val = entry.data.i32;
}

template <>
inline void GetDataPointer<float>(camera_metadata_ro_entry_t& entry,
                           const float** val) {
  *val = entry.data.f;
}

template <>
inline void GetDataPointer<int64_t>(camera_metadata_ro_entry_t& entry,
                           const int64_t** val) {
  *val = entry.data.i64;
}

template <>
inline void GetDataPointer<double>(camera_metadata_ro_entry_t& entry,
                           const double** val) {
  *val = entry.data.d;
}

template <>
inline void GetDataPointer<camera_metadata_rational_t>(camera_metadata_ro_entry_t& entry,
                           const camera_metadata_rational_t** val) {
  *val = entry.data.r;
}

// SingleTagValue(metadata, tag, val)
//
// Get the value of the |tag| entry in |metadata|.
// |tag| is expected to refer to an entry with a single item
// of the templated type (a "single item" is exactly N values
// if the templated type is an array of size N). An error will be
// returned if it the wrong number of items are present.
//
// Returns:
//   -ENOENT: The tag couldn't be found or was empty.
//   -EINVAL: The tag contained more than one item, or |val| is null.
//   -ENODEV: The tag claims to be non-empty, but the data pointer is null.
//   0: Success. |*val| will contain the value for |tag|.

// Singleton.
template <typename T>
static int SingleTagValue(const android::CameraMetadata& metadata,
                          int32_t tag,
                          T* val) {
  if (!val) {
    HAL_LOGE("Null pointer passed to SingleTagValue.");
    return -EINVAL;
  }
  camera_metadata_ro_entry_t entry = metadata.find(tag);
  if (entry.count == 0) {
    HAL_LOGE("Metadata tag %d is empty.", tag);
    return -ENOENT;
  } else if (entry.count != 1) {
    HAL_LOGE(
        "Error: expected metadata tag %d to contain exactly 1 value "
        "(had %zu).",
        tag,
        entry.count);
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

// Specialization for std::array.
template <typename T, size_t N>
static int SingleTagValue(const android::CameraMetadata& metadata,
                          int32_t tag,
                          std::array<T, N>* val) {
  if (!val) {
    HAL_LOGE("Null pointer passed to SingleTagValue.");
    return -EINVAL;
  }
  camera_metadata_ro_entry_t entry = metadata.find(tag);
  if (entry.count == 0) {
    HAL_LOGE("Metadata tag %d is empty.", tag);
    return -ENOENT;
  } else if (entry.count != N) {
    HAL_LOGE(
        "Error: expected metadata tag %d to contain a single array of "
        "exactly %zu values (had %zu).",
        tag,
        N,
        entry.count);
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

// VectorTagValue(metadata, tag, val)
//
// Get the value of the |tag| entry in |metadata|.
// |tag| is expected to refer to an entry with a vector
// of the templated type. For arrays, an error will be
// returned if it the wrong number of items are present.
//
// Returns:
//   -ENOENT: The tag couldn't be found or was empty. While technically an
//            empty vector may be valid, this error is returned for consistency
//            with SingleTagValue.
//   -EINVAL: The tag contained an invalid number of entries (e.g. 6 entries for
//            a vector of length 4 arrays), or |val| is null.
//   -ENODEV: The tag claims to be non-empty, but the data pointer is null.
//   0: Success. |*val| will contain the values for |tag|.
template <typename T>
static int VectorTagValue(const android::CameraMetadata& metadata,
                          int32_t tag,
                          std::vector<T>* val) {
  if (!val) {
    HAL_LOGE("Null pointer passed to VectorTagValue.");
    return -EINVAL;
  }
  camera_metadata_ro_entry_t entry = metadata.find(tag);
  if (entry.count == 0) {
    return -ENOENT;
  }
  const T* data = nullptr;
  GetDataPointer(entry, &data);
  if (data == nullptr) {
    HAL_LOGE("Metadata tag %d claims to have elements but is empty.", tag);
    return -ENODEV;
  }
  // Copy the data for |tag| into the output vector.
  *val = std::vector<T>(data, data + entry.count);
  return 0;
}

// Specialization for std::array.
template <typename T, size_t N>
static int VectorTagValue(const android::CameraMetadata& metadata,
                          int32_t tag,
                          std::vector<std::array<T, N>>* val) {
  if (!val) {
    HAL_LOGE("Null pointer passed to VectorTagValue.");
    return -EINVAL;
  }
  camera_metadata_ro_entry_t entry = metadata.find(tag);
  if (entry.count == 0) {
    return -ENOENT;
  }
  if (entry.count % N != 0) {
    HAL_LOGE(
        "Error: expected metadata tag %d to contain a vector of arrays of "
        "length %zu (had %zu entries, which is not divisible by %zu).",
        tag,
        N,
        entry.count,
        N);
    return -EINVAL;
  }
  const T* data = nullptr;
  GetDataPointer(entry, &data);
  if (data == nullptr) {
    HAL_LOGE("Metadata tag %d claims to have elements but is empty.", tag);
    return -ENODEV;
  }
  // Copy the data for |tag| into separate arrays for the output vector.
  size_t num_arrays = entry.count / N;
  *val = std::vector<std::array<T, N>>(num_arrays);
  for (size_t i = 0; i < num_arrays; ++i) {
    for (size_t j = 0; j < N; ++j) {
      val->at(i)[j] = data[i * N + j];
    }
  }
  return 0;
}

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_METADATA_COMMON_H_
