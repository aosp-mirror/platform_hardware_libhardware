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

#include <array>

#include <camera/CameraMetadata.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using testing::AtMost;
using testing::Return;
using testing::ReturnRef;
using testing::SetArgPointee;
using testing::Test;
using testing::_;

namespace v4l2_camera_hal {

class ControlTest : public Test {
 protected:
  // A subclass of Control with the pure virtual methods mocked out.
  template <typename T>
  class MockControl : public Control<T> {
   public:
    MockControl(int32_t control_tag) : Control<T>(control_tag){};
    MOCK_CONST_METHOD1_T(PopulateStaticFields,
                         int(android::CameraMetadata* metadata));
    MOCK_CONST_METHOD1_T(GetValue, int(T* value));
    MOCK_METHOD1_T(SetValue, int(const T& value));
    MOCK_CONST_METHOD1_T(IsSupported, bool(const T& value));
  };

  virtual void SetUp() {
    control_.reset(new MockControl<uint8_t>(control_tag_));
  }
  // Check that metadata of a given tag matches expectations.
  virtual void ExpectMetadataEq(const android::CameraMetadata& metadata,
                                int32_t tag, const uint8_t* expected,
                                size_t size) {
    camera_metadata_ro_entry_t entry = metadata.find(tag);
    ASSERT_EQ(entry.count, size);
    for (size_t i = 0; i < size; ++i) {
      EXPECT_EQ(entry.data.u8[i], expected[i]);
    }
  }
  virtual void ExpectMetadataEq(const android::CameraMetadata& metadata,
                                int32_t tag, const int32_t* expected,
                                size_t size) {
    camera_metadata_ro_entry_t entry = metadata.find(tag);
    ASSERT_EQ(entry.count, size);
    for (size_t i = 0; i < size; ++i) {
      EXPECT_EQ(entry.data.i32[i], expected[i]);
    }
  }
  // Single item.
  template <typename T>
  void ExpectMetadataEq(const android::CameraMetadata& metadata, int32_t tag,
                        T expected) {
    ExpectMetadataEq(metadata, tag, &expected, 1);
  }
  // Vector of items.
  template <typename T>
  void ExpectMetadataEq(const android::CameraMetadata& metadata, int32_t tag,
                        const std::vector<T>& expected) {
    ExpectMetadataEq(metadata, tag, expected.data(), expected.size());
  }

  std::unique_ptr<MockControl<uint8_t>> control_;

