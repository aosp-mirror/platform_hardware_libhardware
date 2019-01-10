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
#include <gtest/gtest.h>
#include <system/camera.h>

#include "array_vector.h"
#include "metadata_common.h"

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
  const int32_t max_inputs_tag_ = ANDROID_REQUEST_MAX_NUM_INPUT_STREAMS;
  const int32_t max_outputs_tag_ = ANDROID_REQUEST_MAX_NUM_OUTPUT_STREAMS;
  const int32_t configs_tag_ = ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS;
  const int32_t stalls_tag_ = ANDROID_SCALER_AVAILABLE_STALL_DURATIONS;
  const int32_t reprocess_formats_tag_ =
      ANDROID_SCALER_AVAILABLE_INPUT_OUTPUT_FORMATS_MAP;

  const std::vector<int32_t> valid_orientations_ = {0, 90, 180, 270};
  // TODO(b/31384253): check for required configs/reprocess formats.
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

TEST_F(MetadataReaderTest, MaxInputs) {
  int32_t expected = 12;
  ASSERT_EQ(v4l2_camera_hal::UpdateMetadata(
                metadata_.get(), max_inputs_tag_, expected),
            0);
  FillDUT();
  int32_t actual = expected + 1;
  ASSERT_EQ(dut_->MaxInputStreams(&actual), 0);
  EXPECT_EQ(actual, expected);
}

TEST_F(MetadataReaderTest, EmptyMaxInputs) {
  FillDUT();
  // Max inputs is an optional key; if not present the default is 0.
  int32_t expected = 0;
  int32_t actual = expected + 1;
  ASSERT_EQ(dut_->MaxInputStreams(&actual), 0);
  EXPECT_EQ(actual, expected);
}

TEST_F(MetadataReaderTest, MaxOutputs) {
  std::array<int32_t, 3> expected = {{12, 34, 56}};
  ASSERT_EQ(v4l2_camera_hal::UpdateMetadata(
                metadata_.get(), max_outputs_tag_, expected),
            0);
  FillDUT();
  std::array<int32_t, 3> actual;
  ASSERT_EQ(dut_->MaxOutputStreams(&actual[0], &actual[1], &actual[2]), 0);
  EXPECT_EQ(actual, expected);
}

TEST_F(MetadataReaderTest, InvalidMaxOutputs) {
  // Must be a 3-tuple to be valid.
  std::array<int32_t, 4> invalid = {{12, 34, 56, 78}};
  ASSERT_EQ(v4l2_camera_hal::UpdateMetadata(
                metadata_.get(), max_outputs_tag_, invalid),
            0);
  FillDUT();
  int32_t actual;
  // Don't mind the aliasing since we don't care about the value.
  ASSERT_EQ(dut_->MaxOutputStreams(&actual, &actual, &actual), -EINVAL);
}

TEST_F(MetadataReaderTest, EmptyMaxOutputs) {
  FillDUT();
  int32_t actual;
  // Don't mind the aliasing since we don't care about the value.
  ASSERT_EQ(dut_->MaxOutputStreams(&actual, &actual, &actual), -ENOENT);
}

TEST_F(MetadataReaderTest, StreamConfigurations) {
  v4l2_camera_hal::ArrayVector<int32_t, 4> configs;
  std::array<int32_t, 4> config1{
      {1, 2, 3, ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT}};
  std::array<int32_t, 4> config2{
      {5, 6, 7, ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_INPUT}};
  configs.push_back(config1);
  configs.push_back(config2);
  ASSERT_EQ(
      v4l2_camera_hal::UpdateMetadata(metadata_.get(), configs_tag_, configs),
      0);
  FillDUT();
  std::vector<StreamConfiguration> actual;
  ASSERT_EQ(dut_->StreamConfigurations(&actual), 0);
  ASSERT_EQ(actual.size(), configs.num_arrays());
  EXPECT_EQ(actual[0].spec.format, config1[0]);
  EXPECT_EQ(actual[0].spec.width, config1[1]);
  EXPECT_EQ(actual[0].spec.height, config1[2]);
  EXPECT_EQ(actual[0].direction, config1[3]);
  EXPECT_EQ(actual[1].spec.format, config2[0]);
  EXPECT_EQ(actual[1].spec.width, config2[1]);
  EXPECT_EQ(actual[1].spec.height, config2[2]);
  EXPECT_EQ(actual[1].direction, config2[3]);
}

