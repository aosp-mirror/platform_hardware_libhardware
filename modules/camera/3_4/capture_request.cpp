/*
 * Copyright 2016 The Android Open Source Project
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

#include "capture_request.h"

namespace default_camera_hal {

CaptureRequest::CaptureRequest() : CaptureRequest(nullptr) {}

CaptureRequest::CaptureRequest(const camera3_capture_request_t* request) {
  if (!request) {
    return;
  }

  frame_number = request->frame_number;

  // CameraMetadata makes copies of camera_metadata_t through the
  // assignment operator (the constructor taking a camera_metadata_t*
  // takes ownership instead).
  settings = request->settings;

  // camera3_stream_buffer_t can be default copy constructed,
  // as its pointer values are handles, not ownerships.

  // Copy the input buffer.
  if (request->input_buffer) {
    input_buffer =
        std::make_unique<camera3_stream_buffer_t>(*request->input_buffer);
  }

  // Safely copy all the output buffers.
  uint32_t num_output_buffers = request->num_output_buffers;
  if (num_output_buffers < 0 || !request->output_buffers) {
    num_output_buffers = 0;
  }
  output_buffers.insert(output_buffers.end(),
                        request->output_buffers,
                        request->output_buffers + num_output_buffers);
}

}  // namespace default_camera_hal
