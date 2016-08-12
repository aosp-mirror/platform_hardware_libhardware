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

#include "control.h"

#include <camera/CameraMetadata.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "control_delegate_interface_mock.h"
#include "control_options_interface_mock.h"
#include "metadata_common.h"
#include "test_common.h"

using testing::AtMost;
using testing::Expectation;
using testing::Return;
using testing::ReturnRef;
using testing::SetArgPointee;
using testing::Test;
using testing::_;

namespace v4l2_camera_hal {

class ControlTest : public Test {
 protected:
  virtual void SetUp() {
    mock_delegate_.reset(new ControlDelegateInterfaceMock<uint8_t>());
    mock_options_.reset(new ControlOptionsInterfaceMock<uint8_t>());
    // Nullify control so an error will be thrown if a test doesn't call
    // PrepareControl.
    control_.reset();
  }

  virtual void PrepareControl(bool with_options = true) {
    // Use this method after all the EXPECT_CALLs to pass ownership of the mocks
    // to the device.
    std::unique_ptr<TaggedControlDelegate<uint8_t>> delegate =
        std::make_unique<TaggedControlDelegate<uint8_t>>(
            delegate_tag_, std::move(mock_delegate_));
    std::unique_ptr<TaggedControlOptions<uint8_t>> options =
        std::make_unique<TaggedControlOptions<uint8_t>>(
            options_tag_, std::move(mock_options_));
    if (with_options) {
      control_.reset(
          new Control<uint8_t>(std::move(delegate), std::move(options)));
    } else {
      control_.reset(new Control<uint8_t>(std::move(delegate)));
    }
  }

  std::unique_ptr<Control<uint8_t>> control_;
  std::unique_ptr<ControlDelegateInterfaceMock<uint8_t>> mock_delegate_;
  std::unique_ptr<ControlOptionsInterfaceMock<uint8_t>> mock_options_;

