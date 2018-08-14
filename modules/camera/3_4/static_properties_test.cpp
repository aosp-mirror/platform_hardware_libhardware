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

#include "static_properties.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <hardware/camera3.h>
#include <system/camera.h>

#include "metadata/metadata_reader_mock.h"

using testing::AtMost;
using testing::Expectation;
using testing::Return;
using testing::SetArgPointee;
using testing::Test;
using testing::_;

namespace default_camera_hal {

class StaticPropertiesTest : public Test {
 protected:
  virtual void SetUp() {
    // Ensure tests will probably fail if PrepareDUT isn't called.
    dut_.reset();
    mock_reader_ = std::make_unique<MetadataReaderMock>();
  }

  void PrepareDUT() {
    dut_.reset(StaticProperties::NewStaticProperties(std::move(mock_reader_)));
  }

  void PrepareDefaultDUT() {
    SetDefaultExpectations();
    PrepareDUT();
    ASSERT_NE(dut_, nullptr);
  }

  void SetDefaultExpectations() {
    EXPECT_CALL(*mock_reader_, Facing(_))
        .Times(AtMost(1))
        .WillOnce(DoAll(SetArgPointee<0>(test_facing_), Return(0)));
    EXPECT_CALL(*mock_reader_, Orientation(_))
        .Times(AtMost(1))
        .WillOnce(DoAll(SetArgPointee<0>(test_orientation_), Return(0)));
    EXPECT_CALL(*mock_reader_, MaxInputStreams(_))
        .Times(AtMost(1))
        .WillOnce(DoAll(SetArgPointee<0>(test_max_inputs_), Return(0)));
    EXPECT_CALL(*mock_reader_, MaxOutputStreams(_, _, _))
        .Times(AtMost(1))
        .WillOnce(DoAll(SetArgPointee<0>(test_max_raw_outputs_),
                        SetArgPointee<1>(test_max_non_stalling_outputs_),
                        SetArgPointee<2>(test_max_stalling_outputs_),
                        Return(0)));
    EXPECT_CALL(*mock_reader_, RequestCapabilities(_))
        .Times(AtMost(1))
        .WillOnce(
            DoAll(SetArgPointee<0>(test_request_capabilities_), Return(0)));
    EXPECT_CALL(*mock_reader_, StreamConfigurations(_))
        .Times(AtMost(1))
        .WillOnce(DoAll(SetArgPointee<0>(test_configs_), Return(0)));
    EXPECT_CALL(*mock_reader_, StreamStallDurations(_))
        .Times(AtMost(1))
        .WillOnce(DoAll(SetArgPointee<0>(test_stalls_), Return(0)));
    EXPECT_CALL(*mock_reader_, ReprocessFormats(_))
        .Times(AtMost(1))
        .WillOnce(DoAll(SetArgPointee<0>(test_reprocess_map_), Return(0)));
  }

  camera3_stream_t MakeStream(int32_t format,
                              bool output = true,
                              bool input = false,
                              int32_t width = kWidth,
                              int32_t height = kHeight) {
    int type = -1;
    if (output && input) {
      type = CAMERA3_STREAM_BIDIRECTIONAL;
    } else if (output) {
      type = CAMERA3_STREAM_OUTPUT;
    } else if (input) {
      type = CAMERA3_STREAM_INPUT;
    }
    camera3_stream_t stream;
    stream.stream_type = type;
    stream.width = width;
    stream.height = height;
    stream.format = format;
    return stream;
  }

  void ExpectConfigurationSupported(std::vector<camera3_stream_t>& streams,
                                    bool expected) {
    std::vector<camera3_stream_t*> stream_addresses;
    for (size_t i = 0; i < streams.size(); ++i) {
      stream_addresses.push_back(&streams[i]);
    }
    camera3_stream_configuration_t config = {
        static_cast<uint32_t>(stream_addresses.size()),
        stream_addresses.data(),
        CAMERA3_STREAM_CONFIGURATION_NORMAL_MODE,
        nullptr};
    PrepareDefaultDUT();
    EXPECT_EQ(dut_->StreamConfigurationSupported(&config), expected);
  }

