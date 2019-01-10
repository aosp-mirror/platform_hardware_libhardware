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

#include "menu_control_options.h"

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

class MenuControlOptionsTest : public Test {
 protected:
  virtual void SetUp() {
    mock_defaults_.reset(new DefaultOptionDelegateMock<int>());
    dut_.reset(new MenuControlOptions<int>(options_, mock_defaults_));
  }

  std::unique_ptr<MenuControlOptions<int>> dut_;
  const std::vector<int> options_{1, 10, 19, 30};
  std::shared_ptr<DefaultOptionDelegateMock<int>> mock_defaults_;
};

TEST_F(MenuControlOptionsTest, MetadataRepresentation) {
  // Technically order doesn't matter, but this is faster to write,
  // and still passes.
  EXPECT_EQ(dut_->MetadataRepresentation(), options_);
}

TEST_F(MenuControlOptionsTest, IsSupported) {
  for (auto option : options_) {
    EXPECT_TRUE(dut_->IsSupported(option));
  }
  // And at least one unsupported.
  EXPECT_FALSE(dut_->IsSupported(99));
}

TEST_F(MenuControlOptionsTest, DelegateDefaultValue) {
  int template_index = 3;
  int expected = options_[2];
  ASSERT_TRUE(dut_->IsSupported(expected));
  EXPECT_CALL(*mock_defaults_, DefaultValueForTemplate(template_index, _))
      .WillOnce(DoAll(SetArgPointee<1>(expected), Return(true)));
  int actual = expected - 1;
  EXPECT_EQ(dut_->DefaultValueForTemplate(template_index, &actual), 0);
  EXPECT_EQ(actual, expected);
}

TEST_F(MenuControlOptionsTest, InvalidDelegateDefaultValue) {
  // -1 is not a supported option.
  int template_index = 3;
  int default_val = -1;
  ASSERT_FALSE(dut_->IsSupported(default_val));

  EXPECT_CALL(*mock_defaults_, DefaultValueForTemplate(template_index, _))
      .WillOnce(DoAll(SetArgPointee<1>(default_val), Return(true)));

  int actual = default_val;
  EXPECT_EQ(dut_->DefaultValueForTemplate(template_index, &actual), 0);
  // Should just give any supported option instead.
  EXPECT_TRUE(dut_->IsSupported(actual));
}

TEST_F(MenuControlOptionsTest, NoDelegateDefaultValue) {
  int template_index = 3;
  int actual = -1;
  ASSERT_FALSE(dut_->IsSupported(actual));

  // Have delegate error.
  EXPECT_CALL(*mock_defaults_, DefaultValueForTemplate(template_index, _))
      .WillOnce(Return(false));

  // Should still give *some* supported value.
  EXPECT_EQ(dut_->DefaultValueForTemplate(template_index, &actual), 0);
  EXPECT_TRUE(dut_->IsSupported(actual));
}

TEST_F(MenuControlOptionsTest, NoDefaultValue) {
  // Invalid options don't have a valid default.
  MenuControlOptions<int> bad_options({}, mock_defaults_);
  for (int i = 1; i < CAMERA3_TEMPLATE_COUNT; ++i) {
    int value = -1;
    EXPECT_EQ(bad_options.DefaultValueForTemplate(i, &value), -ENODEV);
  }
}

}  // namespace v4l2_camera_hal
