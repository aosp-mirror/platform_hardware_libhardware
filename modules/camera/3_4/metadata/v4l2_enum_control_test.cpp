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

#include "v4l2_enum_control.h"

#include <array>

#include <camera/CameraMetadata.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../v4l2_wrapper_mock.h"
#include "array_vector.h"
#include "test_common.h"

using testing::AtMost;
using testing::Return;
using testing::SetArgPointee;
using testing::Test;
using testing::_;

namespace v4l2_camera_hal {

class V4L2EnumControlTest : public Test {
 protected:
  V4L2EnumControlTest()
      : options_({10, 20, 30, 50}),  // Subset of the full map.
        options_map_(
            {{0, 0}, {1, 10}, {2, 20}, {3, 30}, {4, 40}, {5, 50}, {6, 60}}) {}

  virtual void SetUp() {
    device_.reset(new V4L2WrapperMock());
    control_.reset(new V4L2EnumControl(device_,
                                       v4l2_control_,
                                       control_tag_,
                                       options_tag_,
                                       options_map_,
                                       options_));
  }

  virtual uint8_t V4L2ToMetadata(int32_t value) {
    return options_map_.at(value);
  }

  virtual int32_t MetadataToV4L2(uint8_t value) {
    for (auto kv : options_map_) {
      if (kv.second == value) {
        return kv.first;
      }
    }
    return -1;
  }

  std::unique_ptr<V4L2EnumControl> control_;
  std::shared_ptr<V4L2WrapperMock> device_;

  static constexpr int v4l2_control_ = 123;
  // Need tags that match the data type (uint8_t) being passed.
  static constexpr int32_t options_tag_ = ANDROID_CONTROL_AVAILABLE_SCENE_MODES;
  static constexpr int32_t control_tag_ = ANDROID_CONTROL_SCENE_MODE;