  std::unique_ptr<StaticProperties> dut_;
  std::unique_ptr<MetadataReaderMock> mock_reader_;

  // Some helper values used for stream testing.
  static constexpr int32_t kWidth = 320;
  static constexpr int32_t kHeight = 240;
  static constexpr int32_t kAlternateWidth = 640;
  static constexpr int32_t kAlternateHeight = 480;

  const int test_facing_ = CAMERA_FACING_FRONT;
  const int test_orientation_ = 90;
  const int32_t test_max_inputs_ = 3;
  const int32_t test_max_raw_outputs_ = 1;
  const int32_t test_max_non_stalling_outputs_ = 2;
  const int32_t test_max_stalling_outputs_ = 3;
  const std::set<uint8_t> test_request_capabilities_ = {
      ANDROID_REQUEST_AVAILABLE_CAPABILITIES_BACKWARD_COMPATIBLE,
      ANDROID_REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR,
      ANDROID_REQUEST_AVAILABLE_CAPABILITIES_PRIVATE_REPROCESSING};

  // Some formats for various purposes (in various combinations,
  // these types should be capable of testing all failure conditions).
  const int32_t output_multisize_non_stalling_ = 1;
  const int32_t bidirectional_self_supporting_stalling_ = 2;
  const int32_t bidirectional_raw_ = HAL_PIXEL_FORMAT_RAW10;
  const int32_t input_ = 3;
  const int32_t other = input_;

