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

#ifndef DEFAULT_CAMERA_HAL_CAPTURE_REQUEST_H_
#define DEFAULT_CAMERA_HAL_CAPTURE_REQUEST_H_

#include <memory>
#include <vector>

#include <camera/CameraMetadata.h>
#include <hardware/camera3.h>

namespace default_camera_hal {

// A simple wrapper for camera3_capture_request_t,
// with a constructor that makes a deep copy from the original struct.
struct CaptureRequest {
  uint32_t frame_number;
  android::CameraMetadata settings;
  std::unique_ptr<camera3_stream_buffer_t> input_buffer;
  std::vector<camera3_stream_buffer_t> output_buffers;

  CaptureRequest();
  // Create a deep copy of |request|.
  CaptureRequest(const camera3_capture_request_t* request);
};

}  // namespace default_camera_hal

#endif  // DEFAULT_CAMERA_HAL_CAPTURE_REQUEST_H_
