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

#include "state.h"

#include <camera/CameraMetadata.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "metadata_common.h"
#include "state_delegate_interface_mock.h"
#include "test_common.h"

using testing::AtMost;
using testing::Expectation;
using testing::Return;
using testing::SetArgPointee;
using testing::Test;
using testing::_;

namespace v4l2_camera_hal {

class StateTest : public Test {
 protected:
  virtual void SetUp() {
    mock_delegate_.reset(new StateDelegateInterfaceMock<uint8_t>());
    // Nullify state so an error will be thrown if a test doesn't call
    // PrepareState.
    state_.reset();
  }

  virtual void PrepareState() {
    // Use this method after all the EXPECT_CALLs to pass ownership of the mocks
    // to the device.
    state_.reset(new State<uint8_t>(tag_, std::move(mock_delegate_)));
  }

  std::unique_ptr<State<uint8_t>> state_;
  std::unique_ptr<StateDelegateInterfaceMock<uint8_t>> mock_delegate_;

  // Need tag that matches the data type (uint8_t) being passed.
  const int32_t tag_ = ANDROID_CONTROL_AF_STATE;
};

TEST_F(StateTest, Tags) {
  PrepareState();
  EXPECT_TRUE(state_->StaticTags().empty());
  EXPECT_TRUE(state_->ControlTags().empty());
  ASSERT_EQ(state_->DynamicTags().size(), 1u);
  EXPECT_EQ(state_->DynamicTags()[0], tag_);
}

TEST_F(StateTest, PopulateStatic) {
  PrepareState();
  android::CameraMetadata metadata;
  ASSERT_EQ(state_->PopulateStaticFields(&metadata), 0);
  EXPECT_TRUE(metadata.isEmpty());
}

TEST_F(StateTest, PopulateDynamic) {
  uint8_t expected = 99;
  EXPECT_CALL(*mock_delegate_, GetValue(_))
      .WillOnce(DoAll(SetArgPointee<0>(expected), Return(0)));

  PrepareState();

  android::CameraMetadata metadata;
  ASSERT_EQ(state_->PopulateDynamicFields(&metadata), 0);
  EXPECT_EQ(metadata.entryCount(), 1u);
  ExpectMetadataEq(metadata, tag_, expected);
}

TEST_F(StateTest, PopulateDynamicFail) {
  int err = 123;
  EXPECT_CALL(*mock_delegate_, GetValue(_)).WillOnce(Return(err));

  PrepareState();

  android::CameraMetadata metadata;
  ASSERT_EQ(state_->PopulateDynamicFields(&metadata), err);
}

TEST_F(StateTest, PopulateTemplate) {
  int template_type = 3;
  PrepareState();
  android::CameraMetadata metadata;
  ASSERT_EQ(state_->PopulateTemplateRequest(template_type, &metadata), 0);
  EXPECT_TRUE(metadata.isEmpty());
}

TEST_F(StateTest, SupportsRequest) {
  PrepareState();
  android::CameraMetadata metadata;
  EXPECT_TRUE(state_->SupportsRequestValues(metadata));
}

TEST_F(StateTest, SetRequest) {
  PrepareState();
  android::CameraMetadata metadata;
  ASSERT_EQ(state_->SetRequestValues(metadata), 0);
}

}  // namespace v4l2_camera_hal
