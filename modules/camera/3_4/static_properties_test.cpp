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
  void SetUp() {
    // Ensure tests will probably fail if PrepareDUT isn't called.
    dut_.reset();
    mock_reader_ = std::make_unique<MetadataReaderMock>();
  }

  void PrepareDUT() {
    dut_.reset(StaticProperties::NewStaticProperties(std::move(mock_reader_)));
  }

  void SetDefaultExpectations() {
    EXPECT_CALL(*mock_reader_, Facing(_))
        .Times(AtMost(1))
        .WillOnce(DoAll(SetArgPointee<0>(test_facing_), Return(0)));
    EXPECT_CALL(*mock_reader_, Orientation(_))
        .Times(AtMost(1))
        .WillOnce(DoAll(SetArgPointee<0>(test_orientation_), Return(0)));
  }

  std::unique_ptr<StaticProperties> dut_;
  std::unique_ptr<MetadataReaderMock> mock_reader_;

  const int test_facing_ = CAMERA_FACING_FRONT;
  const int test_orientation_ = 90;
};

TEST_F(StaticPropertiesTest, FactorySuccess) {
  SetDefaultExpectations();
  PrepareDUT();
  ASSERT_NE(dut_, nullptr);
  EXPECT_EQ(dut_->facing(), test_facing_);
  EXPECT_EQ(dut_->orientation(), test_orientation_);
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

}  // namespace default_camera_hal
