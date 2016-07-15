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

#include "v4l2_metadata.h"

#include <memory>
#include <set>
#include <vector>

#include <camera/CameraMetadata.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "metadata/partial_metadata_interface_mock.h"
#include "v4l2_wrapper_mock.h"

using testing::AtMost;
using testing::Return;
using testing::ReturnRef;
using testing::Test;
using testing::_;

namespace v4l2_camera_hal {

class V4L2MetadataTest : public Test {
 protected:
  // Create a test version of V4L2Metadata with no default components.
  class TestV4L2Metadata : public V4L2Metadata {
   public:
    TestV4L2Metadata(V4L2Wrapper* device) : V4L2Metadata(device){};

   private:
    void PopulateComponents() override{};
  };

  virtual void SetUp() {
    device_.reset(new V4L2WrapperMock());
    dut_.reset(new TestV4L2Metadata(device_.get()));

    component1_.reset(new PartialMetadataInterfaceMock);
    component2_.reset(new PartialMetadataInterfaceMock);
  }

  // Once the component mocks have had expectations set,
  // add them to the device under test.
  virtual void AddComponents() {
    // Don't mind moving; Gmock/Gtest fails on leaked mocks unless disabled by
    // runtime flags.
    dut_->AddComponent(std::move(component1_));
    dut_->AddComponent(std::move(component2_));
  }

  virtual void CompareTags(const std::set<int32_t>& expected,
                           const camera_metadata_entry_t& actual) {
    EXPECT_EQ(expected.size(), actual.count);
    for (size_t i = 0; i < actual.count; ++i) {
      EXPECT_NE(expected.find(actual.data.i32[i]), expected.end());
    }
  }

