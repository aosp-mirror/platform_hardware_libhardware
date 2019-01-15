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

#ifndef V4L2_CAMERA_HAL_METADATA_TEST_COMMON_H_
#define V4L2_CAMERA_HAL_METADATA_TEST_COMMON_H_

#include <array>
#include <vector>

#include <camera/CameraMetadata.h>
#include <gtest/gtest.h>
#include "array_vector.h"
#include "metadata_common.h"

namespace v4l2_camera_hal {

// Check that metadata of a given tag matches expectations.
// Generic.
template <typename T>
static void ExpectMetadataEq(const android::CameraMetadata& metadata,
                             int32_t tag,
                             const T* expected,
                             size_t size) {
  camera_metadata_ro_entry_t entry = metadata.find(tag);
  ASSERT_EQ(entry.count, size);
  const T* data = nullptr;
  GetDataPointer(entry, &data);
  ASSERT_NE(data, nullptr);
  for (size_t i = 0; i < size; ++i) {
    EXPECT_EQ(data[i], expected[i]);
  }
}

// Single item.
template <typename T>
static void ExpectMetadataEq(const android::CameraMetadata& metadata,
                             int32_t tag,
                             T expected) {
  ExpectMetadataEq(metadata, tag, &expected, 1);
}

// Vector of items.
template <typename T>
static void ExpectMetadataEq(const android::CameraMetadata& metadata,
                             int32_t tag,
                             const std::vector<T>& expected) {
  ExpectMetadataEq(metadata, tag, expected.data(), expected.size());
}

// Array of items.
template <typename T, size_t N>
static void ExpectMetadataEq(const android::CameraMetadata& metadata,
                             int32_t tag,
                             const std::array<T, N>& expected) {
  ExpectMetadataEq(metadata, tag, expected.data(), N);
}

// ArrayVector.
template <typename T, size_t N>
static void ExpectMetadataEq(const android::CameraMetadata& metadata,
                             int32_t tag,
                             const ArrayVector<T, N>& expected) {
  ExpectMetadataEq(
      metadata, tag, expected.data(), expected.total_num_elements());
}

// Vector of arrays.
template <typename T, size_t N>
static void ExpectMetadataEq(const android::CameraMetadata& metadata,
                             int32_t tag,
                             const std::vector<std::array<T, N>>& expected) {
  // Convert to array vector so we know all the elements are contiguous.
  ArrayVector<T, N> array_vector;
  for (const auto& array : expected) {
    array_vector.push_back(array);
  }
  ExpectMetadataEq(metadata, tag, array_vector);
}

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_TEST_COMMON_H_
