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

#include "menu_control_options.h"

#include <memory>

#include <gtest/gtest.h>

using testing::Test;

namespace v4l2_camera_hal {

class MenuControlOptionsTest : public Test {
 protected:
  virtual void SetUp() { dut_.reset(new MenuControlOptions<int>(options_)); }

  std::unique_ptr<MenuControlOptions<int>> dut_;
  const std::vector<int> options_{1, 10, 19, 30};
};

TEST_F(MenuControlOptionsTest, MetadataRepresentation) {
  // Technically order doesn't matter, but this is faster to write,
  // and still passes.
  EXPECT_EQ(dut_->MetadataRepresentation(), options_);
}

TEST_F(MenuControlOptionsTest, IsSupported) {
  for (auto option : options_) {
    EXPECT_TRUE(dut_->IsSupported(option));
  }
  // And at least one unsupported.
  EXPECT_FALSE(dut_->IsSupported(99));
}

}  // namespace v4l2_camera_hal
