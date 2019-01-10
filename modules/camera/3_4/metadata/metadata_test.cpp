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

#include "metadata.h"

#include <memory>
#include <set>
#include <vector>

#include <camera/CameraMetadata.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "metadata_common.h"
#include "partial_metadata_interface_mock.h"

using testing::AtMost;
using testing::Return;
using testing::Test;
using testing::_;

namespace v4l2_camera_hal {

class MetadataTest : public Test {
 protected:
  virtual void SetUp() {
    // Clear the DUT. AddComponents must be called before using it.
    dut_.reset();

    component1_.reset(new PartialMetadataInterfaceMock());
    component2_.reset(new PartialMetadataInterfaceMock());
    metadata_.reset(new android::CameraMetadata());
    non_empty_metadata_.reset(new android::CameraMetadata());
    uint8_t val = 1;
    non_empty_metadata_->update(ANDROID_COLOR_CORRECTION_MODE, &val, 1);
  }

  // Once the component mocks have had expectations set,
  // add them to the device under test.
  virtual void AddComponents() {
    // Don't mind moving; Gmock/Gtest fails on leaked mocks unless disabled by
    // runtime flags.
    PartialMetadataSet components;
    components.insert(std::move(component1_));
    components.insert(std::move(component2_));
    dut_.reset(new Metadata(std::move(components)));
  }

  virtual void CompareTags(const std::set<int32_t>& expected,
                           const camera_metadata_entry_t& actual) {
    ASSERT_EQ(expected.size(), actual.count);
    for (size_t i = 0; i < actual.count; ++i) {
      EXPECT_NE(expected.find(actual.data.i32[i]), expected.end());
    }
  }

