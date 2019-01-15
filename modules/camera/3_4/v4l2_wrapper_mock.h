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

// Mock for wrapper class used to communicate with V4L2 devices.

#ifndef V4L2_CAMERA_HAL_V4L2_WRAPPER_MOCK_H_
#define V4L2_CAMERA_HAL_V4L2_WRAPPER_MOCK_H_

#include "v4l2_wrapper.h"

#include <gmock/gmock.h>

namespace v4l2_camera_hal {

class V4L2WrapperMock : public V4L2Wrapper {
 public:
  V4L2WrapperMock() : V4L2Wrapper(""){};
  MOCK_METHOD0(StreamOn, int());
  MOCK_METHOD0(StreamOff, int());
  MOCK_METHOD2(QueryControl,
               int(uint32_t control_id, v4l2_query_ext_ctrl* result));
  MOCK_METHOD2(GetControl, int(uint32_t control_id, int32_t* value));
  MOCK_METHOD3(SetControl,
               int(uint32_t control_id, int32_t desired, int32_t* result));
  MOCK_METHOD1(GetFormats, int(std::set<uint32_t>*));
  MOCK_METHOD1(GetQualifiedFormats, int(std::vector<uint32_t>*));
  MOCK_METHOD2(GetFormatFrameSizes,
               int(uint32_t, std::set<std::array<int32_t, 2>>*));
  MOCK_METHOD3(GetFormatFrameDurationRange,
               int(uint32_t,
                   const std::array<int32_t, 2>&,
                   std::array<int64_t, 2>*));
  MOCK_METHOD2(SetFormat, int(const StreamFormat& desired_format,
                              uint32_t* result_max_buffers));
  MOCK_METHOD2(EnqueueBuffer,
               int(const camera3_stream_buffer_t* camera_buffer,
                   uint32_t* enqueued_index));
  MOCK_METHOD1(DequeueBuffer, int(uint32_t* dequeued_index));
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_V4L2_WRAPPER_MOCK_H_
