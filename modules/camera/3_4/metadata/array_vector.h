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

#ifndef V4L2_CAMERA_HAL_ARRAY_VECTOR_H_
#define V4L2_CAMERA_HAL_ARRAY_VECTOR_H_

#include <array>
#include <vector>

namespace v4l2_camera_hal {
// ArrayVector behaves like a std::vector of fixed length C arrays,
// with push_back accepting std::arrays to standardize length.
// Specific methods to get number of arrays/number of elements
// are provided and an ambiguous "size" is not, to avoid accidental
// incorrect use.
template <class T, size_t N>
class ArrayVector {
 public:
  const T* data() const { return mItems.data(); }
  // The number of arrays.
  size_t num_arrays() const { return mItems.size() / N; }
  // The number of elements amongst all arrays.
  size_t total_num_elements() const { return mItems.size(); }

  // Access the ith array.
  const T* operator[](int i) const { return mItems.data() + (i * N); }
  T* operator[](int i) { return mItems.data() + (i * N); }

  void push_back(const std::array<T, N>& values) {
    mItems.insert(mItems.end(), values.begin(), values.end());
  }

 private:
  std::vector<T> mItems;
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_ARRAY_VECTOR_H_
