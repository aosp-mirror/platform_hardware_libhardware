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

#include "request_tracker.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using testing::AtMost;
using testing::Expectation;
using testing::Return;
using testing::SetArgPointee;
using testing::Test;
using testing::_;

namespace default_camera_hal {

class RequestTrackerTest : public Test {
 protected:
  void SetUp() {
    stream1_.max_buffers = 3;
    stream2_.max_buffers = 3;
    dut_.reset(new RequestTracker());
    streams_ = {&stream1_, &stream2_};
    camera3_stream_configuration_t config{streams_.size(), streams_.data(), 0};
    dut_->SetStreamConfiguration(config);
  }

  std::unique_ptr<CaptureRequest> GenerateCaptureRequest(
      uint32_t frame, std::vector<camera3_stream_t*> streams) {
    std::unique_ptr<CaptureRequest> request =
        std::make_unique<CaptureRequest>();

    // Set the frame number and buffers.
    request->frame_number = frame;
    for (const auto stream : streams) {
      // All we really care about for the buffers is which stream they're for.
      camera3_stream_buffer_t buffer{stream, nullptr, 0, -1, -1};
      request->output_buffers.push_back(buffer);
    }

    return std::move(request);
  }

  void AddRequest(uint32_t frame,
                  std::vector<camera3_stream_t*> streams,
                  bool expected = true) {
    std::unique_ptr<CaptureRequest> request =
        GenerateCaptureRequest(frame, streams);
    EXPECT_EQ(dut_->CanAddRequest(*request), expected);
    if (expected) {
      EXPECT_FALSE(dut_->InFlight(frame));
    }
    EXPECT_EQ(dut_->Add(std::move(request)), expected);
    if (expected) {
      EXPECT_TRUE(dut_->InFlight(frame));
    }
  }

