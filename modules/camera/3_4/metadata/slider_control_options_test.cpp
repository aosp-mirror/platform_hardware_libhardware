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

#include "slider_control_options.h"

#include <memory>

#include <gtest/gtest.h>
#include <hardware/camera3.h>

using testing::Test;

namespace v4l2_camera_hal {

class SliderControlOptionsTest : public Test {
 protected:
  virtual void SetUp() {
    dut_.reset(new SliderControlOptions<int>(min_, max_));
  }

  std::unique_ptr<SliderControlOptions<int>> dut_;
  const int min_ = 1;
  const int max_ = 10;
};

TEST_F(SliderControlOptionsTest, MetadataRepresentation) {
  // Technically order doesn't matter, but this is faster to write,
  // and still passes.
  std::vector<int> expected{min_, max_};
  EXPECT_EQ(dut_->MetadataRepresentation(), expected);
}

TEST_F(SliderControlOptionsTest, IsSupported) {
  for (int i = min_; i <= max_; ++i) {
    EXPECT_TRUE(dut_->IsSupported(i));
  }
  // Out of range unsupported.
  EXPECT_FALSE(dut_->IsSupported(min_ - 1));
  EXPECT_FALSE(dut_->IsSupported(max_ + 1));
}

TEST_F(SliderControlOptionsTest, DefaultValue) {
  // All default values should be supported.
  // For some reason, the templates have values in the range [1, COUNT).
  for (int i = 1; i < CAMERA3_TEMPLATE_COUNT; ++i) {
    int value = -1;
    EXPECT_EQ(dut_->DefaultValueForTemplate(i, &value), 0);
    EXPECT_TRUE(dut_->IsSupported(value));
  }
}

TEST_F(SliderControlOptionsTest, NoDefaultValue) {
  // Invalid options don't have a valid default.
  SliderControlOptions<int> bad_options(10, 9);  // min > max.
  for (int i = 1; i < CAMERA3_TEMPLATE_COUNT; ++i) {
    int value = -1;
    EXPECT_EQ(bad_options.DefaultValueForTemplate(i, &value), -ENODEV);
  }
}

}  // namespace v4l2_camera_hal
