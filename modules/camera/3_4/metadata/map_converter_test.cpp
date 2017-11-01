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

#include "map_converter.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "converter_interface_mock.h"

using testing::Return;
using testing::SetArgPointee;
using testing::Test;
using testing::_;

namespace v4l2_camera_hal {

class MapConverterTest : public Test {
 protected:
  virtual void SetUp() {
    converter_.reset(new ConverterInterfaceMock<int, int32_t>());
    dut_.reset(new MapConverter<int, int32_t, int32_t>(converter_, map_));
  }

  virtual void ExpectConvertToV4L2(int32_t converted, int32_t expected) {
    int initial = 99;
    EXPECT_CALL(*converter_, MetadataToV4L2(initial, _))
        .WillOnce(DoAll(SetArgPointee<1>(converted), Return(0)));

    int32_t actual = expected + 1;  // Initialize to non-expected value.
    ASSERT_EQ(dut_->MetadataToV4L2(initial, &actual), 0);
    EXPECT_EQ(actual, expected);
  }

  std::shared_ptr<ConverterInterfaceMock<int, int32_t>> converter_;
  std::unique_ptr<MapConverter<int, int32_t, int32_t>> dut_;

  const std::map<int32_t, int32_t> map_{{10, 1}, {40, 4}, {20, 2}, {30, 3}};
};

TEST_F(MapConverterTest, NormalConversionToV4L2) {
  // A value that matches the map perfectly.
  auto kv = map_.begin();
  ExpectConvertToV4L2(kv->first, kv->second);
}

TEST_F(MapConverterTest, RoundingDownConversionToV4L2) {
  // A value that's in range but not an exact key value.
  auto kv = map_.begin();
  ExpectConvertToV4L2(kv->first + 1, kv->second);
}

TEST_F(MapConverterTest, RoundingUpConversionToV4L2) {
  // A value that's in range but not an exact key value.
  auto kv = map_.begin();
  ++kv;
  ExpectConvertToV4L2(kv->first - 1, kv->second);
}

TEST_F(MapConverterTest, ClampUpConversionToV4L2) {
  // A value that's below range.
  auto kv = map_.begin();
  ExpectConvertToV4L2(kv->first - 1, kv->second);
}

TEST_F(MapConverterTest, ClampDownConversionToV4L2) {
  // A value that's above range (even after fitting to step).
  auto kv = map_.rbegin();
  ExpectConvertToV4L2(kv->first + 1, kv->second);
}

TEST_F(MapConverterTest, ConversionErrorToV4L2) {
  int initial = 99;
  int err = -99;
  EXPECT_CALL(*converter_, MetadataToV4L2(initial, _)).WillOnce(Return(err));

  int32_t unused;
  EXPECT_EQ(dut_->MetadataToV4L2(initial, &unused), err);
}

TEST_F(MapConverterTest, NormalConversionToMetadata) {
  auto kv = map_.begin();
  int expected = 99;
  EXPECT_CALL(*converter_, V4L2ToMetadata(kv->first, _))
      .WillOnce(DoAll(SetArgPointee<1>(expected), Return(0)));

  int actual = expected + 1;  // Initialize to non-expected value.
  ASSERT_EQ(dut_->V4L2ToMetadata(kv->second, &actual), 0);
  EXPECT_EQ(actual, expected);
}

TEST_F(MapConverterTest, NotFoundConversionToMetadata) {
  int unused;
  ASSERT_EQ(dut_->V4L2ToMetadata(100, &unused), -EINVAL);
}

}  // namespace v4l2_camera_hal