  camera3_stream_t stream1_;
  camera3_stream_t stream2_;
  std::vector<camera3_stream_t*> streams_;
  std::unique_ptr<RequestTracker> dut_;
};

TEST_F(RequestTrackerTest, AddValid) {
  uint32_t frame = 34;
  EXPECT_FALSE(dut_->InFlight(frame));
  AddRequest(frame, {&stream1_});
}

TEST_F(RequestTrackerTest, AddInput) {
  EXPECT_TRUE(dut_->Empty());

  // Add a request
  uint32_t frame = 42;
  std::unique_ptr<CaptureRequest> expected = GenerateCaptureRequest(frame, {});
  // Set the input buffer instead of any outputs.
  expected->input_buffer.reset(
      new camera3_stream_buffer_t{&stream1_, nullptr, 0, -1, -1});
  stream1_.max_buffers = 1;

  EXPECT_TRUE(dut_->Add(std::move(expected)));
  EXPECT_TRUE(dut_->InFlight(frame));
  // Should have added to the count of buffers for stream 1.
  EXPECT_TRUE(dut_->StreamFull(&stream1_));
}

TEST_F(RequestTrackerTest, AddMultipleStreams) {
  stream1_.max_buffers = 1;
  stream2_.max_buffers = 1;

  EXPECT_FALSE(dut_->StreamFull(&stream1_));
  EXPECT_FALSE(dut_->StreamFull(&stream2_));

  // Add a request using both streams.
  AddRequest(99, {&stream1_, &stream2_});

  // Should both have been counted.
  EXPECT_TRUE(dut_->StreamFull(&stream1_));
  EXPECT_TRUE(dut_->StreamFull(&stream2_));
}

TEST_F(RequestTrackerTest, AddUnconfigured) {
  camera3_stream_t stream;
  // Unconfigured should be considered full.
  EXPECT_TRUE(dut_->StreamFull(&stream));
  AddRequest(1, {&stream}, false);
}

TEST_F(RequestTrackerTest, AddPastCapacity) {
  // Set the limit of stream 2 to 1.
  stream2_.max_buffers = 1;

  for (size_t i = 0; i < stream1_.max_buffers; ++i) {
    EXPECT_FALSE(dut_->StreamFull(&stream1_));
    EXPECT_FALSE(dut_->StreamFull(&stream2_));
    AddRequest(i, {&stream1_});
  }
  // Filled up stream 1.
  EXPECT_TRUE(dut_->StreamFull(&stream1_));
  // Stream 2 should still not be full since nothing was added.
  EXPECT_FALSE(dut_->StreamFull(&stream2_));

  // Limit has been hit, can't add more.
  AddRequest(stream1_.max_buffers, {&stream1_, &stream2_}, false);
  EXPECT_TRUE(dut_->StreamFull(&stream1_));
  // Should not have added to the count of stream 2.
  EXPECT_FALSE(dut_->StreamFull(&stream2_));
}

TEST_F(RequestTrackerTest, AddDuplicate) {
  uint32_t frame = 42;
  AddRequest(frame, {&stream1_});
  // Can't add a duplicate.
  AddRequest(frame, {&stream2_}, false);
}

TEST_F(RequestTrackerTest, RemoveValid) {
  EXPECT_TRUE(dut_->Empty());

  // Add a request
  uint32_t frame = 42;
  std::unique_ptr<CaptureRequest> request =
      GenerateCaptureRequest(frame, {&stream1_});
  CaptureRequest* expected = request.get();
  EXPECT_TRUE(dut_->Add(std::move(request)));
  EXPECT_TRUE(dut_->InFlight(frame));
  AddRequest(frame + 1, {&stream1_});
  EXPECT_FALSE(dut_->Empty());

  // Remove it.
  request.reset();
  EXPECT_TRUE(dut_->Remove(frame, &request));
  // Should have removed only the desired request.
  EXPECT_FALSE(dut_->Empty());

  // Should have returned the desired request.
  EXPECT_EQ(request.get(), expected);
}

TEST_F(RequestTrackerTest, RemoveNoResult) {
  EXPECT_TRUE(dut_->Empty());

  // Add a request
  uint32_t frame = 42;
  AddRequest(frame, {&stream1_});
  EXPECT_FALSE(dut_->Empty());

  // Remove it, but don't ask for the returned request.
  EXPECT_TRUE(dut_->Remove(frame));
  EXPECT_TRUE(dut_->Empty());
}

TEST_F(RequestTrackerTest, RemoveInvalid) {
  EXPECT_TRUE(dut_->Empty());

  // Add a request
  uint32_t frame = 42;
  AddRequest(frame, {&stream1_});
  EXPECT_FALSE(dut_->Empty());

  // Try to remove a different one.
  uint32_t bad_frame = frame + 1;
  EXPECT_FALSE(dut_->InFlight(bad_frame));
  EXPECT_FALSE(dut_->Remove(frame + 1));
  EXPECT_FALSE(dut_->Empty());
}

TEST_F(RequestTrackerTest, ClearRequests) {
  // Create some requests.
  uint32_t frame1 = 42;
  uint32_t frame2 = frame1 + 1;
  std::unique_ptr<CaptureRequest> request1 =
      GenerateCaptureRequest(frame1, {&stream1_});
  std::unique_ptr<CaptureRequest> request2 =
      GenerateCaptureRequest(frame2, {&stream2_});
  std::set<CaptureRequest*> expected;
  expected.insert(request1.get());
  expected.insert(request2.get());

  // Insert them.
  EXPECT_TRUE(dut_->Add(std::move(request1)));
  EXPECT_TRUE(dut_->Add(std::move(request2)));
  EXPECT_TRUE(dut_->InFlight(frame1));
  EXPECT_TRUE(dut_->InFlight(frame2));
  EXPECT_FALSE(dut_->Empty());
  std::set<std::unique_ptr<CaptureRequest>> actual_wrapped;

  // Clear them out.
  dut_->Clear(&actual_wrapped);
  EXPECT_TRUE(dut_->Empty());

  // Get the hard pointer values for comparison.
  std::set<CaptureRequest*> actual;
  for (const auto& actual_request : actual_wrapped) {
    actual.insert(actual_request.get());
  }
  EXPECT_EQ(actual, expected);
}

TEST_F(RequestTrackerTest, ClearRequestsNoResult) {
  // Add some requests.
  EXPECT_TRUE(dut_->Empty());
  AddRequest(1, {&stream1_});
  AddRequest(2, {&stream2_});
  EXPECT_FALSE(dut_->Empty());
  // Don't bother getting the cleared requests.
  dut_->Clear();
  EXPECT_TRUE(dut_->Empty());
}

TEST_F(RequestTrackerTest, ClearConfiguration) {
  EXPECT_FALSE(dut_->StreamFull(&stream1_));
  EXPECT_FALSE(dut_->StreamFull(&stream2_));

  // Clear the configuration.
  dut_->ClearStreamConfiguration();

  // Both streams should be considered full now, since neither is configured.
  EXPECT_TRUE(dut_->StreamFull(&stream1_));
  EXPECT_TRUE(dut_->StreamFull(&stream2_));
}

}  // namespace default_camera_hal
