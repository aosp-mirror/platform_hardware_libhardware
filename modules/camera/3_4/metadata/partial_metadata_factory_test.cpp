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

#include "partial_metadata_factory.h"

#include <camera/CameraMetadata.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "converter_interface_mock.h"
#include "metadata_common.h"
#include "test_common.h"
#include "v4l2_wrapper_mock.h"

using testing::AtMost;
using testing::Expectation;
using testing::Return;
using testing::SetArgPointee;
using testing::Test;
using testing::_;

namespace v4l2_camera_hal {

class PartialMetadataFactoryTest : public Test {
 protected:
  virtual void SetUp() {
    mock_device_.reset(new V4L2WrapperMock());
    mock_converter_.reset(new ConverterInterfaceMock<uint8_t, int32_t>());
    // Nullify control so an error will be thrown
    // if a test doesn't construct it.
    control_.reset();
  }

  virtual void ExpectControlTags() {
    ASSERT_EQ(control_->StaticTags().size(), 1u);
    EXPECT_EQ(control_->StaticTags()[0], options_tag_);
    ASSERT_EQ(control_->ControlTags().size(), 1u);
    EXPECT_EQ(control_->ControlTags()[0], delegate_tag_);
    ASSERT_EQ(control_->DynamicTags().size(), 1u);
    EXPECT_EQ(control_->DynamicTags()[0], delegate_tag_);
  }

  virtual void ExpectControlOptions(const std::vector<uint8_t>& options) {
    // Options should be available.
    android::CameraMetadata metadata;
    ASSERT_EQ(control_->PopulateStaticFields(&metadata), 0);
    EXPECT_EQ(metadata.entryCount(), 1u);
    ExpectMetadataEq(metadata, options_tag_, options);
  }

  virtual void ExpectControlValue(uint8_t value) {
    android::CameraMetadata metadata;
    ASSERT_EQ(control_->PopulateDynamicFields(&metadata), 0);
    EXPECT_EQ(metadata.entryCount(), 1u);
    ExpectMetadataEq(metadata, delegate_tag_, value);
  }

  std::unique_ptr<Control<uint8_t>> control_;
  std::shared_ptr<ConverterInterfaceMock<uint8_t, int32_t>> mock_converter_;
  std::shared_ptr<V4L2WrapperMock> mock_device_;

