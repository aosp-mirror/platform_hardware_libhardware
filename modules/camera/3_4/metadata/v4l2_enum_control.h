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

#ifndef V4L2_CAMERA_HAL_METADATA_V4L2_ENUM_CONTROL_H_
#define V4L2_CAMERA_HAL_METADATA_V4L2_ENUM_CONTROL_H_

#include <map>
#include <memory>
#include <vector>

#include <gtest/gtest_prod.h>

#include "../v4l2_wrapper.h"
#include "optioned_control.h"

namespace v4l2_camera_hal {

// A V4L2EnumControl is a direct mapping between a V4L2 control
// and an Android metadata control.
class V4L2EnumControl : public OptionedControl<uint8_t> {
 public:
  // Use this method to create V4L2EnumControl objects.
  // Functionally equivalent to "new V4L2EnumControl"
  // except that it may return nullptr in case of failure.
  static V4L2EnumControl* NewV4L2EnumControl(
      std::shared_ptr<V4L2Wrapper> device,
      int v4l2_control,
      int32_t control_tag,
      int32_t options_tag,
      const std::map<int32_t, uint8_t>& v4l2_to_metadata);

 private:
  // Constructor private to allow failing on bad input.
  // Use NewV4L2EnumControl instead.
  // The values in |v4l2_to_metadata| must be a superset of |options|.
  V4L2EnumControl(std::shared_ptr<V4L2Wrapper> device,
                  int v4l2_control,
                  int32_t control_tag,
                  int32_t options_tag,
                  const std::map<int32_t, uint8_t> v4l2_to_metadata,
                  std::vector<uint8_t> options);

  virtual int GetValue(uint8_t* value) const override;
  virtual int SetValue(const uint8_t& value) override;

  std::shared_ptr<V4L2Wrapper> device_;
  const int v4l2_control_;
  const std::map<int32_t, uint8_t> v4l2_to_metadata_;

  FRIEND_TEST(V4L2EnumControlTest, SetValue);
  FRIEND_TEST(V4L2EnumControlTest, SetValueFail);
  FRIEND_TEST(V4L2EnumControlTest, SetInvalidValue);
  FRIEND_TEST(V4L2EnumControlTest, SetUnmapped);
  FRIEND_TEST(V4L2EnumControlTest, GetValue);
  FRIEND_TEST(V4L2EnumControlTest, GetValueFail);
  FRIEND_TEST(V4L2EnumControlTest, GetUnmapped);
  friend class V4L2EnumControlTest;  // Access to private constructor.

  DISALLOW_COPY_AND_ASSIGN(V4L2EnumControl);
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_METADATA_V4L2_ENUM_CONTROL_H_