TEST_F(MetadataReaderTest, InvalidStreamConfigurationDirection) {
  // -1 is not a valid direction.
  std::array<int32_t, 4> config{{1, 2, 3, -1}};
  ASSERT_EQ(
      v4l2_camera_hal::UpdateMetadata(metadata_.get(), configs_tag_, config),
      0);
  FillDUT();
  std::vector<StreamConfiguration> actual;
  ASSERT_EQ(dut_->StreamConfigurations(&actual), -EINVAL);
}

TEST_F(MetadataReaderTest, InvalidStreamConfigurationSize) {
  // Both size dimensions must be > 0.
  std::array<int32_t, 4> config{
      {1, 2, 0, ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_INPUT}};
  ASSERT_EQ(
      v4l2_camera_hal::UpdateMetadata(metadata_.get(), configs_tag_, config),
      0);
  FillDUT();
  std::vector<StreamConfiguration> actual;
  ASSERT_EQ(dut_->StreamConfigurations(&actual), -EINVAL);
}

TEST_F(MetadataReaderTest, InvalidStreamConfigurationNumElements) {
  // Should be a multiple of 4.
  std::array<int32_t, 5> config{
      {1, 2, 3, ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_INPUT, 5}};
  ASSERT_EQ(
      v4l2_camera_hal::UpdateMetadata(metadata_.get(), configs_tag_, config),
      0);
  FillDUT();
  std::vector<StreamConfiguration> actual;
  ASSERT_EQ(dut_->StreamConfigurations(&actual), -EINVAL);
}

// TODO(b/31384253): Test that failure occurs if
// required configurations are not present.

TEST_F(MetadataReaderTest, NoStreamConfigurations) {
  FillDUT();
  std::vector<StreamConfiguration> actual;
  ASSERT_EQ(dut_->StreamConfigurations(&actual), -ENOENT);
}

TEST_F(MetadataReaderTest, StreamStallDurations) {
  v4l2_camera_hal::ArrayVector<int64_t, 4> stalls;
  std::array<int64_t, 4> stall1{{1, 2, 3, 4}};
  std::array<int64_t, 4> stall2{{5, 6, 7, 8}};
  stalls.push_back(stall1);
  stalls.push_back(stall2);
  ASSERT_EQ(
      v4l2_camera_hal::UpdateMetadata(metadata_.get(), stalls_tag_, stalls), 0);
  FillDUT();
  std::vector<StreamStallDuration> actual;
  ASSERT_EQ(dut_->StreamStallDurations(&actual), 0);
  ASSERT_EQ(actual.size(), stalls.num_arrays());
  EXPECT_EQ(actual[0].spec.format, stall1[0]);
  EXPECT_EQ(actual[0].spec.width, stall1[1]);
  EXPECT_EQ(actual[0].spec.height, stall1[2]);
  EXPECT_EQ(actual[0].duration, stall1[3]);
  EXPECT_EQ(actual[1].spec.format, stall2[0]);
  EXPECT_EQ(actual[1].spec.width, stall2[1]);
  EXPECT_EQ(actual[1].spec.height, stall2[2]);
  EXPECT_EQ(actual[1].duration, stall2[3]);
}

