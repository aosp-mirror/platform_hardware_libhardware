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

#include "slider_control_options.h"

#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <hardware/camera3.h>
#include "default_option_delegate_mock.h"

using testing::Return;
using testing::SetArgPointee;
using testing::Test;
using testing::_;

namespace v4l2_camera_hal {

class SliderControlOptionsTest : public Test {
 protected:
  virtual void SetUp() {
    mock_defaults_.reset(new DefaultOptionDelegateMock<int>());
    dut_.reset(new SliderControlOptions<int>(min_, max_, mock_defaults_));
  }

  std::unique_ptr<SliderControlOptions<int>> dut_;
  std::shared_ptr<DefaultOptionDelegateMock<int>> mock_defaults_;
  const int min_ = 1;
  const int max_ = 10;
};

TEST_F(SliderControlOptionsTest, MetadataRepresentation) {
  // Technically order doesn't matter, but this is faster to write,
  // and still passes.
  std::vector<int> expected{min_, max_};
  EXPECT_EQ(dut_->MetadataRepresentation(), expected);
}

TEST_F(SliderControlOptionsTest, IsSupported) {
  for (int i = min_; i <= max_; ++i) {
    EXPECT_TRUE(dut_->IsSupported(i));
  }
  // Out of range unsupported.
  EXPECT_FALSE(dut_->IsSupported(min_ - 1));
  EXPECT_FALSE(dut_->IsSupported(max_ + 1));
}

TEST_F(SliderControlOptionsTest, DelegateDefaultValue) {
  int template_index = 3;
  int expected = max_ - 1;
  ASSERT_TRUE(dut_->IsSupported(expected));
  EXPECT_CALL(*mock_defaults_, DefaultValueForTemplate(template_index, _))
      .WillOnce(DoAll(SetArgPointee<1>(expected), Return(true)));
  int actual = expected - 1;
  EXPECT_EQ(dut_->DefaultValueForTemplate(template_index, &actual), 0);
  EXPECT_EQ(actual, expected);
}

TEST_F(SliderControlOptionsTest, LowDelegateDefaultValue) {
  int template_index = 3;
  // min - 1 is below the valid range.
  int default_val = min_ - 1;
  // Should get bumped up into range.
  int expected = min_;
  ASSERT_FALSE(dut_->IsSupported(default_val));
  ASSERT_TRUE(dut_->IsSupported(expected));

  EXPECT_CALL(*mock_defaults_, DefaultValueForTemplate(template_index, _))
      .WillOnce(DoAll(SetArgPointee<1>(default_val), Return(true)));
  int actual = default_val;
  EXPECT_EQ(dut_->DefaultValueForTemplate(template_index, &actual), 0);
  EXPECT_EQ(actual, expected);
}

TEST_F(SliderControlOptionsTest, HighDelegateDefaultValue) {
  int template_index = 3;
  // max + 1 is above the valid range.
  int default_val = max_ + 1;
  // Should get bumped down into range.
  int expected = max_;
  ASSERT_FALSE(dut_->IsSupported(default_val));
  ASSERT_TRUE(dut_->IsSupported(expected));

  EXPECT_CALL(*mock_defaults_, DefaultValueForTemplate(template_index, _))
      .WillOnce(DoAll(SetArgPointee<1>(default_val), Return(true)));
  int actual = default_val;
  EXPECT_EQ(dut_->DefaultValueForTemplate(template_index, &actual), 0);
  EXPECT_EQ(actual, expected);
}

TEST_F(SliderControlOptionsTest, NoDelegateDefaultValue) {
  int template_index = 3;
  int actual = min_ - 1;
  ASSERT_FALSE(dut_->IsSupported(actual));

  // Have delegate error.
  EXPECT_CALL(*mock_defaults_, DefaultValueForTemplate(template_index, _))
      .WillOnce(Return(false));

  // Should still give *some* supported value.
  EXPECT_EQ(dut_->DefaultValueForTemplate(template_index, &actual), 0);
  EXPECT_TRUE(dut_->IsSupported(actual));
}

TEST_F(SliderControlOptionsTest, NoDefaultValue) {
  // Invalid options don't have a valid default.
  SliderControlOptions<int> bad_options(10, 9, mock_defaults_);  // min > max.
  for (int i = 1; i < CAMERA3_TEMPLATE_COUNT; ++i) {
    int value = -1;
    EXPECT_EQ(bad_options.DefaultValueForTemplate(i, &value), -ENODEV);
  }
}

}  // namespace v4l2_camera_hal
