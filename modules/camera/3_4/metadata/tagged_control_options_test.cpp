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

#include "tagged_control_options.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "control_options_interface_mock.h"

using testing::Return;
using testing::SetArgPointee;
using testing::Test;
using testing::_;

namespace v4l2_camera_hal {

class TaggedControlOptionsTest : public Test {
 protected:
  virtual void SetUp() {
    mock_options_.reset(new ControlOptionsInterfaceMock<uint8_t>());
    // Nullify dut so an error will be thrown if a test doesn't call PrepareDUT.
    dut_.reset();
  }

  virtual void PrepareDUT() {
    // Use this method after all the EXPECT_CALLs to pass ownership of the
    // options
    // to the device.
    dut_.reset(
        new TaggedControlOptions<uint8_t>(tag_, std::move(mock_options_)));
  }

  std::unique_ptr<TaggedControlOptions<uint8_t>> dut_;
  std::unique_ptr<ControlOptionsInterfaceMock<uint8_t>> mock_options_;
  const int32_t tag_ = 123;
};

TEST_F(TaggedControlOptionsTest, GetTag) {
  PrepareDUT();
  EXPECT_EQ(dut_->tag(), tag_);
}

TEST_F(TaggedControlOptionsTest, MetadataRepresentation) {
  std::vector<uint8_t> expected{3, 4, 5};
  EXPECT_CALL(*mock_options_, MetadataRepresentation())
      .WillOnce(Return(expected));
  PrepareDUT();
  ASSERT_EQ(dut_->MetadataRepresentation(), expected);
}

TEST_F(TaggedControlOptionsTest, IsSupportedTrue) {
  bool supported = true;
  uint8_t value = 3;
  EXPECT_CALL(*mock_options_, IsSupported(value)).WillOnce(Return(supported));
  PrepareDUT();
  ASSERT_EQ(dut_->IsSupported(value), supported);
}

TEST_F(TaggedControlOptionsTest, IsSupportedFalse) {
  bool supported = false;
  uint8_t value = 3;
  EXPECT_CALL(*mock_options_, IsSupported(value)).WillOnce(Return(supported));
  PrepareDUT();
  ASSERT_EQ(dut_->IsSupported(value), supported);
}

TEST_F(TaggedControlOptionsTest, DefaultValue) {
  uint8_t value = 99;
  int template_id = 3;
  EXPECT_CALL(*mock_options_, DefaultValueForTemplate(template_id, _))
      .WillOnce(DoAll(SetArgPointee<1>(value), Return(0)));
  PrepareDUT();
  uint8_t actual = value + 1;
  EXPECT_EQ(dut_->DefaultValueForTemplate(template_id, &actual), 0);
  EXPECT_EQ(actual, value);
}

TEST_F(TaggedControlOptionsTest, DefaultValueFail) {
  int err = 12;
  int template_id = 3;
  EXPECT_CALL(*mock_options_, DefaultValueForTemplate(template_id, _))
      .WillOnce(Return(err));
  PrepareDUT();
  uint8_t unused;
  EXPECT_EQ(dut_->DefaultValueForTemplate(template_id, &unused), err);
}

}  // namespace v4l2_camera_hal
