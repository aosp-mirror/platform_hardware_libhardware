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

#include "fixed_property.h"

#include <array>
#include <vector>

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

class FixedPropertyTest : public Test {
 protected:
  // Need tags that match the data types being passed.
  static constexpr int32_t byte_tag_ = ANDROID_CONTROL_SCENE_MODE_OVERRIDES;
  static constexpr int32_t float_tag_ = ANDROID_COLOR_CORRECTION_GAINS;
  static constexpr int32_t int_tag_ = ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION;
  static constexpr int32_t int_tag2_ = ANDROID_JPEG_ORIENTATION;
};

TEST_F(FixedPropertyTest, Tags) {
  FixedProperty<int32_t> property(int_tag_, 1);

  // Should have only the single tag it was constructed with.
  EXPECT_EQ(property.ControlTags().size(), 0);
  EXPECT_EQ(property.DynamicTags().size(), 0);
  ASSERT_EQ(property.StaticTags().size(), 1);
  // The macro doesn't like the int_tag_ variable being passed in directly.
  int32_t expected_tag = int_tag_;
  EXPECT_EQ(property.StaticTags()[0], expected_tag);
}

TEST_F(FixedPropertyTest, PopulateStaticSingleNumber) {
  // Set up a fixed property.
  int32_t data = 1234;
  FixedProperty<int32_t> property(int_tag_, data);

  // Populate static fields.
  android::CameraMetadata metadata;
  ASSERT_EQ(property.PopulateStaticFields(&metadata), 0);

  // Check the results.
  // Should only have added 1 entry.
  EXPECT_EQ(metadata.entryCount(), 1);
  // Should have added the right entry.
  camera_metadata_entry_t entry = metadata.find(int_tag_);
  ASSERT_EQ(entry.count, 1);
  EXPECT_EQ(*entry.data.i32, data);
}

TEST_F(FixedPropertyTest, PopulateStaticVector) {
  // Set up a fixed property.
  std::vector<float> data({0.1, 2.3, 4.5, 6.7});
  FixedProperty<std::vector<float>> property(float_tag_, data);

  // Populate static fields.
  android::CameraMetadata metadata;
  ASSERT_EQ(property.PopulateStaticFields(&metadata), 0);

  // Check the results.
  // Should only have added 1 entry.
  EXPECT_EQ(metadata.entryCount(), 1);
  // Should have added the right entry.
  camera_metadata_entry_t entry = metadata.find(float_tag_);
  ASSERT_EQ(entry.count, data.size());
  for (size_t i = 0; i < data.size(); ++i) {
    EXPECT_EQ(entry.data.f[i], data[i]);
  }
}

TEST_F(FixedPropertyTest, PopulateStaticArray) {
  // Set up a fixed property.
  std::array<float, 4> data({{0.1, 2.3, 4.5, 6.7}});
  FixedProperty<std::array<float, 4>> property(float_tag_, data);

  // Populate static fields.
  android::CameraMetadata metadata;
  ASSERT_EQ(property.PopulateStaticFields(&metadata), 0);

  // Check the results.
  // Should only have added 1 entry.
  EXPECT_EQ(metadata.entryCount(), 1);
  // Should have added the right entry.
  camera_metadata_entry_t entry = metadata.find(float_tag_);
  ASSERT_EQ(entry.count, data.size());
  for (size_t i = 0; i < data.size(); ++i) {
    EXPECT_EQ(entry.data.f[i], data[i]);
  }
}

TEST_F(FixedPropertyTest, PopulateStaticArrayVector) {
  // Set up a fixed property.
  ArrayVector<uint8_t, 3> data;
  data.push_back({{1, 2, 3}});
  data.push_back({{4, 5, 6}});
  FixedProperty<ArrayVector<uint8_t, 3>> property(byte_tag_, data);

  // Populate static fields.
  android::CameraMetadata metadata;
  ASSERT_EQ(property.PopulateStaticFields(&metadata), 0);

  // Check the results.
  // Should only have added 1 entry.
  EXPECT_EQ(metadata.entryCount(), 1);
  // Should have added the right entry.
  camera_metadata_entry_t entry = metadata.find(byte_tag_);
  ASSERT_EQ(entry.count, data.total_num_elements());
  for (size_t i = 0; i < data.total_num_elements(); ++i) {
    EXPECT_EQ(entry.data.u8[i], data.data()[i]);
  }
}

TEST_F(FixedPropertyTest, PopulateDynamic) {
  FixedProperty<int32_t> property(int_tag_, 1);

  android::CameraMetadata metadata;
  EXPECT_EQ(property.PopulateDynamicFields(&metadata), 0);

  // Shouldn't have added anything.
  EXPECT_TRUE(metadata.isEmpty());
}

TEST_F(FixedPropertyTest, SupportsRequest) {
  FixedProperty<int32_t> property(int_tag_, 1);
  android::CameraMetadata metadata;
  EXPECT_EQ(property.SupportsRequestValues(metadata), true);
}

TEST_F(FixedPropertyTest, SetRequest) {
  FixedProperty<int32_t> property(int_tag_, 1);
  android::CameraMetadata metadata;
  EXPECT_EQ(property.SetRequestValues(metadata), 0);
}

}  // namespace v4l2_camera_hal
