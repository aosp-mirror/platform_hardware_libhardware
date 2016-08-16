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

#include "ignored_control_delegate.h"

#include <gtest/gtest.h>

using testing::Test;

namespace v4l2_camera_hal {

TEST(IgnoredControlDelegateTest, DefaultGet) {
  int32_t value = 12;
  IgnoredControlDelegate<int32_t> control(value);
  int32_t actual = 0;
  ASSERT_EQ(control.GetValue(&actual), 0);
  EXPECT_EQ(actual, value);
}

TEST(IgnoredControlDelegateTest, GetAndSet) {
  int32_t value = 12;
  IgnoredControlDelegate<int32_t> control(value);
  int32_t new_value = 13;
  ASSERT_EQ(control.SetValue(new_value), 0);
  int32_t actual = 0;
  ASSERT_EQ(control.GetValue(&actual), 0);
  // Should still be the default.
  EXPECT_EQ(actual, value);
}

}  // namespace v4l2_camera_hal
