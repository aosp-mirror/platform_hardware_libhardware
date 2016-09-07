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

#include "metadata_reader.h"

#include <camera/CameraMetadata.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <system/camera.h>

#include "metadata_common.h"

using testing::AtMost;
using testing::Expectation;
using testing::Return;
using testing::Test;

namespace default_camera_hal {

class MetadataReaderTest : public Test {
 protected:
  void SetUp() {
    ResetMetadata();
    // FillDUT should be called before using the device under test.
    dut_.reset();
  }

  void ResetMetadata() {
    metadata_ = std::make_unique<android::CameraMetadata>();
  }

  void FillDUT() {
    dut_ = std::make_unique<MetadataReader>(std::move(metadata_));
    ResetMetadata();
  }

  std::unique_ptr<MetadataReader> dut_;
  std::unique_ptr<android::CameraMetadata> metadata_;

  const int32_t facing_tag_ = ANDROID_LENS_FACING;
  const int32_t orientation_tag_ = ANDROID_SENSOR_ORIENTATION;
  const std::vector<int32_t> valid_orientations_ = {0, 90, 180, 270};
};

TEST_F(MetadataReaderTest, FacingTranslations) {
  // Check that the enums are converting properly.
  std::map<uint8_t, int> translations{
      {ANDROID_LENS_FACING_FRONT, CAMERA_FACING_FRONT},
      {ANDROID_LENS_FACING_BACK, CAMERA_FACING_BACK},
      {ANDROID_LENS_FACING_EXTERNAL, CAMERA_FACING_EXTERNAL}};
  for (const auto& translation : translations) {
    ASSERT_EQ(v4l2_camera_hal::UpdateMetadata(
                  metadata_.get(), facing_tag_, translation.first),
              0);
    FillDUT();

    int expected = translation.second;
    int actual = expected + 1;
    EXPECT_EQ(dut_->Facing(&actual), 0);
    EXPECT_EQ(actual, expected);
  }
}

TEST_F(MetadataReaderTest, InvalidFacing) {
  uint8_t invalid = 99;
  ASSERT_EQ(
      v4l2_camera_hal::UpdateMetadata(metadata_.get(), facing_tag_, invalid),
      0);
  FillDUT();
  int actual = 0;
  EXPECT_EQ(dut_->Facing(&actual), -EINVAL);
}

TEST_F(MetadataReaderTest, EmptyFacing) {
  FillDUT();
  int actual = 0;
  EXPECT_EQ(dut_->Facing(&actual), -ENOENT);
}

TEST_F(MetadataReaderTest, ValidOrientations) {
  for (int32_t orientation : valid_orientations_) {
    ASSERT_EQ(v4l2_camera_hal::UpdateMetadata(
                  metadata_.get(), orientation_tag_, orientation),
              0);
    FillDUT();

    int actual = orientation + 1;
    EXPECT_EQ(dut_->Orientation(&actual), 0);
    EXPECT_EQ(actual, orientation);
  }
}

TEST_F(MetadataReaderTest, InvalidOrientations) {
  // High.
  for (int32_t orientation : valid_orientations_) {
    ASSERT_EQ(v4l2_camera_hal::UpdateMetadata(
                  metadata_.get(), orientation_tag_, orientation + 1),
              0);
    FillDUT();
    int actual = 0;
    EXPECT_EQ(dut_->Orientation(&actual), -EINVAL);
  }
  // Low.
  for (int32_t orientation : valid_orientations_) {
    ASSERT_EQ(v4l2_camera_hal::UpdateMetadata(
                  metadata_.get(), orientation_tag_, orientation - 1),
              0);
    FillDUT();
    int actual = 0;
    EXPECT_EQ(dut_->Orientation(&actual), -EINVAL);
  }
}

TEST_F(MetadataReaderTest, EmptyOrientation) {
  FillDUT();
  int actual = 0;
  EXPECT_EQ(dut_->Orientation(&actual), -ENOENT);
}

}  // namespace default_camera_hal