  // Device under test.
  std::unique_ptr<V4L2Metadata> dut_;
  // Mocks.
  std::unique_ptr<V4L2WrapperMock> device_;
  std::unique_ptr<PartialMetadataInterfaceMock> component1_;
  std::unique_ptr<PartialMetadataInterfaceMock> component2_;
  // An empty vector to use as necessary.
  std::vector<int32_t> empty_tags_;
};

TEST_F(V4L2MetadataTest, FillStaticSuccess) {
  android::CameraMetadata metadata(1);
  camera_metadata_t* metadata_raw = metadata.release();

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
  EXPECT_CALL(*component1_, StaticTags()).WillOnce(ReturnRef(static_tags_1));
  EXPECT_CALL(*component1_, ControlTags()).WillOnce(ReturnRef(control_tags_1));
  EXPECT_CALL(*component1_, DynamicTags()).WillOnce(ReturnRef(dynamic_tags_1));
  EXPECT_CALL(*component2_, StaticTags()).WillOnce(ReturnRef(static_tags_2));
  EXPECT_CALL(*component2_, ControlTags()).WillOnce(ReturnRef(control_tags_2));
  EXPECT_CALL(*component2_, DynamicTags()).WillOnce(ReturnRef(dynamic_tags_2));

  AddComponents();
  // Should succeed. If it didn't, no reason to continue checking output.
  ASSERT_EQ(dut_->FillStaticMetadata(&metadata_raw), 0);

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
  metadata.acquire(metadata_raw);
  CompareTags(static_tags,
              metadata.find(ANDROID_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS));
  CompareTags(control_tags,
              metadata.find(ANDROID_REQUEST_AVAILABLE_REQUEST_KEYS));
  CompareTags(dynamic_tags,
              metadata.find(ANDROID_REQUEST_AVAILABLE_RESULT_KEYS));
}

TEST_F(V4L2MetadataTest, FillStaticFail) {
  camera_metadata_t* metadata = nullptr;
  int err = -99;
  // Order undefined, and may or may not exit early; use AtMost.
  EXPECT_CALL(*component1_, PopulateStaticFields(_))
      .Times(AtMost(1))
      .WillOnce(Return(0));
  EXPECT_CALL(*component2_, PopulateStaticFields(_)).WillOnce(Return(err));

  // May or may not exit early, may still try to populate meta tags.
  EXPECT_CALL(*component1_, StaticTags())
      .Times(AtMost(1))
      .WillOnce(ReturnRef(empty_tags_));
  EXPECT_CALL(*component1_, ControlTags())
      .Times(AtMost(1))
      .WillOnce(ReturnRef(empty_tags_));
  EXPECT_CALL(*component1_, DynamicTags())
      .Times(AtMost(1))
      .WillOnce(ReturnRef(empty_tags_));
  EXPECT_CALL(*component2_, StaticTags())
      .Times(AtMost(1))
      .WillOnce(ReturnRef(empty_tags_));
  EXPECT_CALL(*component2_, ControlTags())
      .Times(AtMost(1))
      .WillOnce(ReturnRef(empty_tags_));
  EXPECT_CALL(*component2_, DynamicTags())
      .Times(AtMost(1))
      .WillOnce(ReturnRef(empty_tags_));

  AddComponents();
  // If any component errors, error should be returned
  EXPECT_EQ(dut_->FillStaticMetadata(&metadata), err);
}

TEST_F(V4L2MetadataTest, IsValidSuccess) {
  camera_metadata_t* metadata = nullptr;

  // Should check if all the component request values are valid.
  EXPECT_CALL(*component1_, SupportsRequestValues(_)).WillOnce(Return(true));
  EXPECT_CALL(*component2_, SupportsRequestValues(_)).WillOnce(Return(true));

  AddComponents();
  // Should succeed.
  EXPECT_TRUE(dut_->IsValidRequest(metadata));
}

TEST_F(V4L2MetadataTest, IsValidFail) {
  camera_metadata_t* metadata = nullptr;

  // Should check if all the component request values are valid.
  // Order undefined, and may or may not exit early; use AtMost.
  EXPECT_CALL(*component1_, SupportsRequestValues(_))
      .Times(AtMost(1))
      .WillOnce(Return(true));
  EXPECT_CALL(*component2_, SupportsRequestValues(_)).WillOnce(Return(false));

  AddComponents();
  // Should fail since one of the components failed.
  EXPECT_FALSE(dut_->IsValidRequest(metadata));
}

TEST_F(V4L2MetadataTest, SetSettingsSuccess) {
  camera_metadata_t* metadata = nullptr;

  // Should check if all the components set successfully.
  EXPECT_CALL(*component1_, SetRequestValues(_)).WillOnce(Return(0));
  EXPECT_CALL(*component2_, SetRequestValues(_)).WillOnce(Return(0));

  AddComponents();
  // Should succeed.
  EXPECT_EQ(dut_->SetRequestSettings(metadata), 0);
}

TEST_F(V4L2MetadataTest, SetSettingsFail) {
  camera_metadata_t* metadata = nullptr;
  int err = -99;

  // Should check if all the components set successfully.
  // Order undefined, and may or may not exit early; use AtMost.
  EXPECT_CALL(*component1_, SetRequestValues(_))
      .Times(AtMost(1))
      .WillOnce(Return(0));
  EXPECT_CALL(*component2_, SetRequestValues(_)).WillOnce(Return(err));

  AddComponents();
  // Should fail since one of the components failed.
  EXPECT_EQ(dut_->SetRequestSettings(metadata), err);
}

TEST_F(V4L2MetadataTest, FillResultSuccess) {
  camera_metadata_t* metadata = nullptr;

  // Should check if all the components fill results successfully.
  EXPECT_CALL(*component1_, PopulateDynamicFields(_)).WillOnce(Return(0));
  EXPECT_CALL(*component2_, PopulateDynamicFields(_)).WillOnce(Return(0));

  AddComponents();
  // Should succeed.
  EXPECT_EQ(dut_->FillResultMetadata(&metadata), 0);
}

TEST_F(V4L2MetadataTest, FillResultFail) {
  camera_metadata_t* metadata = nullptr;
  int err = -99;

  // Should check if all the components fill results successfully.
  // Order undefined, and may or may not exit early; use AtMost.
  EXPECT_CALL(*component1_, PopulateDynamicFields(_))
      .Times(AtMost(1))
      .WillOnce(Return(0));
  EXPECT_CALL(*component2_, PopulateDynamicFields(_)).WillOnce(Return(err));

  AddComponents();
  // Should fail since one of the components failed.
  EXPECT_EQ(dut_->FillResultMetadata(&metadata), err);
}

}  // namespace v4l2_camera_hal
