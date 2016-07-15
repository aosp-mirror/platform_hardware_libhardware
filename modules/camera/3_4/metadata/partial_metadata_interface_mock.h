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

#include <gmock/gmock.h>

#include "partial_metadata_interface.h"

namespace v4l2_camera_hal {

class PartialMetadataInterfaceMock : public PartialMetadataInterface {
 public:
  PartialMetadataInterfaceMock() : PartialMetadataInterface(){};
  MOCK_METHOD0(StaticTags, const std::vector<int32_t>&());
  MOCK_METHOD0(ControlTags, const std::vector<int32_t>&());
  MOCK_METHOD0(DynamicTags, const std::vector<int32_t>&());
  MOCK_METHOD1(PopulateStaticFields, int(camera_metadata_t** metadata));
  MOCK_METHOD1(PopulateDynamicFields, int(camera_metadata_t** metadata));
  MOCK_METHOD1(SupportsRequestValues, bool(const camera_metadata_t* metadata));
  MOCK_METHOD1(SetRequestValues, int(const camera_metadata_t* metadata));
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_PARTIAL_METADATA_INTERFACE_MOCK_H_