  const std::vector<uint8_t> options_;
  const std::map<int32_t, uint8_t> options_map_;
};

TEST_F(V4L2EnumControlTest, NewV4L2EnumSuccess) {
  // Should query the device.
  v4l2_query_ext_ctrl query_result;
  query_result.type = V4L2_CTRL_TYPE_MENU;
  query_result.minimum = 1;
  query_result.maximum = 7;
  query_result.step = 2;
  EXPECT_CALL(*device_, QueryControl(v4l2_control_, _))
      .WillOnce(DoAll(SetArgPointee<1>(query_result), Return(0)));

  std::unique_ptr<V4L2EnumControl> test_control(
      V4L2EnumControl::NewV4L2EnumControl(
          device_, v4l2_control_, control_tag_, options_tag_, options_map_));
  // Shouldn't be null.
  ASSERT_NE(test_control.get(), nullptr);

  // Should pass through tags.
  // The macro doesn't like the static variables
  // being passed directly to assertions.
  int32_t expected_tag = options_tag_;
  ASSERT_EQ(test_control->StaticTags().size(), 1);
  EXPECT_EQ(test_control->StaticTags()[0], expected_tag);
  // Controls use the same tag for getting and setting.
  expected_tag = control_tag_;
  ASSERT_EQ(test_control->ControlTags().size(), 1);
  EXPECT_EQ(test_control->ControlTags()[0], expected_tag);
  ASSERT_EQ(test_control->DynamicTags().size(), 1);
  EXPECT_EQ(test_control->DynamicTags()[0], expected_tag);

  // Should populate the options according to capabilities returned.
  android::CameraMetadata metadata;
  ASSERT_EQ(test_control->PopulateStaticFields(&metadata), 0);
  // Min 1, max 7, step 2 means {1,3,5} converted to metadata values.
  // There is no 7 in the map, so it shouldn't be present.
  std::vector<uint8_t> expected_options;
  expected_options.push_back(V4L2ToMetadata(1));
  expected_options.push_back(V4L2ToMetadata(3));
  expected_options.push_back(V4L2ToMetadata(5));
  ExpectMetadataEq(metadata, options_tag_, expected_options);
}

TEST_F(V4L2EnumControlTest, NewV4L2EnumFailed) {
  // Querying the device fails.
  int err = -99;
  EXPECT_CALL(*device_, QueryControl(v4l2_control_, _)).WillOnce(Return(err));

  std::unique_ptr<V4L2EnumControl> test_control(
      V4L2EnumControl::NewV4L2EnumControl(
          device_, v4l2_control_, control_tag_, options_tag_, options_map_));
  // Should return null to indicate error.
  ASSERT_EQ(test_control.get(), nullptr);
}

TEST_F(V4L2EnumControlTest, NewV4L2EnumInvalid) {
  // Control type is not supported.
  v4l2_query_ext_ctrl query_result;
  query_result.type = V4L2_CTRL_TYPE_INTEGER;
  query_result.minimum = 1;
  query_result.maximum = 5;
  query_result.step = 2;
  EXPECT_CALL(*device_, QueryControl(v4l2_control_, _))
      .WillOnce(DoAll(SetArgPointee<1>(query_result), Return(0)));

  std::unique_ptr<V4L2EnumControl> test_control(
      V4L2EnumControl::NewV4L2EnumControl(
          device_, v4l2_control_, control_tag_, options_tag_, options_map_));
  // Should return null to indicate error.
  ASSERT_FALSE(test_control);
}

TEST_F(V4L2EnumControlTest, SetValue) {
  // Should go through the device.
  uint8_t val = options_[1];
  EXPECT_CALL(*device_, SetControl(v4l2_control_, MetadataToV4L2(val), _))
      .WillOnce(Return(0));
  EXPECT_EQ(control_->SetValue(val), 0);
}

TEST_F(V4L2EnumControlTest, SetValueFail) {
  // Should go through the device but fail.
  uint8_t val = options_[1];
  int err = -99;
  EXPECT_CALL(*device_, SetControl(v4l2_control_, MetadataToV4L2(val), _))
      .WillOnce(Return(err));
  EXPECT_EQ(control_->SetValue(val), err);
}

TEST_F(V4L2EnumControlTest, SetInvalidValue) {
  uint8_t val = 99;  // Not one of the supported values.
  EXPECT_EQ(control_->SetValue(val), -EINVAL);
}

TEST_F(V4L2EnumControlTest, SetUnmapped) {
  // If the enum control is validly constructed, this should never happen.
  // Purposefully misconstruct a device for this test (empty map).
  V4L2EnumControl test_control(
      device_, v4l2_control_, control_tag_, options_tag_, {}, options_);
  EXPECT_EQ(test_control.SetValue(options_[0]), -ENODEV);
}

TEST_F(V4L2EnumControlTest, GetValue) {
  // Should go through the device.
  uint8_t expected = options_[0];
  EXPECT_CALL(*device_, GetControl(v4l2_control_, _))
      .WillOnce(DoAll(SetArgPointee<1>(MetadataToV4L2(expected)), Return(0)));
  uint8_t actual =
      expected + 1;  // Initialize to something other than expected.
  EXPECT_EQ(control_->GetValue(&actual), 0);
  EXPECT_EQ(actual, expected);
}

TEST_F(V4L2EnumControlTest, GetValueFail) {
  // Should go through the device but fail.
  int err = -99;
  EXPECT_CALL(*device_, GetControl(v4l2_control_, _)).WillOnce(Return(err));
  uint8_t unused;
  EXPECT_EQ(control_->GetValue(&unused), err);
}

TEST_F(V4L2EnumControlTest, GetUnmapped) {
  int32_t invalid = -99;  // Not in our map.
  EXPECT_CALL(*device_, GetControl(v4l2_control_, _))
      .WillOnce(DoAll(SetArgPointee<1>(invalid), Return(0)));
  uint8_t unused;
  EXPECT_EQ(control_->GetValue(&unused), -ENODEV);
}

}  // namespace v4l2_camera_hal