  // Need tags that match the data type (uint8_t) being passed.
  const int32_t delegate_tag_ = ANDROID_COLOR_CORRECTION_ABERRATION_MODE;
  const int32_t options_tag_ =
      ANDROID_COLOR_CORRECTION_AVAILABLE_ABERRATION_MODES;
};

class DISABLED_PartialMetadataFactoryTest : public PartialMetadataFactoryTest {
};

TEST_F(PartialMetadataFactoryTest, FixedState) {
  uint8_t value = 13;
  std::unique_ptr<State<uint8_t>> state = FixedState(delegate_tag_, value);

  ASSERT_EQ(state->StaticTags().size(), 0u);
  ASSERT_EQ(state->ControlTags().size(), 0u);
  ASSERT_EQ(state->DynamicTags().size(), 1u);
  EXPECT_EQ(state->DynamicTags()[0], delegate_tag_);

  android::CameraMetadata metadata;
  ASSERT_EQ(state->PopulateDynamicFields(&metadata), 0);
  EXPECT_EQ(metadata.entryCount(), 1u);
  ExpectMetadataEq(metadata, delegate_tag_, value);
}

TEST_F(PartialMetadataFactoryTest, NoEffectMenu) {
  std::vector<uint8_t> test_options = {9, 8, 12};
  control_ =
      NoEffectMenuControl<uint8_t>(delegate_tag_, options_tag_, test_options);
  ASSERT_NE(control_, nullptr);

  ExpectControlTags();

  // Options should be available.
  ExpectControlOptions(test_options);
  // Default value should be test_options[0].
  ExpectControlValue(test_options[0]);
}

TEST_F(PartialMetadataFactoryTest, NoEffectGenericMenu) {
  uint8_t default_val = 9;
  control_ = NoEffectControl<uint8_t>(
      ControlType::kMenu, delegate_tag_, options_tag_, default_val);
  ASSERT_NE(control_, nullptr);

  ExpectControlTags();

  // Options should be available.
  ExpectControlOptions({default_val});
  // |default_val| should be default option.
  ExpectControlValue(default_val);
}

TEST_F(PartialMetadataFactoryTest, NoEffectSlider) {
  std::vector<uint8_t> test_range = {9, 12};
  control_ = NoEffectSliderControl<uint8_t>(
      delegate_tag_, options_tag_, test_range[0], test_range[1]);
  ASSERT_NE(control_, nullptr);

  ExpectControlTags();

  // Single option should be available.
  ExpectControlOptions(test_range);
  // Default value should be the minimum (test_range[0]).
  ExpectControlValue(test_range[0]);
}

TEST_F(PartialMetadataFactoryTest, NoEffectGenericSlider) {
  uint8_t default_val = 9;
  control_ = NoEffectControl<uint8_t>(
      ControlType::kSlider, delegate_tag_, options_tag_, default_val);
  ASSERT_NE(control_, nullptr);

  ExpectControlTags();

  // Range containing only |default_val| should be available.
  ExpectControlOptions({default_val, default_val});
  // |default_val| should be default option.
  ExpectControlValue(default_val);
}

TEST_F(PartialMetadataFactoryTest, V4L2FactoryQueryFail) {
  int control_id = 55;
  // Should query the device.
  EXPECT_CALL(*mock_device_, QueryControl(control_id, _)).WillOnce(Return(-1));
  control_ = V4L2Control<uint8_t>(ControlType::kMenu,
                                  delegate_tag_,
                                  options_tag_,
                                  mock_device_,
                                  control_id,
                                  mock_converter_);
  // Failure, should return null.
  ASSERT_EQ(control_, nullptr);
}

TEST_F(PartialMetadataFactoryTest, V4L2FactoryQueryBadType) {
  int control_id = 55;
  v4l2_query_ext_ctrl query_result;
  query_result.type = V4L2_CTRL_TYPE_CTRL_CLASS;
  // Should query the device.
  EXPECT_CALL(*mock_device_, QueryControl(control_id, _))
      .WillOnce(DoAll(SetArgPointee<1>(query_result), Return(0)));
  control_ = V4L2Control<uint8_t>(ControlType::kMenu,
                                  delegate_tag_,
                                  options_tag_,
                                  mock_device_,
                                  control_id,
                                  mock_converter_);
  // Failure, should return null.
  ASSERT_EQ(control_, nullptr);
}

TEST_F(PartialMetadataFactoryTest, V4L2FactoryQueryBadRange) {
  int control_id = 55;
  v4l2_query_ext_ctrl query_result;
  query_result.type = V4L2_CTRL_TYPE_MENU;
  query_result.minimum = 10;
  query_result.maximum = 1;  // Less than minimum.
  // Should query the device.
  EXPECT_CALL(*mock_device_, QueryControl(control_id, _))
      .WillOnce(DoAll(SetArgPointee<1>(query_result), Return(0)));
  control_ = V4L2Control<uint8_t>(ControlType::kMenu,
                                  delegate_tag_,
                                  options_tag_,
                                  mock_device_,
                                  control_id,
                                  mock_converter_);
  // Failure, should return null.
  ASSERT_EQ(control_, nullptr);
}

TEST_F(PartialMetadataFactoryTest, V4L2FactoryTypeRequestMenuMismatch) {
  int control_id = 55;
  v4l2_query_ext_ctrl query_result;
  query_result.type = V4L2_CTRL_TYPE_INTEGER;
  query_result.minimum = 1;
  query_result.maximum = 7;
  query_result.step = 2;
  // Have conversions for values 1-5, by step size 2.
  std::map<int32_t, uint8_t> conversion_map = {{1, 10}, {3, 30}, {5, 50}};

  // Should query the device.
  EXPECT_CALL(*mock_device_, QueryControl(control_id, _))
      .WillOnce(DoAll(SetArgPointee<1>(query_result), Return(0)));

  // If you ask for a Menu, but the V4L2 control is a slider type, that's bad.
  control_ = V4L2Control<uint8_t>(ControlType::kMenu,
                                  delegate_tag_,
                                  options_tag_,
                                  mock_device_,
                                  control_id,
                                  mock_converter_);
  ASSERT_EQ(control_, nullptr);
}

TEST_F(PartialMetadataFactoryTest, V4L2FactoryTypeRequestSliderMismatch) {
  int control_id = 55;
  v4l2_query_ext_ctrl query_result;
  query_result.type = V4L2_CTRL_TYPE_MENU;
  query_result.minimum = 1;
  query_result.maximum = 7;
  query_result.step = 2;
  // Have conversions for values 1-5, by step size 2.
  std::map<int32_t, uint8_t> conversion_map = {{1, 10}, {3, 30}, {5, 50}};

  // Should query the device.
  EXPECT_CALL(*mock_device_, QueryControl(control_id, _))
      .WillOnce(DoAll(SetArgPointee<1>(query_result), Return(0)));

  // If you ask for a Slider and get a Menu, that's bad.
  control_ = V4L2Control<uint8_t>(ControlType::kSlider,
                                  delegate_tag_,
                                  options_tag_,
                                  mock_device_,
                                  control_id,
                                  mock_converter_);
  ASSERT_EQ(control_, nullptr);
}

TEST_F(DISABLED_PartialMetadataFactoryTest, V4L2FactoryMenu) {
  // TODO(b/30921166): Correct Menu support so this can be re-enabled.
  int control_id = 55;
  v4l2_query_ext_ctrl query_result;
  query_result.type = V4L2_CTRL_TYPE_MENU;
  query_result.minimum = 1;
  query_result.maximum = 7;
  query_result.step = 2;
  // Have conversions for values 1-5, by step size 2.
  std::map<int32_t, uint8_t> conversion_map = {{1, 10}, {3, 30}, {5, 50}};

  // Should query the device.
  EXPECT_CALL(*mock_device_, QueryControl(control_id, _))
      .WillOnce(DoAll(SetArgPointee<1>(query_result), Return(0)));
  // Should convert values.
  std::vector<uint8_t> expected_options;
  for (auto kv : conversion_map) {
    EXPECT_CALL(*mock_converter_, V4L2ToMetadata(kv.first, _))
        .WillOnce(DoAll(SetArgPointee<1>(kv.second), Return(0)));
    expected_options.push_back(kv.second);
  }
  // Will fail to convert 7 with -EINVAL, shouldn't matter.
  EXPECT_CALL(*mock_converter_, V4L2ToMetadata(7, _)).WillOnce(Return(-EINVAL));

  control_ = V4L2Control<uint8_t>(ControlType::kMenu,
                                  delegate_tag_,
                                  options_tag_,
                                  mock_device_,
                                  control_id,
                                  mock_converter_);
  ASSERT_NE(control_, nullptr);

  ExpectControlTags();
  ExpectControlOptions(expected_options);
}

TEST_F(DISABLED_PartialMetadataFactoryTest, V4L2FactoryMenuConversionFail) {
  // TODO(b/30921166): Correct Menu support so this can be re-enabled.
  int control_id = 55;
  v4l2_query_ext_ctrl query_result;
  query_result.type = V4L2_CTRL_TYPE_MENU;
  query_result.minimum = 1;
  query_result.maximum = 7;
  query_result.step = 2;

  // Should query the device.
  EXPECT_CALL(*mock_device_, QueryControl(control_id, _))
      .WillOnce(DoAll(SetArgPointee<1>(query_result), Return(0)));
  // Conversion fails with non-EINVAL error.
  EXPECT_CALL(*mock_converter_, V4L2ToMetadata(_, _)).WillOnce(Return(-1));

  control_ = V4L2Control<uint8_t>(ControlType::kMenu,
                                  delegate_tag_,
                                  options_tag_,
                                  mock_device_,
                                  control_id,
                                  mock_converter_);
  ASSERT_EQ(control_, nullptr);
}

TEST_F(DISABLED_PartialMetadataFactoryTest, V4L2FactoryMenuNoConversions) {
  // TODO(b/30921166): Correct Menu support so this can be re-enabled.
  int control_id = 55;
  v4l2_query_ext_ctrl query_result;
  query_result.type = V4L2_CTRL_TYPE_MENU;
  query_result.minimum = 1;
  query_result.maximum = 1;
  query_result.step = 1;

  // Should query the device.
  EXPECT_CALL(*mock_device_, QueryControl(control_id, _))
      .WillOnce(DoAll(SetArgPointee<1>(query_result), Return(0)));
  // Conversion fails with -EINVAL error.
  EXPECT_CALL(*mock_converter_, V4L2ToMetadata(1, _)).WillOnce(Return(-EINVAL));

  control_ = V4L2Control<uint8_t>(ControlType::kMenu,
                                  delegate_tag_,
                                  options_tag_,
                                  mock_device_,
                                  control_id,
                                  mock_converter_);
  // Since there were no convertable options, should fail.
  ASSERT_EQ(control_, nullptr);
}

TEST_F(PartialMetadataFactoryTest, V4L2FactoryInteger) {
  int control_id = 55;
  v4l2_query_ext_ctrl query_result;
  query_result.type = V4L2_CTRL_TYPE_INTEGER;
  query_result.minimum = 1;
  query_result.maximum = 7;
  query_result.step = 2;
  // Have conversions for values 1 & 7.
  std::map<int32_t, uint8_t> conversion_map = {{1, 10}, {7, 70}};

  // Should query the device.
  EXPECT_CALL(*mock_device_, QueryControl(control_id, _))
      .WillOnce(DoAll(SetArgPointee<1>(query_result), Return(0)));
  // Should convert values.
  std::vector<uint8_t> expected_options;
  for (auto kv : conversion_map) {
    EXPECT_CALL(*mock_converter_, V4L2ToMetadata(kv.first, _))
        .WillOnce(DoAll(SetArgPointee<1>(kv.second), Return(0)));
    expected_options.push_back(kv.second);
  }

  control_ = V4L2Control<uint8_t>(ControlType::kSlider,
                                  delegate_tag_,
                                  options_tag_,
                                  mock_device_,
                                  control_id,
                                  mock_converter_);
  ASSERT_NE(control_, nullptr);

  ExpectControlTags();
  ExpectControlOptions(expected_options);

  // Should be fitting converted values to steps.
  uint8_t set_val = 10;
  android::CameraMetadata metadata;
  EXPECT_EQ(UpdateMetadata(&metadata, delegate_tag_, set_val), 0);
  EXPECT_CALL(*mock_converter_, MetadataToV4L2(set_val, _))
      .WillOnce(DoAll(SetArgPointee<1>(4), Return(0)));
  // When it calls into the device, the 4 returned above should be
  // rounded down to the step value of 3.
  EXPECT_CALL(*mock_device_, SetControl(control_id, 3, _)).WillOnce(Return(0));
  EXPECT_EQ(control_->SetRequestValues(metadata), 0);
}

TEST_F(PartialMetadataFactoryTest, V4L2FactoryIntegerFailedConversion) {
  int control_id = 55;
  v4l2_query_ext_ctrl query_result;
  query_result.type = V4L2_CTRL_TYPE_INTEGER;
  query_result.minimum = 1;
  query_result.maximum = 7;
  query_result.step = 2;

  // Should query the device.
  EXPECT_CALL(*mock_device_, QueryControl(control_id, _))
      .WillOnce(DoAll(SetArgPointee<1>(query_result), Return(0)));
  // Fail to convert a value. Even -EINVAL is bad in this case.
  EXPECT_CALL(*mock_converter_, V4L2ToMetadata(1, _)).WillOnce(Return(-EINVAL));

  control_ = V4L2Control<uint8_t>(ControlType::kSlider,
                                  delegate_tag_,
                                  options_tag_,
                                  mock_device_,
                                  control_id,
                                  mock_converter_);
  ASSERT_EQ(control_, nullptr);
}

TEST_F(PartialMetadataFactoryTest, V4L2FallbackMenu) {
  uint8_t default_val = 9;
  int control_id = 55;

  // Should query the device.
  EXPECT_CALL(*mock_device_, QueryControl(control_id, _)).WillOnce(Return(-1));

  // Shouldn't fail, should fall back to menu control.
  control_ = V4L2ControlOrDefault<uint8_t>(ControlType::kMenu,
                                           delegate_tag_,
                                           options_tag_,
                                           mock_device_,
                                           control_id,
                                           mock_converter_,
                                           default_val);
  ASSERT_NE(control_, nullptr);

  ExpectControlTags();

  // Options should be available.
  ExpectControlOptions({default_val});
  // |default_val| should be default option.
  ExpectControlValue(default_val);
}

TEST_F(PartialMetadataFactoryTest, V4L2FallbackSlider) {
  uint8_t default_val = 9;
  int control_id = 55;

  // Should query the device.
  EXPECT_CALL(*mock_device_, QueryControl(control_id, _)).WillOnce(Return(-1));

  // Shouldn't fail, should fall back to slider control.
  control_ = V4L2ControlOrDefault<uint8_t>(ControlType::kSlider,
                                           delegate_tag_,
                                           options_tag_,
                                           mock_device_,
                                           control_id,
                                           mock_converter_,
                                           default_val);
  ASSERT_NE(control_, nullptr);

  ExpectControlTags();

  // Range containing only |default_val| should be available.
  ExpectControlOptions({default_val, default_val});
  // |default_val| should be default option.
  ExpectControlValue(default_val);
}

}  // namespace v4l2_camera_hal
