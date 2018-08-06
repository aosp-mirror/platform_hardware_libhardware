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

// #define LOG_NDEBUG 0
#define LOG_TAG "RequestTracker"

#include "request_tracker.h"

#include <cutils/log.h>

namespace default_camera_hal {

RequestTracker::RequestTracker() {}

RequestTracker::~RequestTracker() {}

void RequestTracker::SetStreamConfiguration(
    const camera3_stream_configuration_t& config) {
  // Clear the old configuration.
  ClearStreamConfiguration();
  // Add an entry to the buffer tracking map for each configured stream.
  for (size_t i = 0; i < config.num_streams; ++i) {
    buffers_in_flight_.emplace(config.streams[i], 0);
  }
}

void RequestTracker::ClearStreamConfiguration() {
  // The keys of the in flight buffer map are the configured streams.
  buffers_in_flight_.clear();
}

// Helper: get the streams used by a request.
std::set<camera3_stream_t*> RequestStreams(const CaptureRequest& request) {
  std::set<camera3_stream_t*> result;
  if (request.input_buffer) {
    result.insert(request.input_buffer->stream);
  }
  for (const auto& output_buffer : request.output_buffers) {
    result.insert(output_buffer.stream);
  }
  return result;
}

bool RequestTracker::Add(std::shared_ptr<CaptureRequest> request) {
  if (!CanAddRequest(*request)) {
    return false;
  }

  // Add to the count for each stream used.
  for (const auto stream : RequestStreams(*request)) {
    ++buffers_in_flight_[stream];
  }

  // Store the request.
  frames_in_flight_[request->frame_number] = request;

  return true;
}

bool RequestTracker::Remove(std::shared_ptr<CaptureRequest> request) {
  if (!request) {
    return false;
  }

  // Get the request.
  const auto frame_number_request =
      frames_in_flight_.find(request->frame_number);
  if (frame_number_request == frames_in_flight_.end()) {
    ALOGE("%s: Frame %u is not in flight.", __func__, request->frame_number);
    return false;
  } else if (request != frame_number_request->second) {
    ALOGE(
        "%s: Request for frame %u cannot be removed: "
        "does not matched the stored request.",
        __func__,
        request->frame_number);
    return false;
  }

  frames_in_flight_.erase(frame_number_request);

  // Decrement the counts of used streams.
  for (const auto stream : RequestStreams(*request)) {
    --buffers_in_flight_[stream];
  }

  return true;
}

void RequestTracker::Clear(
    std::set<std::shared_ptr<CaptureRequest>>* requests) {
  // If desired, extract all the currently in-flight requests.
  if (requests) {
    for (auto& frame_number_request : frames_in_flight_) {
      requests->insert(frame_number_request.second);
    }
  }

  // Clear out all tracking.
  frames_in_flight_.clear();
  // Maintain the configuration, but reset counts.
  for (auto& stream_count : buffers_in_flight_) {
    stream_count.second = 0;
  }
}

bool RequestTracker::CanAddRequest(const CaptureRequest& request) const {
  // Check that it's not a duplicate.
  if (frames_in_flight_.count(request.frame_number) > 0) {
    ALOGE("%s: Already tracking a request with frame number %d.",
          __func__,
          request.frame_number);
    return false;
  }

  // Check that each stream has space
  // (which implicitly checks if it is configured).
  for (const auto stream : RequestStreams(request)) {
    if (StreamFull(stream)) {
      ALOGE("%s: Stream %p is full.", __func__, stream);
      return false;
    }
  }
  return true;
}

bool RequestTracker::StreamFull(const camera3_stream_t* handle) const {
  const auto it = buffers_in_flight_.find(handle);
  if (it == buffers_in_flight_.end()) {
    // Unconfigured streams are implicitly full.
    ALOGV("%s: Stream %p is not a configured stream.", __func__, handle);
    return true;
  } else {
    return it->second >= it->first->max_buffers;
  }
}

bool RequestTracker::InFlight(uint32_t frame_number) const {
  return frames_in_flight_.count(frame_number) > 0;
}

bool RequestTracker::Empty() const {
  return frames_in_flight_.empty();
}

}  // namespace default_camera_hal
