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

#include "ranged_converter.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "converter_interface_mock.h"

using testing::Return;
using testing::SetArgPointee;
using testing::Test;
using testing::_;

namespace v4l2_camera_hal {

class RangedConverterTest : public Test {
 protected:
  virtual void SetUp() {
    converter_.reset(new ConverterInterfaceMock<int, int32_t>());
    dut_.reset(
        new RangedConverter<int, int32_t>(converter_, min_, max_, step_));
  }

  virtual void ExpectConvert(int32_t converted, int32_t expected) {
    int initial = 99;
    EXPECT_CALL(*converter_, MetadataToV4L2(initial, _))
        .WillOnce(DoAll(SetArgPointee<1>(converted), Return(0)));

    int32_t actual = expected + 1;  // Initialize to non-expected value.
    ASSERT_EQ(dut_->MetadataToV4L2(initial, &actual), 0);
    EXPECT_EQ(actual, expected);
  }

  std::shared_ptr<ConverterInterfaceMock<int, int32_t>> converter_;
  std::unique_ptr<RangedConverter<int, int32_t>> dut_;

  const int32_t min_ = -11;
  const int32_t max_ = 10;
  const int32_t step_ = 3;
};

TEST_F(RangedConverterTest, NormalConversion) {
  // A value that's in range and on step.
  ExpectConvert(max_ - step_, max_ - step_);
}

TEST_F(RangedConverterTest, RoundingConversion) {
  // A value that's in range but off step.
  ExpectConvert(max_ - step_ + 1, max_ - step_);
}

TEST_F(RangedConverterTest, ClampUpConversion) {
  // A value that's below range.
  ExpectConvert(min_ - 1, min_);
}

TEST_F(RangedConverterTest, ClampDownConversion) {
  // A value that's above range (even after fitting to step).
  ExpectConvert(max_ + step_, max_);
}

TEST_F(RangedConverterTest, ConversionError) {
  int initial = 99;
  int err = -99;
  EXPECT_CALL(*converter_, MetadataToV4L2(initial, _)).WillOnce(Return(err));

  int32_t unused;
  EXPECT_EQ(dut_->MetadataToV4L2(initial, &unused), err);
}

}  // namespace v4l2_camera_hal