  // Need tags that match the data type (uint8_t) being passed.
  static constexpr int32_t control_tag_ =
      ANDROID_COLOR_CORRECTION_ABERRATION_MODE;
};

TEST_F(ControlTest, Tags) {
  // Should have no static tags by default.
  EXPECT_EQ(control_->StaticTags().size(), 0);
  // Controls use the same tag for getting and setting.
  // The macro doesn't like the static variables
  // being passed directly to assertions.
  int32_t expected_tag = control_tag_;
  ASSERT_EQ(control_->ControlTags().size(), 1);
  EXPECT_EQ(control_->ControlTags()[0], expected_tag);
  ASSERT_EQ(control_->DynamicTags().size(), 1);
  EXPECT_EQ(control_->DynamicTags()[0], expected_tag);
}

TEST_F(ControlTest, PopulateDynamic) {
  uint8_t test_option = 99;
  EXPECT_CALL(*control_, GetValue(_))
      .WillOnce(DoAll(SetArgPointee<0>(test_option), Return(0)));

  android::CameraMetadata metadata;
  EXPECT_EQ(control_->PopulateDynamicFields(&metadata), 0);

  // Should only have added 1 entry.
  EXPECT_EQ(metadata.entryCount(), 1);
  // Should have added the right entry.
  ExpectMetadataEq(metadata, control_tag_, test_option);
}

TEST_F(ControlTest, PopulateDynamicFail) {
  int err = -99;
  EXPECT_CALL(*control_, GetValue(_)).WillOnce(Return(err));

  android::CameraMetadata metadata;
  EXPECT_EQ(control_->PopulateDynamicFields(&metadata), err);

  // Should not have added an entry.
  EXPECT_TRUE(metadata.isEmpty());
}

TEST_F(ControlTest, SupportsRequest) {
  android::CameraMetadata metadata;
  uint8_t test_option = 123;
  ASSERT_EQ(metadata.update(control_tag_, &test_option, 1), android::OK);

  EXPECT_CALL(*control_, IsSupported(test_option)).WillOnce(Return(true));
  EXPECT_EQ(control_->SupportsRequestValues(metadata), true);
}

TEST_F(ControlTest, ArraySupportsRequest) {
  android::CameraMetadata metadata;
  std::array<uint8_t, 2> test_option = {{12, 34}};
  ASSERT_EQ(
      metadata.update(control_tag_, test_option.data(), test_option.size()),
      android::OK);

  MockControl<std::array<uint8_t, 2>> test_control(control_tag_);
  EXPECT_CALL(test_control, IsSupported(test_option)).WillOnce(Return(true));
  EXPECT_EQ(test_control.SupportsRequestValues(metadata), true);
}

TEST_F(ControlTest, SupportsRequestFail) {
  android::CameraMetadata metadata;
  uint8_t test_option = 123;
  ASSERT_EQ(metadata.update(control_tag_, &test_option, 1), android::OK);

  EXPECT_CALL(*control_, IsSupported(test_option)).WillOnce(Return(false));
  EXPECT_EQ(control_->SupportsRequestValues(metadata), false);
}

TEST_F(ControlTest, SupportsRequestInvalidNumber) {
  // Start with a request for multiple values.
  android::CameraMetadata metadata;
  std::vector<uint8_t> test_data = {1, 2, 3};
  ASSERT_EQ(metadata.update(control_tag_, test_data.data(), test_data.size()),
            android::OK);
  EXPECT_EQ(control_->SupportsRequestValues(metadata), false);
}

TEST_F(ControlTest, ArraySupportsRequestInvalidNumber) {
  // Start with a request for a single (non-array) value.
  android::CameraMetadata metadata;
  uint8_t test_data = 1;
  ASSERT_EQ(metadata.update(control_tag_, &test_data, 1), android::OK);

  MockControl<std::array<uint8_t, 2>> test_control(control_tag_);
  EXPECT_EQ(test_control.SupportsRequestValues(metadata), false);
}

TEST_F(ControlTest, SupportsRequestEmpty) {
  android::CameraMetadata metadata;
  EXPECT_EQ(control_->SupportsRequestValues(metadata), true);
}

TEST_F(ControlTest, SetRequest) {
  android::CameraMetadata metadata(1);
  uint8_t test_option = 123;
  ASSERT_EQ(metadata.update(control_tag_, &test_option, 1), android::OK);

  EXPECT_CALL(*control_, SetValue(test_option)).WillOnce(Return(0));
  // Make the request.
  ASSERT_EQ(control_->SetRequestValues(metadata), 0);
}

TEST_F(ControlTest, ArraySetRequest) {
  android::CameraMetadata metadata;
  std::array<uint8_t, 2> test_option = {{12, 34}};
  ASSERT_EQ(
      metadata.update(control_tag_, test_option.data(), test_option.size()),
      android::OK);

  MockControl<std::array<uint8_t, 2>> test_control(control_tag_);
  EXPECT_CALL(test_control, SetValue(test_option)).WillOnce(Return(0));
  EXPECT_EQ(test_control.SetRequestValues(metadata), 0);
}

TEST_F(ControlTest, SetRequestFail) {
  android::CameraMetadata metadata(1);
  uint8_t test_option = 123;
  ASSERT_EQ(metadata.update(control_tag_, &test_option, 1), android::OK);

  int err = -99;
  EXPECT_CALL(*control_, SetValue(test_option)).WillOnce(Return(err));
  EXPECT_EQ(control_->SetRequestValues(metadata), err);
}

TEST_F(ControlTest, SetRequestInvalidNumber) {
  // Start with a request for multiple values.
  android::CameraMetadata metadata;
  std::vector<uint8_t> test_data = {1, 2, 3};
  ASSERT_EQ(metadata.update(control_tag_, test_data.data(), test_data.size()),
            android::OK);
  EXPECT_EQ(control_->SetRequestValues(metadata), -EINVAL);
}

TEST_F(ControlTest, ArraySetRequestInvalidNumber) {
  // Start with a request for a single (non-array) value.
  android::CameraMetadata metadata;
  uint8_t test_data = 1;
  ASSERT_EQ(metadata.update(control_tag_, &test_data, 1), android::OK);

  MockControl<std::array<uint8_t, 2>> test_control(control_tag_);
  EXPECT_EQ(test_control.SetRequestValues(metadata), -EINVAL);
}

TEST_F(ControlTest, SetRequestEmpty) {
  // Should do nothing.
  android::CameraMetadata metadata;
  EXPECT_EQ(control_->SetRequestValues(metadata), 0);
}

}  // namespace v4l2_camera_hal