TEST_F(MetadataReaderTest, InvalidStreamStallDurationDuration) {
  // -1 is not a valid duration.
  std::array<int64_t, 4> stall{{1, 2, 3, -1}};
  ASSERT_EQ(
      v4l2_camera_hal::UpdateMetadata(metadata_.get(), stalls_tag_, stall), 0);
  FillDUT();
  std::vector<StreamStallDuration> actual;
  ASSERT_EQ(dut_->StreamStallDurations(&actual), -EINVAL);
}

TEST_F(MetadataReaderTest, InvalidStreamStallDurationSize) {
  // Both size dimensions must be > 0.
  std::array<int64_t, 4> stall{{1, 2, 0, 3}};
  ASSERT_EQ(
      v4l2_camera_hal::UpdateMetadata(metadata_.get(), stalls_tag_, stall), 0);
  FillDUT();
  std::vector<StreamStallDuration> actual;
  ASSERT_EQ(dut_->StreamStallDurations(&actual), -EINVAL);
}

TEST_F(MetadataReaderTest, InvalidStreamStallDurationNumElements) {
  // Should be a multiple of 4.
  std::array<int64_t, 5> stall{{1, 2, 3, 4, 5}};
  ASSERT_EQ(
      v4l2_camera_hal::UpdateMetadata(metadata_.get(), stalls_tag_, stall), 0);
  FillDUT();
  std::vector<StreamStallDuration> actual;
  ASSERT_EQ(dut_->StreamStallDurations(&actual), -EINVAL);
}

// TODO(b/31384253): Test that failure occurs if
// YUV_420_888, RAW10, RAW12, RAW_OPAQUE, or IMPLEMENTATION_DEFINED
// formats have stall durations > 0.

TEST_F(MetadataReaderTest, NoStreamStallDurations) {
  FillDUT();
  std::vector<StreamStallDuration> actual;
  ASSERT_EQ(dut_->StreamStallDurations(&actual), -ENOENT);
}

TEST_F(MetadataReaderTest, ReprocessFormats) {
  ReprocessFormatMap expected{{1, {4}}, {2, {5, 6}}, {3, {7, 8, 9}}};
  std::vector<int32_t> raw;
  for (const auto& input_outputs : expected) {
    raw.push_back(input_outputs.first);
    raw.push_back(input_outputs.second.size());
    raw.insert(
        raw.end(), input_outputs.second.begin(), input_outputs.second.end());
  }
  ASSERT_EQ(v4l2_camera_hal::UpdateMetadata(
                metadata_.get(), reprocess_formats_tag_, raw),
            0);
  FillDUT();
  ReprocessFormatMap actual;
  ASSERT_EQ(dut_->ReprocessFormats(&actual), 0);
  EXPECT_EQ(actual, expected);
}

TEST_F(MetadataReaderTest, ReprocessFormatsNoOutputs) {
  // 0 indicates that there are 0 output formats for input format 1,
  // which is not ok.
  std::vector<int32_t> raw{1, 0};
  ASSERT_EQ(v4l2_camera_hal::UpdateMetadata(
                metadata_.get(), reprocess_formats_tag_, raw),
            0);
  FillDUT();
  ReprocessFormatMap actual;
  ASSERT_EQ(dut_->ReprocessFormats(&actual), -EINVAL);
}

TEST_F(MetadataReaderTest, ReprocessFormatsPastEnd) {
  // 3 indicates that there are 3 output formats for input format 1,
  // which is not ok since there are only 2 here.
  std::vector<int32_t> raw{1, 3, 0, 0};
  ASSERT_EQ(v4l2_camera_hal::UpdateMetadata(
                metadata_.get(), reprocess_formats_tag_, raw),
            0);
  FillDUT();
  ReprocessFormatMap actual;
  ASSERT_EQ(dut_->ReprocessFormats(&actual), -EINVAL);
}

TEST_F(MetadataReaderTest, EmptyReprocessFormats) {
  FillDUT();
  ReprocessFormatMap actual;
  ASSERT_EQ(dut_->ReprocessFormats(&actual), -ENOENT);
}

}  // namespace default_camera_hal
