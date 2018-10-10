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

#include "property.h"

#include <array>
#include <vector>

#include <camera/CameraMetadata.h>
#include <gtest/gtest.h>
#include <hardware/camera3.h>
#include "array_vector.h"
#include "metadata_common.h"
#include "test_common.h"

using testing::Test;

namespace v4l2_camera_hal {

class PropertyTest : public Test {
 protected:
  // Need tags that match the data types being passed.
  static constexpr int32_t byte_tag_ = ANDROID_CONTROL_SCENE_MODE_OVERRIDES;
  static constexpr int32_t float_tag_ = ANDROID_COLOR_CORRECTION_GAINS;
  static constexpr int32_t int_tag_ = ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION;
  static constexpr int32_t int_tag2_ = ANDROID_JPEG_ORIENTATION;
};

TEST_F(PropertyTest, Tags) {
  Property<int32_t> property(int_tag_, 1);

  // Should have only the single tag it was constructed with.
  EXPECT_EQ(property.ControlTags().size(), 0u);
  EXPECT_EQ(property.DynamicTags().size(), 0u);
  ASSERT_EQ(property.StaticTags().size(), 1u);
  // The macro doesn't like the int_tag_ variable being passed in directly.
  int32_t expected_tag = int_tag_;
  EXPECT_EQ(property.StaticTags()[0], expected_tag);
}

TEST_F(PropertyTest, PopulateStaticSingleNumber) {
  // Set up a fixed property.
  int32_t data = 1234;
  Property<int32_t> property(int_tag_, data);

  // Populate static fields.
  android::CameraMetadata metadata;
  ASSERT_EQ(property.PopulateStaticFields(&metadata), 0);

  // Check the results.
  // Should only have added 1 entry.
  EXPECT_EQ(metadata.entryCount(), 1u);
  // Should have added the right entry.
  ExpectMetadataEq(metadata, int_tag_, data);
}

// TODO(b/30839858): These tests are really testing the metadata_common.h
// UpdateMetadata methods, and shouldn't be conducted here.
TEST_F(PropertyTest, PopulateStaticVector) {
  // Set up a fixed property.
  std::vector<float> data({0.1, 2.3, 4.5, 6.7});
  Property<std::vector<float>> property(float_tag_, data);

  // Populate static fields.
  android::CameraMetadata metadata;
  ASSERT_EQ(property.PopulateStaticFields(&metadata), 0);

  // Check the results.
  // Should only have added 1 entry.
  EXPECT_EQ(metadata.entryCount(), 1u);
  // Should have added the right entry.
  ExpectMetadataEq(metadata, float_tag_, data);
}

TEST_F(PropertyTest, PopulateStaticArray) {
  // Set up a fixed property.
  std::array<float, 4> data({{0.1, 2.3, 4.5, 6.7}});
  Property<std::array<float, 4>> property(float_tag_, data);

  // Populate static fields.
  android::CameraMetadata metadata;
  ASSERT_EQ(property.PopulateStaticFields(&metadata), 0);

  // Check the results.
  // Should only have added 1 entry.
  EXPECT_EQ(metadata.entryCount(), 1u);
  // Should have added the right entry.
  ExpectMetadataEq(metadata, float_tag_, data);
}

TEST_F(PropertyTest, PopulateStaticArrayVector) {
  // Set up a fixed property.
  ArrayVector<uint8_t, 3> data;
  data.push_back({{1, 2, 3}});
  data.push_back({{4, 5, 6}});
  Property<ArrayVector<uint8_t, 3>> property(byte_tag_, data);

  // Populate static fields.
  android::CameraMetadata metadata;
  ASSERT_EQ(property.PopulateStaticFields(&metadata), 0);

  // Check the results.
  // Should only have added 1 entry.
  EXPECT_EQ(metadata.entryCount(), 1u);
  // Should have added the right entry.
  ExpectMetadataEq(metadata, byte_tag_, data);
}

TEST_F(PropertyTest, PopulateDynamic) {
  Property<int32_t> property(int_tag_, 1);

  android::CameraMetadata metadata;
  EXPECT_EQ(property.PopulateDynamicFields(&metadata), 0);

  // Shouldn't have added anything.
  EXPECT_TRUE(metadata.isEmpty());
}

TEST_F(PropertyTest, PopulateTemplate) {
  Property<int32_t> property(int_tag_, 1);

  for (int i = 1; i < CAMERA3_TEMPLATE_COUNT; ++i) {
    android::CameraMetadata metadata;
    EXPECT_EQ(property.PopulateTemplateRequest(i, &metadata), 0);
    // Shouldn't have added anything.
    EXPECT_TRUE(metadata.isEmpty());
  }
}

TEST_F(PropertyTest, SupportsRequest) {
  Property<int32_t> property(int_tag_, 1);
  android::CameraMetadata metadata;
  EXPECT_EQ(property.SupportsRequestValues(metadata), true);
}

TEST_F(PropertyTest, SetRequest) {
  Property<int32_t> property(int_tag_, 1);
  android::CameraMetadata metadata;
  EXPECT_EQ(property.SetRequestValues(metadata), 0);
}

}  // namespace v4l2_camera_hal