  // Need tags that match the data type (uint8_t) being passed.
  const int32_t delegate_tag_ = ANDROID_COLOR_CORRECTION_ABERRATION_MODE;
  const int32_t options_tag_ =
      ANDROID_COLOR_CORRECTION_AVAILABLE_ABERRATION_MODES;
};

TEST_F(ControlTest, Tags) {
  PrepareControl();
  ASSERT_EQ(control_->StaticTags().size(), 1);
  EXPECT_EQ(control_->StaticTags()[0], options_tag_);
  // Controls use the same delgate, and thus tag, for getting and setting.
  ASSERT_EQ(control_->ControlTags().size(), 1);
  EXPECT_EQ(control_->ControlTags()[0], delegate_tag_);
  ASSERT_EQ(control_->DynamicTags().size(), 1);
  EXPECT_EQ(control_->DynamicTags()[0], delegate_tag_);
}

TEST_F(ControlTest, TagsNoOptions) {
  PrepareControl(false);
  // No options, so no options tag.
  ASSERT_EQ(control_->StaticTags().size(), 0);
  // Controls use the same delgate, and thus tag, for getting and setting.
  ASSERT_EQ(control_->ControlTags().size(), 1);
  EXPECT_EQ(control_->ControlTags()[0], delegate_tag_);
  ASSERT_EQ(control_->DynamicTags().size(), 1);
  EXPECT_EQ(control_->DynamicTags()[0], delegate_tag_);
}

TEST_F(ControlTest, PopulateStatic) {
  std::vector<uint8_t> expected{1, 10, 20};
  EXPECT_CALL(*mock_options_, MetadataRepresentation())
      .WillOnce(Return(expected));
  PrepareControl();

  android::CameraMetadata metadata;
  ASSERT_EQ(control_->PopulateStaticFields(&metadata), 0);
  // Should only have added 1 entry.
  EXPECT_EQ(metadata.entryCount(), 1);
  // Should have added the right entry.
  ExpectMetadataEq(metadata, options_tag_, expected);
}

TEST_F(ControlTest, PopulateStaticNoOptions) {
  PrepareControl(false);
  android::CameraMetadata metadata;
  ASSERT_EQ(control_->PopulateStaticFields(&metadata), 0);
  // Should not have added any entry.
  EXPECT_TRUE(metadata.isEmpty());
}

TEST_F(ControlTest, PopulateDynamic) {
  uint8_t test_option = 99;
  EXPECT_CALL(*mock_delegate_, GetValue(_))
      .WillOnce(DoAll(SetArgPointee<0>(test_option), Return(0)));
  PrepareControl();

  android::CameraMetadata metadata;
  ASSERT_EQ(control_->PopulateDynamicFields(&metadata), 0);

  // Should only have added 1 entry.
  EXPECT_EQ(metadata.entryCount(), 1);
  // Should have added the right entry.
  ExpectMetadataEq(metadata, delegate_tag_, test_option);
}

TEST_F(ControlTest, PopulateDynamicNoOptions) {
  // Lack of options shouldn't change anything for PopulateDynamic.
  uint8_t test_option = 99;
  EXPECT_CALL(*mock_delegate_, GetValue(_))
      .WillOnce(DoAll(SetArgPointee<0>(test_option), Return(0)));
  PrepareControl(false);

  android::CameraMetadata metadata;
  EXPECT_EQ(control_->PopulateDynamicFields(&metadata), 0);

  // Should only have added 1 entry.
  EXPECT_EQ(metadata.entryCount(), 1);
  // Should have added the right entry.
  ExpectMetadataEq(metadata, delegate_tag_, test_option);
}

TEST_F(ControlTest, PopulateDynamicFail) {
  int err = -99;
  EXPECT_CALL(*mock_delegate_, GetValue(_)).WillOnce(Return(err));
  PrepareControl();

  android::CameraMetadata metadata;
  EXPECT_EQ(control_->PopulateDynamicFields(&metadata), err);

  // Should not have added an entry.
  EXPECT_TRUE(metadata.isEmpty());
}

TEST_F(ControlTest, SupportsRequest) {
  android::CameraMetadata metadata;
  uint8_t test_option = 123;
  ASSERT_EQ(UpdateMetadata(&metadata, delegate_tag_, test_option), 0);

  EXPECT_CALL(*mock_options_, IsSupported(test_option)).WillOnce(Return(true));
  PrepareControl();

  EXPECT_EQ(control_->SupportsRequestValues(metadata), true);
}

TEST_F(ControlTest, SupportsRequestNoOptions) {
  android::CameraMetadata metadata;
  uint8_t test_option = 123;
  ASSERT_EQ(UpdateMetadata(&metadata, delegate_tag_, test_option), 0);
  PrepareControl(false);

  EXPECT_EQ(control_->SupportsRequestValues(metadata), true);
}

TEST_F(ControlTest, SupportsRequestFail) {
  android::CameraMetadata metadata;
  uint8_t test_option = 123;
  ASSERT_EQ(UpdateMetadata(&metadata, delegate_tag_, test_option), 0);

  EXPECT_CALL(*mock_options_, IsSupported(test_option)).WillOnce(Return(false));
  PrepareControl();

  EXPECT_EQ(control_->SupportsRequestValues(metadata), false);
}

TEST_F(ControlTest, SupportsRequestInvalidNumber) {
  // Start with a request for multiple values.
  android::CameraMetadata metadata;
  std::vector<uint8_t> test_data = {1, 2, 3};
  ASSERT_EQ(UpdateMetadata(&metadata, delegate_tag_, test_data), 0);
  PrepareControl();
  EXPECT_EQ(control_->SupportsRequestValues(metadata), false);
}

TEST_F(ControlTest, SupportsRequestInvalidNumberNoOptions) {
  // Start with a request for multiple values.
  android::CameraMetadata metadata;
  std::vector<uint8_t> test_data = {1, 2, 3};
  ASSERT_EQ(UpdateMetadata(&metadata, delegate_tag_, test_data), 0);
  PrepareControl(false);
  // Not having any explicit options does not exempt a control
  // from requiring the right number of values.
  EXPECT_EQ(control_->SupportsRequestValues(metadata), false);
}

TEST_F(ControlTest, SupportsRequestEmpty) {
  android::CameraMetadata metadata;
  PrepareControl();
  EXPECT_EQ(control_->SupportsRequestValues(metadata), true);
}

TEST_F(ControlTest, SetRequest) {
  android::CameraMetadata metadata;
  uint8_t test_option = 123;
  ASSERT_EQ(UpdateMetadata(&metadata, delegate_tag_, test_option), 0);

  Expectation validation_check =
      EXPECT_CALL(*mock_options_, IsSupported(test_option))
          .WillOnce(Return(true));
  EXPECT_CALL(*mock_delegate_, SetValue(test_option))
      .After(validation_check)
      .WillOnce(Return(0));
  PrepareControl();

  // Make the request.
  ASSERT_EQ(control_->SetRequestValues(metadata), 0);
}

TEST_F(ControlTest, SetRequestNoOptions) {
  android::CameraMetadata metadata;
  uint8_t test_option = 123;
  ASSERT_EQ(UpdateMetadata(&metadata, delegate_tag_, test_option), 0);

  // No options, no validation check.
  EXPECT_CALL(*mock_delegate_, SetValue(test_option)).WillOnce(Return(0));
  PrepareControl(false);

  // Make the request.
  ASSERT_EQ(control_->SetRequestValues(metadata), 0);
}

TEST_F(ControlTest, SetRequestSettingFail) {
  android::CameraMetadata metadata;
  uint8_t test_option = 123;
  ASSERT_EQ(UpdateMetadata(&metadata, delegate_tag_, test_option), 0);

  int err = 99;
  Expectation validation_check =
      EXPECT_CALL(*mock_options_, IsSupported(test_option))
          .WillOnce(Return(true));
  EXPECT_CALL(*mock_delegate_, SetValue(test_option))
      .After(validation_check)
      .WillOnce(Return(err));
  PrepareControl();

  EXPECT_EQ(control_->SetRequestValues(metadata), err);
}

TEST_F(ControlTest, SetRequestValidationFail) {
  android::CameraMetadata metadata;
  uint8_t test_option = 123;
  ASSERT_EQ(UpdateMetadata(&metadata, delegate_tag_, test_option), 0);

  EXPECT_CALL(*mock_options_, IsSupported(test_option)).WillOnce(Return(false));
  PrepareControl();

  EXPECT_EQ(control_->SetRequestValues(metadata), -EINVAL);
}

TEST_F(ControlTest, SetRequestInvalidNumber) {
  // Start with a request for multiple values.
  android::CameraMetadata metadata;
  std::vector<uint8_t> test_data = {1, 2, 3};
  ASSERT_EQ(UpdateMetadata(&metadata, delegate_tag_, test_data), 0);

  PrepareControl();
  EXPECT_EQ(control_->SetRequestValues(metadata), -EINVAL);
}

TEST_F(ControlTest, SetRequestInvalidNumberNoOptions) {
  // Start with a request for multiple values.
  android::CameraMetadata metadata;
  std::vector<uint8_t> test_data = {1, 2, 3};
  ASSERT_EQ(UpdateMetadata(&metadata, delegate_tag_, test_data), 0);

  PrepareControl(false);
  // Not having explicit options does not change that an incorrect
  // number of values is invalid.
  EXPECT_EQ(control_->SetRequestValues(metadata), -EINVAL);
}

TEST_F(ControlTest, SetRequestEmpty) {
  // Should do nothing.
  android::CameraMetadata metadata;
  PrepareControl();
  EXPECT_EQ(control_->SetRequestValues(metadata), 0);
}

TEST_F(ControlTest, NoEffectMenuFactory) {
  std::vector<uint8_t> test_options = {9, 8, 12};
  std::unique_ptr<Control<uint8_t>> dut = Control<uint8_t>::NoEffectMenuControl(
      delegate_tag_, options_tag_, test_options);
  ASSERT_NE(dut, nullptr);

  ASSERT_EQ(dut->StaticTags().size(), 1);
  EXPECT_EQ(dut->StaticTags()[0], options_tag_);
  // Controls use the same delgate, and thus tag, for getting and setting.
  ASSERT_EQ(dut->ControlTags().size(), 1);
  EXPECT_EQ(dut->ControlTags()[0], delegate_tag_);
  ASSERT_EQ(dut->DynamicTags().size(), 1);
  EXPECT_EQ(dut->DynamicTags()[0], delegate_tag_);

  // Options should be available.
  android::CameraMetadata metadata;
  ASSERT_EQ(dut->PopulateStaticFields(&metadata), 0);
  EXPECT_EQ(metadata.entryCount(), 1);
  ExpectMetadataEq(metadata, options_tag_, test_options);

  // Default value should be test_options[0].
  metadata.clear();
  ASSERT_EQ(dut->PopulateDynamicFields(&metadata), 0);
  EXPECT_EQ(metadata.entryCount(), 1);
  ExpectMetadataEq(metadata, delegate_tag_, test_options[0]);
}

}  // namespace v4l2_camera_hal
