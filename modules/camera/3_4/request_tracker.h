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

#ifndef DEFAULT_CAMERA_HAL_REQUEST_TRACKER_H_
#define DEFAULT_CAMERA_HAL_REQUEST_TRACKER_H_

#include <map>
#include <memory>
#include <set>

#include <android-base/macros.h>
#include <hardware/camera3.h>
#include "capture_request.h"

namespace default_camera_hal {

// Keep track of what requests and streams are in flight.
class RequestTracker {
 public:
  RequestTracker();
  virtual ~RequestTracker();

  // Configuration methods. Both have undefined effects on in-flight requests,
  // and should only be called when empty.
  // Add configured streams. Replaces the previous configuration if any.
  virtual void SetStreamConfiguration(
      const camera3_stream_configuration_t& config);
  // Reset to no configured streams.
  virtual void ClearStreamConfiguration();

  // Tracking methods.
  // Track a request.
  // False if a request of the same frame number is already being tracked
  virtual bool Add(std::shared_ptr<CaptureRequest> request);
  // Stop tracking a request.
  // False if the given request is not being tracked.
  virtual bool Remove(std::shared_ptr<CaptureRequest> request = nullptr);
  // Empty out all requests being tracked.
  virtual void Clear(
      std::set<std::shared_ptr<CaptureRequest>>* requests = nullptr);

  // Accessors to check availability.
  // Check that a request isn't already in flight, and won't overflow any
  // streams.
  virtual bool CanAddRequest(const CaptureRequest& request) const;
  // True if the given stream is already at max capacity.
  virtual bool StreamFull(const camera3_stream_t* handle) const;
  // True if a request is being tracked for the given frame number.
  virtual bool InFlight(uint32_t frame_number) const;
  // True if no requests being tracked.
  virtual bool Empty() const;

 private:
  // Track for each stream, how many buffers are in flight.
  std::map<const camera3_stream_t*, size_t> buffers_in_flight_;
  // Track the frames in flight.
  std::map<uint32_t, std::shared_ptr<CaptureRequest>> frames_in_flight_;

  DISALLOW_COPY_AND_ASSIGN(RequestTracker);
};

}  // namespace default_camera_hal

#endif  // DEFAULT_CAMERA_HAL_REQUEST_TRACKER_H_
