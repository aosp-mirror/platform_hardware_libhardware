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

// Mock for partial metadata interfaces.

#ifndef V4L2_CAMERA_HAL_PARTIAL_METADATA_INTERFACE_MOCK_H_
#define V4L2_CAMERA_HAL_PARTIAL_METADATA_INTERFACE_MOCK_H_

#include "partial_metadata_interface.h"

#include <gmock/gmock.h>

namespace v4l2_camera_hal {

class PartialMetadataInterfaceMock : public PartialMetadataInterface {
 public:
  PartialMetadataInterfaceMock() : PartialMetadataInterface(){};
  MOCK_CONST_METHOD0(StaticTags, std::vector<int32_t>());
  MOCK_CONST_METHOD0(ControlTags, std::vector<int32_t>());
  MOCK_CONST_METHOD0(DynamicTags, std::vector<int32_t>());
  MOCK_CONST_METHOD1(PopulateStaticFields, int(android::CameraMetadata*));
  MOCK_CONST_METHOD1(PopulateDynamicFields, int(android::CameraMetadata*));
  MOCK_CONST_METHOD2(PopulateTemplateRequest,
                     int(int, android::CameraMetadata*));
  MOCK_CONST_METHOD1(SupportsRequestValues,
                     bool(const android::CameraMetadata&));
  MOCK_METHOD1(SetRequestValues, int(const android::CameraMetadata&));
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_PARTIAL_METADATA_INTERFACE_MOCK_H_
