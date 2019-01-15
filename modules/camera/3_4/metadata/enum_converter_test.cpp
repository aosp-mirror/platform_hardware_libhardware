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

#include "enum_converter.h"

#include <gtest/gtest.h>

using testing::Test;

namespace v4l2_camera_hal {

class EnumConverterTest : public Test {
 protected:
  virtual void SetUp() {
    converter_.reset(
        new EnumConverter({{one_to_one_v4l2_, one_to_one_metadata_},
                           {one_to_many_v4l2_, many_to_one_metadata_1_},
                           {one_to_many_v4l2_, many_to_one_metadata_2_},
                           {many_to_one_v4l2_1_, one_to_many_metadata_},
                           {many_to_one_v4l2_2_, one_to_many_metadata_},
                           {unused_v4l2_, unused_metadata_}}));
  }

  std::unique_ptr<EnumConverter> converter_;

  const int32_t one_to_one_v4l2_ = 12;
  const int32_t one_to_many_v4l2_ = 34;
  const int32_t many_to_one_v4l2_1_ = 56;
  const int32_t many_to_one_v4l2_2_ = 78;
  const int32_t unused_v4l2_ = 910;
  const uint8_t one_to_one_metadata_ = 109;
  const uint8_t one_to_many_metadata_ = 87;
  const uint8_t many_to_one_metadata_1_ = 65;
  const uint8_t many_to_one_metadata_2_ = 43;
  const uint8_t unused_metadata_ = 21;
};

// Convert single.
TEST_F(EnumConverterTest, OneToOneConversion) {
  uint8_t metadata_val = 1;
  ASSERT_EQ(converter_->V4L2ToMetadata(one_to_one_v4l2_, &metadata_val), 0);
  EXPECT_EQ(metadata_val, one_to_one_metadata_);

  int32_t v4l2_val = 1;
  ASSERT_EQ(converter_->MetadataToV4L2(one_to_one_metadata_, &v4l2_val), 0);
  EXPECT_EQ(v4l2_val, one_to_one_v4l2_);
}

TEST_F(EnumConverterTest, OneToManyConversion) {
  // Should be one of the acceptable values.
  uint8_t metadata_val = 1;
  ASSERT_EQ(converter_->V4L2ToMetadata(one_to_many_v4l2_, &metadata_val), 0);
  EXPECT_TRUE(metadata_val == many_to_one_metadata_1_ ||
              metadata_val == many_to_one_metadata_2_);

  int32_t v4l2_val = 1;
  ASSERT_EQ(converter_->MetadataToV4L2(one_to_many_metadata_, &v4l2_val), 0);
  EXPECT_TRUE(v4l2_val == many_to_one_v4l2_1_ ||
              v4l2_val == many_to_one_v4l2_2_);
}

TEST_F(EnumConverterTest, ManyToOneConversion) {
  uint8_t metadata_val = 1;
  ASSERT_EQ(converter_->V4L2ToMetadata(many_to_one_v4l2_1_, &metadata_val), 0);
  EXPECT_EQ(metadata_val, one_to_many_metadata_);
  metadata_val = 1;  // Reset.
  ASSERT_EQ(converter_->V4L2ToMetadata(many_to_one_v4l2_2_, &metadata_val), 0);
  EXPECT_EQ(metadata_val, one_to_many_metadata_);

  int32_t v4l2_val = 1;
  ASSERT_EQ(converter_->MetadataToV4L2(many_to_one_metadata_1_, &v4l2_val), 0);
  EXPECT_EQ(v4l2_val, one_to_many_v4l2_);
  v4l2_val = 1;  // Reset.
  ASSERT_EQ(converter_->MetadataToV4L2(many_to_one_metadata_2_, &v4l2_val), 0);
  EXPECT_EQ(v4l2_val, one_to_many_v4l2_);
}

TEST_F(EnumConverterTest, InvalidConversion) {
  uint8_t metadata_val = 1;
  EXPECT_EQ(converter_->V4L2ToMetadata(1, &metadata_val), -EINVAL);

  int32_t v4l2_val = 1;
  EXPECT_EQ(converter_->MetadataToV4L2(1, &v4l2_val), -EINVAL);
}

}  // namespace v4l2_camera_hal
