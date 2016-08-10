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

#include "optioned_control.h"

#include <camera/CameraMetadata.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "test_common.h"

using testing::AtMost;
using testing::Return;
using testing::ReturnRef;
using testing::Test;
using testing::_;

namespace v4l2_camera_hal {

class OptionedControlTest : public Test {
 protected:
  // A subclass of OptionedControl with the pure virtual methods mocked out.
  template <typename T>
  class MockOptionedControl : public OptionedControl<T> {
   public:
    MockOptionedControl(int32_t control_tag,
                        int32_t options_tag,
                        std::vector<T> options)
        : OptionedControl<T>(control_tag, options_tag, options){};
    MOCK_CONST_METHOD1_T(GetValue, int(T* value));
    MOCK_METHOD1_T(SetValue, int(const T& value));
  };

  OptionedControlTest()
      : options_({ANDROID_COLOR_CORRECTION_ABERRATION_MODE_OFF,
                  ANDROID_COLOR_CORRECTION_ABERRATION_MODE_FAST,
                  ANDROID_COLOR_CORRECTION_ABERRATION_MODE_HIGH_QUALITY}) {}

  virtual void SetUp() {
    control_.reset(
        new MockOptionedControl<uint8_t>(control_tag_, options_tag_, options_));
  }

  std::unique_ptr<OptionedControl<uint8_t>> control_;

  // Need tags that match the data type (uint8_t) being passed.
  static constexpr int32_t options_tag_ =
      ANDROID_COLOR_CORRECTION_AVAILABLE_ABERRATION_MODES;
  static constexpr int32_t control_tag_ =
      ANDROID_COLOR_CORRECTION_ABERRATION_MODE;

  const std::vector<uint8_t> options_;
};

TEST_F(OptionedControlTest, Tags) {
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

TEST_F(OptionedControlTest, PopulateStatic) {
  // Populate static fields.
  android::CameraMetadata metadata;
  ASSERT_EQ(control_->PopulateStaticFields(&metadata), 0);

  // Check the results.
  // Should only have added 1 entry.
  EXPECT_EQ(metadata.entryCount(), 1);
  // Should have added the right entry.
  ExpectMetadataEq(metadata, options_tag_, options_);
}

TEST_F(OptionedControlTest, IsSupported) {
  for (auto option : options_) {
    EXPECT_TRUE(control_->IsSupported(option));
  }
  // And at least one unsupported.
  EXPECT_FALSE(control_->IsSupported(99));
}

}  // namespace v4l2_camera_hal