  const std::vector<StreamConfiguration> test_configs_ = {
      {{{output_multisize_non_stalling_,
         kWidth,
         kHeight,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT}}},
      {{{output_multisize_non_stalling_,
         kAlternateWidth,
         kAlternateHeight,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT}}},
      {{{bidirectional_self_supporting_stalling_,
         kWidth,
         kHeight,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_INPUT}}},
      {{{bidirectional_self_supporting_stalling_,
         kWidth,
         kHeight,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT}}},
      {{{bidirectional_raw_,
         kWidth,
         kHeight,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_INPUT}}},
      {{{bidirectional_raw_,
         kWidth,
         kHeight,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT}}},
      {{{input_,
         kWidth,
         kHeight,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_INPUT}}}};
  // Raw having a stall duration shouldn't matter,
  // it should still be counted as the raw type.
  const std::vector<StreamStallDuration> test_stalls_ = {
      {{{output_multisize_non_stalling_, kWidth, kHeight, 0}}},
      {{{output_multisize_non_stalling_,
         kAlternateWidth,
         kAlternateHeight,
         0}}},
      {{{bidirectional_self_supporting_stalling_, kWidth, kHeight, 10}}},
      {{{bidirectional_raw_, kWidth, kHeight, 15}}}};
  // Format 2 can go to itself or 1. 3 and RAW can only go to 1.
  const ReprocessFormatMap test_reprocess_map_ = {
      {bidirectional_self_supporting_stalling_,
       {output_multisize_non_stalling_,
        bidirectional_self_supporting_stalling_}},
      {bidirectional_raw_, {output_multisize_non_stalling_}},
      {input_, {output_multisize_non_stalling_}}};
  // Codify the above information about format capabilities in some helpful
  // vectors.
  int32_t multi_size_format_ = 1;
  const std::vector<int32_t> input_formats_ = {2, 3, HAL_PIXEL_FORMAT_RAW10};
  const std::vector<int32_t> output_formats_ = {1, 2, HAL_PIXEL_FORMAT_RAW10};
};

TEST_F(StaticPropertiesTest, FactorySuccess) {
  PrepareDefaultDUT();
  EXPECT_EQ(dut_->facing(), test_facing_);
  EXPECT_EQ(dut_->orientation(), test_orientation_);

  // Stream configurations tested seperately.
}

TEST_F(StaticPropertiesTest, FactoryFailedFacing) {
  SetDefaultExpectations();
  // Override with a failure expectation.
  EXPECT_CALL(*mock_reader_, Facing(_)).WillOnce(Return(99));
  PrepareDUT();
  EXPECT_EQ(dut_, nullptr);
}

TEST_F(StaticPropertiesTest, FactoryFailedOrientation) {
  SetDefaultExpectations();
  // Override with a failure expectation.
  EXPECT_CALL(*mock_reader_, Orientation(_)).WillOnce(Return(99));
  PrepareDUT();
  EXPECT_EQ(dut_, nullptr);
}

TEST_F(StaticPropertiesTest, FactoryFailedMaxInputs) {
  SetDefaultExpectations();
  // Override with a failure expectation.
  EXPECT_CALL(*mock_reader_, MaxInputStreams(_)).WillOnce(Return(99));
  PrepareDUT();
  EXPECT_EQ(dut_, nullptr);
}

TEST_F(StaticPropertiesTest, FactoryFailedMaxOutputs) {
  SetDefaultExpectations();
  // Override with a failure expectation.
  EXPECT_CALL(*mock_reader_, MaxOutputStreams(_, _, _)).WillOnce(Return(99));
  PrepareDUT();
  EXPECT_EQ(dut_, nullptr);
}

TEST_F(StaticPropertiesTest, FactoryFailedRequestCapabilities) {
  SetDefaultExpectations();
  // Override with a failure expectation.
  EXPECT_CALL(*mock_reader_, RequestCapabilities(_)).WillOnce(Return(99));
  PrepareDUT();
  EXPECT_EQ(dut_, nullptr);
}

TEST_F(StaticPropertiesTest, FactoryFailedStreamConfigs) {
  SetDefaultExpectations();
  // Override with a failure expectation.
  EXPECT_CALL(*mock_reader_, StreamConfigurations(_)).WillOnce(Return(99));
  PrepareDUT();
  EXPECT_EQ(dut_, nullptr);
}

TEST_F(StaticPropertiesTest, FactoryFailedStallDurations) {
  SetDefaultExpectations();
  // Override with a failure expectation.
  EXPECT_CALL(*mock_reader_, StreamStallDurations(_)).WillOnce(Return(99));
  PrepareDUT();
  EXPECT_EQ(dut_, nullptr);
}

TEST_F(StaticPropertiesTest, FactoryFailedReprocessFormats) {
  SetDefaultExpectations();
  // Override with a failure expectation.
  EXPECT_CALL(*mock_reader_, ReprocessFormats(_)).WillOnce(Return(99));
  PrepareDUT();
  EXPECT_EQ(dut_, nullptr);
}

TEST_F(StaticPropertiesTest, FactoryNoReprocessFormats) {
  // If there are no inputs allowed, the reprocess formats shouldn't matter.
  SetDefaultExpectations();
  // Override max inputs.
  EXPECT_CALL(*mock_reader_, MaxInputStreams(_))
      .WillOnce(DoAll(SetArgPointee<0>(0), Return(0)));
  // Override reprocess formats with a failure expectation.
  EXPECT_CALL(*mock_reader_, ReprocessFormats(_))
      .Times(AtMost(1))
      .WillOnce(Return(99));
  PrepareDUT();
  // Should be ok.
  EXPECT_NE(dut_, nullptr);
}

TEST_F(StaticPropertiesTest, FactoryInvalidCapabilities) {
  SetDefaultExpectations();
  // Override configs with an extra output format.
  std::vector<StreamConfiguration> configs = test_configs_;
  configs.push_back(
      {{{5,
         kWidth,
         kHeight,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT}}});
  EXPECT_CALL(*mock_reader_, StreamConfigurations(_))
      .WillOnce(DoAll(SetArgPointee<0>(configs), Return(0)));
  PrepareDUT();
  // Should fail because not every output has a stall.
  EXPECT_EQ(dut_, nullptr);
}

TEST_F(StaticPropertiesTest, InvalidReprocessNoInputs) {
  SetDefaultExpectations();
  // Override configs by removing all inputs.
  std::vector<StreamConfiguration> configs = test_configs_;
  for (auto it = configs.begin(); it != configs.end();) {
    if ((*it).direction ==
        ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_INPUT) {
      it = configs.erase(it);
    } else {
      ++it;
    }
  }
  EXPECT_CALL(*mock_reader_, StreamConfigurations(_))
      .WillOnce(DoAll(SetArgPointee<0>(configs), Return(0)));
  PrepareDUT();
  // Should fail because inputs are supported but there are no input formats.
  EXPECT_EQ(dut_, nullptr);
}

TEST_F(StaticPropertiesTest, InvalidReprocessExtraInput) {
  SetDefaultExpectations();
  // Override configs with an extra input format.
  std::vector<StreamConfiguration> configs = test_configs_;
  configs.push_back({{{5,
                       kWidth,
                       kHeight,
                       ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_INPUT}}});
  EXPECT_CALL(*mock_reader_, StreamConfigurations(_))
      .WillOnce(DoAll(SetArgPointee<0>(configs), Return(0)));
  PrepareDUT();
  // Should fail because no reprocess outputs are listed for the extra input.
  EXPECT_EQ(dut_, nullptr);
}

TEST_F(StaticPropertiesTest, InvalidReprocessExtraMapEntry) {
  SetDefaultExpectations();
  // Override the reprocess map with an extra entry.
  ReprocessFormatMap reprocess_map = test_reprocess_map_;
  reprocess_map[5] = {1};
  EXPECT_CALL(*mock_reader_, ReprocessFormats(_))
      .WillOnce(DoAll(SetArgPointee<0>(reprocess_map), Return(0)));
  PrepareDUT();
  // Should fail because the extra map entry doesn't correspond to an input.
  EXPECT_EQ(dut_, nullptr);
}

TEST_F(StaticPropertiesTest, InvalidReprocessWrongMapEntries) {
  SetDefaultExpectations();
  // Override the reprocess map replacing the entry for the
  // input-only format with the output-only format.
  ReprocessFormatMap reprocess_map = test_reprocess_map_;
  reprocess_map.erase(input_);
  reprocess_map[output_multisize_non_stalling_] = {1};
  EXPECT_CALL(*mock_reader_, ReprocessFormats(_))
      .WillOnce(DoAll(SetArgPointee<0>(reprocess_map), Return(0)));
  PrepareDUT();
  // Should fail because not all input formats are present/
  // one of the map "input" formats is output only.
  EXPECT_EQ(dut_, nullptr);
}

TEST_F(StaticPropertiesTest, InvalidReprocessNotAnOutput) {
  SetDefaultExpectations();
  // Override the reprocess map with a non-output output entry.
  ReprocessFormatMap reprocess_map = test_reprocess_map_;
  reprocess_map[input_].insert(input_);
  EXPECT_CALL(*mock_reader_, ReprocessFormats(_))
      .WillOnce(DoAll(SetArgPointee<0>(reprocess_map), Return(0)));
  PrepareDUT();
  // Should fail because a specified output format doesn't support output.
  EXPECT_EQ(dut_, nullptr);
}

TEST_F(StaticPropertiesTest, TemplatesValid) {
  PrepareDefaultDUT();
  for (int i = 1; i < CAMERA3_TEMPLATE_COUNT; ++i) {
    EXPECT_TRUE(dut_->TemplateSupported(i));
  }
}

TEST_F(StaticPropertiesTest, ConfigureSingleOutput) {
  std::vector<camera3_stream_t> streams;
  streams.push_back(MakeStream(output_multisize_non_stalling_));
  ExpectConfigurationSupported(streams, true);
}

TEST_F(StaticPropertiesTest, ConfigureMultipleOutputs) {
  std::vector<camera3_stream_t> streams;
  // 2 outputs, of different sizes.
  streams.push_back(MakeStream(bidirectional_raw_));
  // Use the alternate size.
  streams.push_back(MakeStream(output_multisize_non_stalling_,
                               true,
                               false,
                               kAlternateWidth,
                               kAlternateHeight));
  ExpectConfigurationSupported(streams, true);
}

TEST_F(StaticPropertiesTest, ConfigureInput) {
  std::vector<camera3_stream_t> streams;
  // Single input -> different output.
  streams.push_back(MakeStream(input_, false, true));
  // Use the alternate size, it should be ok.
  streams.push_back(MakeStream(output_multisize_non_stalling_,
                               true,
                               false,
                               kAlternateWidth,
                               kAlternateHeight));
  ExpectConfigurationSupported(streams, true);
}

TEST_F(StaticPropertiesTest, ConfigureBidirectional) {
  std::vector<camera3_stream_t> streams;
  // Single input -> same output.
  streams.push_back(
      MakeStream(bidirectional_self_supporting_stalling_, true, true));
  ExpectConfigurationSupported(streams, true);
}

TEST_F(StaticPropertiesTest, ConfigureMultipleReprocess) {
  std::vector<camera3_stream_t> streams;
  // Single input -> multiple outputs.
  streams.push_back(
      MakeStream(bidirectional_self_supporting_stalling_, true, true));
  streams.push_back(MakeStream(output_multisize_non_stalling_));
  ExpectConfigurationSupported(streams, true);
}

TEST_F(StaticPropertiesTest, ConfigureNull) {
  PrepareDefaultDUT();
  EXPECT_FALSE(dut_->StreamConfigurationSupported(nullptr));
}

TEST_F(StaticPropertiesTest, ConfigureEmptyStreams) {
  std::vector<camera3_stream_t*> streams(1);
  camera3_stream_configuration_t config = {
      0, streams.data(), CAMERA3_STREAM_CONFIGURATION_NORMAL_MODE, nullptr};
  PrepareDefaultDUT();
  EXPECT_FALSE(dut_->StreamConfigurationSupported(&config));
}

TEST_F(StaticPropertiesTest, ConfigureNullStreams) {
  std::vector<camera3_stream_t*> streams(2, nullptr);
  camera3_stream_configuration_t config = {
      static_cast<uint32_t>(streams.size()),
      streams.data(),
      CAMERA3_STREAM_CONFIGURATION_NORMAL_MODE,
      nullptr};
  PrepareDefaultDUT();
  EXPECT_FALSE(dut_->StreamConfigurationSupported(&config));
}

TEST_F(StaticPropertiesTest, ConfigureNullStreamVector) {
  // Even if the camera claims to have multiple streams, check for null.
  camera3_stream_configuration_t config = {
      3, nullptr, CAMERA3_STREAM_CONFIGURATION_NORMAL_MODE, nullptr};
  PrepareDefaultDUT();
  EXPECT_FALSE(dut_->StreamConfigurationSupported(&config));
}

TEST_F(StaticPropertiesTest, ConfigureNoOutput) {
  std::vector<camera3_stream_t> streams;
  // Only an input stream, no output.
  streams.push_back(MakeStream(input_, false, true));
  ExpectConfigurationSupported(streams, false);
}

TEST_F(StaticPropertiesTest, ConfigureInvalidType) {
  std::vector<camera3_stream_t> streams;
  // Not input, output, or bidirectional.
  streams.push_back(MakeStream(output_multisize_non_stalling_, false, false));
  ExpectConfigurationSupported(streams, false);
}

TEST_F(StaticPropertiesTest, ConfigureSpecFormatDoesNotExist) {
  std::vector<camera3_stream_t> streams;
  // Format 99 is not supported in any form.
  streams.push_back(MakeStream(99));
  ExpectConfigurationSupported(streams, false);
}

TEST_F(StaticPropertiesTest, ConfigureSpecSizeDoesNotExist) {
  std::vector<camera3_stream_t> streams;
  // Size 99 x 99 not supported for the output format.
  streams.push_back(
      MakeStream(output_multisize_non_stalling_, true, false, 99, 99));
  ExpectConfigurationSupported(streams, false);
}

TEST_F(StaticPropertiesTest, ConfigureNotAnInput) {
  std::vector<camera3_stream_t> streams;
  streams.push_back(MakeStream(output_multisize_non_stalling_));
  // Can't use output-only format as an input.
  streams.push_back(MakeStream(output_multisize_non_stalling_, false, true));
  ExpectConfigurationSupported(streams, false);
}

TEST_F(StaticPropertiesTest, ConfigureNotAnOutput) {
  std::vector<camera3_stream_t> streams;
  // Can't use input-only format as an output.
  streams.push_back(MakeStream(input_));
  ExpectConfigurationSupported(streams, false);
}

TEST_F(StaticPropertiesTest, ConfigureTooManyInputs) {
  std::vector<camera3_stream_t> streams;
  // At the threshold is ok.
  for (int32_t i = 0; i < test_max_inputs_; ++i) {
    streams.push_back(MakeStream(input_, false, true));
  }
  // Have a valid output still.
  streams.push_back(MakeStream(output_multisize_non_stalling_));
  ExpectConfigurationSupported(streams, false);

  // Reset.
  mock_reader_ = std::make_unique<MetadataReaderMock>();
  streams.clear();

  // Try again with too many.
  for (int32_t i = 0; i <= test_max_inputs_; ++i) {
    streams.push_back(MakeStream(input_, false, true));
  }
  // Have a valid output still.
  streams.push_back(MakeStream(output_multisize_non_stalling_));
  ExpectConfigurationSupported(streams, false);
}

TEST_F(StaticPropertiesTest, ConfigureTooManyRaw) {
  std::vector<camera3_stream_t> streams;
  // At the threshold is ok.
  for (int32_t i = 0; i < test_max_raw_outputs_; ++i) {
    streams.push_back(MakeStream(bidirectional_raw_));
  }
  ExpectConfigurationSupported(streams, true);

  // Reset.
  mock_reader_ = std::make_unique<MetadataReaderMock>();
  streams.clear();

  // Try again with too many.
  for (int32_t i = 0; i <= test_max_raw_outputs_; ++i) {
    streams.push_back(MakeStream(bidirectional_raw_));
  }
  ExpectConfigurationSupported(streams, false);
}

TEST_F(StaticPropertiesTest, ConfigureTooManyStalling) {
  std::vector<camera3_stream_t> streams;
  // At the threshold is ok.
  for (int32_t i = 0; i < test_max_stalling_outputs_; ++i) {
    streams.push_back(MakeStream(bidirectional_self_supporting_stalling_));
  }
  ExpectConfigurationSupported(streams, true);

  // Reset.
  mock_reader_ = std::make_unique<MetadataReaderMock>();
  streams.clear();

  // Try again with too many.
  for (int32_t i = 0; i <= test_max_stalling_outputs_; ++i) {
    streams.push_back(MakeStream(bidirectional_self_supporting_stalling_));
  }
  ExpectConfigurationSupported(streams, false);
}

TEST_F(StaticPropertiesTest, ConfigureTooManyNonStalling) {
  std::vector<camera3_stream_t> streams;
  // At the threshold is ok.
  for (int32_t i = 0; i < test_max_non_stalling_outputs_; ++i) {
    streams.push_back(MakeStream(output_multisize_non_stalling_));
  }
  ExpectConfigurationSupported(streams, true);

  // Reset.
  mock_reader_ = std::make_unique<MetadataReaderMock>();
  streams.clear();

  // Try again with too many.
  for (int32_t i = 0; i <= test_max_non_stalling_outputs_; ++i) {
    streams.push_back(MakeStream(output_multisize_non_stalling_));
  }
  ExpectConfigurationSupported(streams, false);
}

TEST_F(StaticPropertiesTest, ConfigureUnuspportedInput) {
  std::vector<camera3_stream_t> streams;
  streams.push_back(MakeStream(input_, false, true));
  streams.push_back(MakeStream(bidirectional_raw_));
  // No matching output format for input.
  ExpectConfigurationSupported(streams, false);
}

TEST_F(StaticPropertiesTest, ConfigureUnsupportedOutput) {
  std::vector<camera3_stream_t> streams;
  streams.push_back(MakeStream(input_, false, true));
  // The universal output does match input.
  streams.push_back(MakeStream(output_multisize_non_stalling_));
  // Raw does not match input.
  streams.push_back(MakeStream(bidirectional_raw_));
  // Input is matched; it's ok that raw doesn't match (only the actual
  // requests care).
  ExpectConfigurationSupported(streams, true);
}

TEST_F(StaticPropertiesTest, ConfigureUnsupportedBidirectional) {
  std::vector<camera3_stream_t> streams;
  // The test raw format, while supporting both input and output,
  // does not actually support itself.
  streams.push_back(MakeStream(bidirectional_raw_, true, true));
  ExpectConfigurationSupported(streams, false);
}

TEST_F(StaticPropertiesTest, ConfigureBadOperationMode) {
  // A valid stream set.
  camera3_stream_t stream = MakeStream(output_multisize_non_stalling_);
  camera3_stream_t* stream_address = &stream;
  // But not a valid config.
  camera3_stream_configuration_t config = {
      1,
      &stream_address,
      99, // Not a valid operation mode.
      nullptr
  };
  PrepareDefaultDUT();
  EXPECT_FALSE(dut_->StreamConfigurationSupported(&config));
}

TEST_F(StaticPropertiesTest, ReprocessingSingleOutput) {
  camera3_stream_t input_stream = MakeStream(input_);
  camera3_stream_t output_stream = MakeStream(output_multisize_non_stalling_);
  PrepareDefaultDUT();
  EXPECT_TRUE(dut_->ReprocessingSupported(&input_stream, {&output_stream}));
}

TEST_F(StaticPropertiesTest, ReprocessingMultipleOutputs) {
  camera3_stream_t input_stream =
      MakeStream(bidirectional_self_supporting_stalling_, false, true);
  // Bi-directional self-supporting supports the universal output and itself.
  camera3_stream_t output_stream1 = MakeStream(output_multisize_non_stalling_);
  camera3_stream_t output_stream2 =
      MakeStream(bidirectional_self_supporting_stalling_);
  PrepareDefaultDUT();
  EXPECT_TRUE(dut_->ReprocessingSupported(&input_stream,
                                          {&output_stream1, &output_stream2}));
}

TEST_F(StaticPropertiesTest, ReprocessingNoInput) {
  camera3_stream_t output_stream = MakeStream(output_multisize_non_stalling_);
  PrepareDefaultDUT();
  EXPECT_FALSE(dut_->ReprocessingSupported(nullptr, {&output_stream}));
}

TEST_F(StaticPropertiesTest, ReprocessingNoOutput) {
  camera3_stream_t input_stream = MakeStream(input_);
  PrepareDefaultDUT();
  EXPECT_FALSE(dut_->ReprocessingSupported(&input_stream, {}));
}

TEST_F(StaticPropertiesTest, ReprocessingInvalidOutput) {
  camera3_stream_t input_stream = MakeStream(input_, false, true);
  // The universal output does match input.
  camera3_stream_t output_stream1 = MakeStream(output_multisize_non_stalling_);
  // Raw does not match input.
  camera3_stream_t output_stream2 = MakeStream(bidirectional_raw_);
  PrepareDefaultDUT();
  EXPECT_FALSE(dut_->ReprocessingSupported(&input_stream,
                                           {&output_stream1, &output_stream2}));
}

}  // namespace default_camera_hal