  // Device under test.
  std::unique_ptr<Metadata> dut_;
  // Mocks.
  std::unique_ptr<PartialMetadataInterfaceMock> component1_;
  std::unique_ptr<PartialMetadataInterfaceMock> component2_;
  // Metadata.
  std::unique_ptr<android::CameraMetadata> metadata_;
  std::unique_ptr<android::CameraMetadata> non_empty_metadata_;
  // An empty vector to use as necessary.
  std::vector<int32_t> empty_tags_;
};

TEST_F(MetadataTest, FillStaticSuccess) {
  // Should populate all the component static pieces.
  EXPECT_CALL(*component1_, PopulateStaticFields(_)).WillOnce(Return(0));
  EXPECT_CALL(*component2_, PopulateStaticFields(_)).WillOnce(Return(0));

  // Should populate the meta keys, by polling each component's keys.
  std::vector<int32_t> static_tags_1({1, 2});
  std::vector<int32_t> static_tags_2({3, 4});
  std::vector<int32_t> control_tags_1({5, 6});
  std::vector<int32_t> control_tags_2({7, 8});
  std::vector<int32_t> dynamic_tags_1({9, 10});
  std::vector<int32_t> dynamic_tags_2({11, 12});
  EXPECT_CALL(*component1_, StaticTags()).WillOnce(Return(static_tags_1));
  EXPECT_CALL(*component1_, ControlTags()).WillOnce(Return(control_tags_1));
  EXPECT_CALL(*component1_, DynamicTags()).WillOnce(Return(dynamic_tags_1));
  EXPECT_CALL(*component2_, StaticTags()).WillOnce(Return(static_tags_2));
  EXPECT_CALL(*component2_, ControlTags()).WillOnce(Return(control_tags_2));
  EXPECT_CALL(*component2_, DynamicTags()).WillOnce(Return(dynamic_tags_2));

  AddComponents();
  // Should succeed. If it didn't, no reason to continue checking output.
  ASSERT_EQ(dut_->FillStaticMetadata(metadata_.get()), 0);

  // Meta keys should be filled correctly.
  // Note: sets are used here, but it is undefined behavior if
  // the class has multiple componenets reporting overlapping tags.

  // Get the expected tags = combined tags of all components.
  std::set<int32_t> static_tags(static_tags_1.begin(), static_tags_1.end());
  static_tags.insert(static_tags_2.begin(), static_tags_2.end());
  std::set<int32_t> control_tags(control_tags_1.begin(), control_tags_1.end());
  control_tags.insert(control_tags_2.begin(), control_tags_2.end());
  std::set<int32_t> dynamic_tags(dynamic_tags_1.begin(), dynamic_tags_1.end());
  dynamic_tags.insert(dynamic_tags_2.begin(), dynamic_tags_2.end());

  // Static tags includes not only all component static tags, but also
  // the meta AVAILABLE_*_KEYS (* = [REQUEST, RESULT, CHARACTERISTICS]).
  static_tags.emplace(ANDROID_REQUEST_AVAILABLE_REQUEST_KEYS);
  static_tags.emplace(ANDROID_REQUEST_AVAILABLE_RESULT_KEYS);
  static_tags.emplace(ANDROID_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS);

  // Check against what was filled in in the metadata.
  CompareTags(static_tags,
              metadata_->find(ANDROID_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS));
  CompareTags(control_tags,
              metadata_->find(ANDROID_REQUEST_AVAILABLE_REQUEST_KEYS));
  CompareTags(dynamic_tags,
              metadata_->find(ANDROID_REQUEST_AVAILABLE_RESULT_KEYS));
}

TEST_F(MetadataTest, FillStaticFail) {
  int err = -99;
  // Order undefined, and may or may not exit early; use AtMost.
  EXPECT_CALL(*component1_, PopulateStaticFields(_))
      .Times(AtMost(1))
      .WillOnce(Return(0));
  EXPECT_CALL(*component2_, PopulateStaticFields(_)).WillOnce(Return(err));

  // May or may not exit early, may still try to populate meta tags.
  EXPECT_CALL(*component1_, StaticTags())
      .Times(AtMost(1))
      .WillOnce(Return(empty_tags_));
  EXPECT_CALL(*component1_, ControlTags())
      .Times(AtMost(1))
      .WillOnce(Return(empty_tags_));
  EXPECT_CALL(*component1_, DynamicTags())
      .Times(AtMost(1))
      .WillOnce(Return(empty_tags_));
  EXPECT_CALL(*component2_, StaticTags())
      .Times(AtMost(1))
      .WillOnce(Return(empty_tags_));
  EXPECT_CALL(*component2_, ControlTags())
      .Times(AtMost(1))
      .WillOnce(Return(empty_tags_));
  EXPECT_CALL(*component2_, DynamicTags())
      .Times(AtMost(1))
      .WillOnce(Return(empty_tags_));

  AddComponents();
  // If any component errors, error should be returned
  EXPECT_EQ(dut_->FillStaticMetadata(metadata_.get()), err);
}

TEST_F(MetadataTest, FillStaticNull) {
  AddComponents();
  EXPECT_EQ(dut_->FillStaticMetadata(nullptr), -EINVAL);
}

TEST_F(MetadataTest, IsValidSuccess) {
  // Should check if all the component request values are valid.
  EXPECT_CALL(*component1_, SupportsRequestValues(_)).WillOnce(Return(true));
  EXPECT_CALL(*component2_, SupportsRequestValues(_)).WillOnce(Return(true));

  AddComponents();
  // Should succeed.
  // Note: getAndLock is a lock against pointer invalidation, not concurrency,
  // and unlocks on object destruction.
  EXPECT_TRUE(dut_->IsValidRequest(*non_empty_metadata_));
}

TEST_F(MetadataTest, IsValidFail) {
  // Should check if all the component request values are valid.
  // Order undefined, and may or may not exit early; use AtMost.
  EXPECT_CALL(*component1_, SupportsRequestValues(_))
      .Times(AtMost(1))
      .WillOnce(Return(true));
  EXPECT_CALL(*component2_, SupportsRequestValues(_)).WillOnce(Return(false));

  AddComponents();
  // Should fail since one of the components failed.
  // Note: getAndLock is a lock against pointer invalidation, not concurrency,
  // and unlocks on object destruction.
  EXPECT_FALSE(dut_->IsValidRequest(*non_empty_metadata_));
}

TEST_F(MetadataTest, IsValidEmpty) {
  // Setting null settings is a special case indicating to use the
  // previous (valid) settings. As such it is inherently valid.
  // Should not try to check any components.
  EXPECT_CALL(*component1_, SupportsRequestValues(_)).Times(0);
  EXPECT_CALL(*component2_, SupportsRequestValues(_)).Times(0);

  AddComponents();
  EXPECT_TRUE(dut_->IsValidRequest(*metadata_));
}

TEST_F(MetadataTest, GetTemplateSuccess) {
  int template_type = 3;

  // Should check if all the components fill the template successfully.
  EXPECT_CALL(*component1_, PopulateTemplateRequest(template_type, _))
      .WillOnce(Return(0));
  EXPECT_CALL(*component2_, PopulateTemplateRequest(template_type, _))
      .WillOnce(Return(0));

  AddComponents();
  // Should succeed.
  EXPECT_EQ(dut_->GetRequestTemplate(template_type, metadata_.get()), 0);
}

TEST_F(MetadataTest, GetTemplateFail) {
  int err = -99;
  int template_type = 3;

  // Should check if all the components fill the template successfully.
  // Order undefined, and may or may not exit early; use AtMost.
  EXPECT_CALL(*component1_, PopulateTemplateRequest(template_type, _))
      .Times(AtMost(1))
      .WillOnce(Return(0));
  EXPECT_CALL(*component2_, PopulateTemplateRequest(template_type, _))
      .WillOnce(Return(err));

  AddComponents();
  // Should fail since one of the components failed.
  EXPECT_EQ(dut_->GetRequestTemplate(template_type, metadata_.get()), err);
}

TEST_F(MetadataTest, GetTemplateNull) {
  AddComponents();
  EXPECT_EQ(dut_->GetRequestTemplate(1, nullptr), -EINVAL);
}

TEST_F(MetadataTest, GetTemplateInvalid) {
  int template_type = 99;  // Invalid template type.

  AddComponents();
  // Should fail fast since template type is invalid.
  EXPECT_EQ(dut_->GetRequestTemplate(template_type, metadata_.get()), -EINVAL);
}

TEST_F(MetadataTest, SetSettingsSuccess) {
  // Should check if all the components set successfully.
  EXPECT_CALL(*component1_, SetRequestValues(_)).WillOnce(Return(0));
  EXPECT_CALL(*component2_, SetRequestValues(_)).WillOnce(Return(0));

  AddComponents();
  // Should succeed.
  // Note: getAndLock is a lock against pointer invalidation, not concurrency,
  // and unlocks on object destruction.
  EXPECT_EQ(dut_->SetRequestSettings(*non_empty_metadata_), 0);
}

TEST_F(MetadataTest, SetSettingsFail) {
  int err = -99;

  // Should check if all the components set successfully.
  // Order undefined, and may or may not exit early; use AtMost.
  EXPECT_CALL(*component1_, SetRequestValues(_))
      .Times(AtMost(1))
      .WillOnce(Return(0));
  EXPECT_CALL(*component2_, SetRequestValues(_)).WillOnce(Return(err));

  AddComponents();
  // Should fail since one of the components failed.
  // Note: getAndLock is a lock against pointer invalidation, not concurrency,
  // and unlocks on object destruction.
  EXPECT_EQ(dut_->SetRequestSettings(*non_empty_metadata_), err);
}

TEST_F(MetadataTest, SetSettingsEmpty) {
  // Setting null settings is a special case indicating to use the
  // previous settings. Should not try to set any components.
  EXPECT_CALL(*component1_, SetRequestValues(_)).Times(0);
  EXPECT_CALL(*component2_, SetRequestValues(_)).Times(0);

  AddComponents();
  // Should succeed.
  EXPECT_EQ(dut_->SetRequestSettings(*metadata_), 0);
}

TEST_F(MetadataTest, FillResultSuccess) {
  // Should check if all the components fill results successfully.
  EXPECT_CALL(*component1_, PopulateDynamicFields(_)).WillOnce(Return(0));
  EXPECT_CALL(*component2_, PopulateDynamicFields(_)).WillOnce(Return(0));

  AddComponents();
  // Should succeed.
  EXPECT_EQ(dut_->FillResultMetadata(metadata_.get()), 0);
}

TEST_F(MetadataTest, FillResultFail) {
  int err = -99;

  // Should check if all the components fill results successfully.
  // Order undefined, and may or may not exit early; use AtMost.
  EXPECT_CALL(*component1_, PopulateDynamicFields(_))
      .Times(AtMost(1))
      .WillOnce(Return(0));
  EXPECT_CALL(*component2_, PopulateDynamicFields(_)).WillOnce(Return(err));

  AddComponents();
  // Should fail since one of the components failed.
  EXPECT_EQ(dut_->FillResultMetadata(metadata_.get()), err);
}

TEST_F(MetadataTest, FillResultNull) {
  AddComponents();
  EXPECT_EQ(dut_->FillResultMetadata(nullptr), -EINVAL);
}

}  // namespace v4l2_camera_hal
