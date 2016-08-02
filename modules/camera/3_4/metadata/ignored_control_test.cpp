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

#include "ignored_control.h"

#include <array>

#include <camera/CameraMetadata.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "array_vector.h"

using testing::AtMost;
using testing::Return;
using testing::ReturnRef;
using testing::Test;
using testing::_;

namespace v4l2_camera_hal {

class IgnoredControlTest : public Test {
 protected:
  IgnoredControlTest()
      : options_({ANDROID_COLOR_CORRECTION_ABERRATION_MODE_OFF,
                  ANDROID_COLOR_CORRECTION_ABERRATION_MODE_FAST,
                  ANDROID_COLOR_CORRECTION_ABERRATION_MODE_HIGH_QUALITY}),
        default_option_(options_[2]) {}

  virtual void SetUp() {
    control_.reset(new IgnoredControl<uint8_t>(control_tag_, options_tag_,
                                               options_, default_option_));
  }

  std::unique_ptr<IgnoredControl<uint8_t>> control_;

  // Need tags that match the data type (uint8_t) being passed.
  static constexpr int32_t options_tag_ =
      ANDROID_COLOR_CORRECTION_AVAILABLE_ABERRATION_MODES;
  static constexpr int32_t control_tag_ =
      ANDROID_COLOR_CORRECTION_ABERRATION_MODE;

  const std::vector<uint8_t> options_;
  const uint8_t default_option_;
};

TEST_F(IgnoredControlTest, Tags) {
  // The macro doesn't like the static variables
  // being passed directly to assertions.
  int32_t expected_tag = options_tag_;
  ASSERT_EQ(control_->StaticTags().size(), 1);
  EXPECT_EQ(control_->StaticTags()[0], expected_tag);

  // Controls use the same tag for getting and setting.
  expected_tag = control_tag_;
  ASSERT_EQ(control_->ControlTags().size(), 1);
  EXPECT_EQ(control_->ControlTags()[0], expected_tag);
  ASSERT_EQ(control_->DynamicTags().size(), 1);
  EXPECT_EQ(control_->DynamicTags()[0], expected_tag);
}

TEST_F(IgnoredControlTest, GetDefaultValue) {
  uint8_t value;
  EXPECT_EQ(control_->GetValue(&value), 0);
  EXPECT_EQ(value, default_option_);
}

TEST_F(IgnoredControlTest, SetAndGetValue) {
  uint8_t new_value = options_[0];
  // Make sure this isn't just testing overwriting the default with itself.
  ASSERT_NE(new_value, default_option_);

  EXPECT_EQ(control_->SetValue(new_value), 0);
  uint8_t result = new_value + 1;  // Initialize to something incorrect.
  EXPECT_EQ(control_->GetValue(&result), 0);
  EXPECT_EQ(result, new_value);
}

TEST_F(IgnoredControlTest, SetAndGetBadValue) {
  uint8_t unsupported_value = 99;
  // Make sure this is actually unsupported
  ASSERT_FALSE(control_->IsSupported(unsupported_value));

  EXPECT_EQ(control_->SetValue(unsupported_value), -EINVAL);
  // Value shouldn't have changed.
  uint8_t result = default_option_ + 1;  // Initialize to something incorrect.
  EXPECT_EQ(control_->GetValue(&result), 0);
  EXPECT_EQ(result, default_option_);
}

}  // namespace v4l2_camera_hal
