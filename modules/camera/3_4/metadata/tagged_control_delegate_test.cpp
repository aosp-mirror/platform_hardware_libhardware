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

#include "tagged_control_delegate.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "control_delegate_interface_mock.h"

using testing::Return;
using testing::SetArgPointee;
using testing::Test;
using testing::_;

namespace v4l2_camera_hal {

class TaggedControlDelegateTest : public Test {
 protected:
  virtual void SetUp() {
    mock_delegate_.reset(new ControlDelegateInterfaceMock<uint8_t>());
    // Nullify dut so an error will be thrown if a test doesn't call PrepareDUT.
    dut_.reset();
  }

  virtual void PrepareDUT() {
    // Use this method after all the EXPECT_CALLs to pass ownership of the
    // delegate
    // to the device.
    dut_.reset(
        new TaggedControlDelegate<uint8_t>(tag_, std::move(mock_delegate_)));
  }

  std::unique_ptr<TaggedControlDelegate<uint8_t>> dut_;
  std::unique_ptr<ControlDelegateInterfaceMock<uint8_t>> mock_delegate_;
  const int32_t tag_ = 123;
};

TEST_F(TaggedControlDelegateTest, GetTag) {
  PrepareDUT();
  EXPECT_EQ(dut_->tag(), tag_);
}

TEST_F(TaggedControlDelegateTest, GetSuccess) {
  uint8_t expected = 3;
  EXPECT_CALL(*mock_delegate_, GetValue(_))
      .WillOnce(DoAll(SetArgPointee<0>(expected), Return(0)));
  PrepareDUT();
  uint8_t actual = expected + 1;  // Initialize to an incorrect value.
  ASSERT_EQ(dut_->GetValue(&actual), 0);
  EXPECT_EQ(actual, expected);
}

TEST_F(TaggedControlDelegateTest, GetFailure) {
  int err = 3;
  EXPECT_CALL(*mock_delegate_, GetValue(_)).WillOnce(Return(err));
  PrepareDUT();
  uint8_t unused = 0;
  ASSERT_EQ(dut_->GetValue(&unused), err);
}

TEST_F(TaggedControlDelegateTest, SetSuccess) {
  uint8_t value = 3;
  EXPECT_CALL(*mock_delegate_, SetValue(value)).WillOnce(Return(0));
  PrepareDUT();
  ASSERT_EQ(dut_->SetValue(value), 0);
}

TEST_F(TaggedControlDelegateTest, SetFailure) {
  int err = 3;
  uint8_t value = 12;
  EXPECT_CALL(*mock_delegate_, SetValue(value)).WillOnce(Return(err));
  PrepareDUT();
  ASSERT_EQ(dut_->SetValue(value), err);
}

}  // namespace v4l2_camera_hal
