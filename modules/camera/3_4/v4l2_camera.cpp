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

//#define LOG_NDEBUG 0
#define LOG_TAG "V4L2Camera"

#include "v4l2_camera.h"

#include <cstdlib>
#include <fcntl.h>

#include <camera/CameraMetadata.h>
#include <hardware/camera3.h>
#include <linux/videodev2.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "common.h"
#include "function_thread.h"
#include "metadata/metadata_common.h"
#include "stream_format.h"
#include "v4l2_metadata_factory.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

namespace v4l2_camera_hal {

V4L2Camera* V4L2Camera::NewV4L2Camera(int id, const std::string path) {
  HAL_LOG_ENTER();

  std::shared_ptr<V4L2Wrapper> v4l2_wrapper(V4L2Wrapper::NewV4L2Wrapper(path));
  if (!v4l2_wrapper) {
    HAL_LOGE("Failed to initialize V4L2 wrapper.");
    return nullptr;
  }

  std::unique_ptr<Metadata> metadata;
  int res = GetV4L2Metadata(v4l2_wrapper, &metadata);
  if (res) {
    HAL_LOGE("Failed to initialize V4L2 metadata: %d", res);
    return nullptr;
  }

  return new V4L2Camera(id, std::move(v4l2_wrapper), std::move(metadata));
}

V4L2Camera::V4L2Camera(int id,
                       std::shared_ptr<V4L2Wrapper> v4l2_wrapper,
                       std::unique_ptr<Metadata> metadata)
    : default_camera_hal::Camera(id),
      device_(std::move(v4l2_wrapper)),
      metadata_(std::move(metadata)),
      buffer_enqueuer_(new FunctionThread(
          std::bind(&V4L2Camera::enqueueRequestBuffers, this))),
      buffer_dequeuer_(new FunctionThread(
          std::bind(&V4L2Camera::dequeueRequestBuffers, this))),
      max_input_streams_(0),
      max_output_streams_({{0, 0, 0}}) {
  HAL_LOG_ENTER();
}

V4L2Camera::~V4L2Camera() {
  HAL_LOG_ENTER();
}

int V4L2Camera::connect() {
  HAL_LOG_ENTER();

  if (connection_) {
    HAL_LOGE("Already connected. Please disconnect and try again.");
    return -EIO;
  }

  connection_.reset(new V4L2Wrapper::Connection(device_));
  if (connection_->status()) {
    HAL_LOGE("Failed to connect to device.");
    return connection_->status();
  }

  // TODO(b/29185945): confirm this is a supported device.
  // This is checked by the HAL, but the device at |device_|'s path may
  // not be the same one that was there when the HAL was loaded.
  // (Alternatively, better hotplugging support may make this unecessary
  // by disabling cameras that get disconnected and checking newly connected
  // cameras, so connect() is never called on an unsupported camera)

  // TODO(b/29158098): Inform service of any flashes that are no longer
  // available because this camera is in use.
  return 0;
}

void V4L2Camera::disconnect() {
  HAL_LOG_ENTER();

  connection_.reset();

  // TODO(b/29158098): Inform service of any flashes that are available again
  // because this camera is no longer in use.
}

int V4L2Camera::flushBuffers() {
  HAL_LOG_ENTER();
  return device_->StreamOff();
}

int V4L2Camera::initStaticInfo(android::CameraMetadata* out) {
  HAL_LOG_ENTER();

  int res = metadata_->FillStaticMetadata(out);
  if (res) {
    HAL_LOGE("Failed to get static metadata.");
    return res;
  }

  // Extract max streams for use in verifying stream configs.
  res = SingleTagValue(
      *out, ANDROID_REQUEST_MAX_NUM_INPUT_STREAMS, &max_input_streams_);
  if (res) {
    HAL_LOGE("Failed to get max num input streams from static metadata.");
    return res;
  }
  res = SingleTagValue(
      *out, ANDROID_REQUEST_MAX_NUM_OUTPUT_STREAMS, &max_output_streams_);
  if (res) {
    HAL_LOGE("Failed to get max num output streams from static metadata.");
    return res;
  }

  return 0;
}

int V4L2Camera::initTemplate(int type, android::CameraMetadata* out) {
  HAL_LOG_ENTER();

  return metadata_->GetRequestTemplate(type, out);
}

void V4L2Camera::initDeviceInfo(camera_info_t* info) {
  HAL_LOG_ENTER();

  // TODO(b/31044975): move this into device interface.
  // For now, just constants.
  info->resource_cost = 100;
  info->conflicting_devices = nullptr;
  info->conflicting_devices_length = 0;
}

int V4L2Camera::initDevice() {
  HAL_LOG_ENTER();

  // Start the buffer enqueue/dequeue threads if they're not already running.
  if (!buffer_enqueuer_->isRunning()) {
    android::status_t res = buffer_enqueuer_->run("Enqueue buffers");
    if (res != android::OK) {
      HAL_LOGE("Failed to start buffer enqueue thread: %d", res);
      return -ENODEV;
    }
  }
  if (!buffer_dequeuer_->isRunning()) {
    android::status_t res = buffer_dequeuer_->run("Dequeue buffers");
    if (res != android::OK) {
      HAL_LOGE("Failed to start buffer dequeue thread: %d", res);
      return -ENODEV;
    }
  }

  return 0;
}

int V4L2Camera::enqueueRequest(
    std::shared_ptr<default_camera_hal::CaptureRequest> request) {
  HAL_LOG_ENTER();

  // Assume request validated before calling this function.
  // (For now, always exactly 1 output buffer, no inputs).
  {
    std::lock_guard<std::mutex> guard(request_queue_lock_);
    request_queue_.push(request);
    requests_available_.notify_one();
  }

  return 0;
}

std::shared_ptr<default_camera_hal::CaptureRequest>
V4L2Camera::dequeueRequest() {
  std::unique_lock<std::mutex> lock(request_queue_lock_);
  while (request_queue_.empty()) {
    requests_available_.wait(lock);
  }

  std::shared_ptr<default_camera_hal::CaptureRequest> request =
      request_queue_.front();
  request_queue_.pop();
  return request;
}

bool V4L2Camera::enqueueRequestBuffers() {
  // Get a request from the queue (blocks this thread until one is available).
  std::shared_ptr<default_camera_hal::CaptureRequest> request =
      dequeueRequest();

  // Assume request validated before being added to the queue
  // (For now, always exactly 1 output buffer, no inputs).

  // Setting and getting settings are best effort here,
  // since there's no way to know through V4L2 exactly what
  // settings are used for a buffer unless we were to enqueue them
  // one at a time, which would be too slow.

  // Set the requested settings
  int res = metadata_->SetRequestSettings(request->settings);
  if (res) {
    HAL_LOGE("Failed to set settings.");
    completeRequest(request, res);
    return true;
  }

  // Replace the requested settings with a snapshot of
  // the used settings/state immediately before enqueue.
  res = metadata_->FillResultMetadata(&request->settings);
  if (res) {
    // Note: since request is a shared pointer, this may happen if another
    // thread has already decided to complete the request (e.g. via flushing),
    // since that locks the metadata (in that case, this failing is fine,
    // and completeRequest will simply do nothing).
    HAL_LOGE("Failed to fill result metadata.");
    completeRequest(request, res);
    return true;
  }

  // Actually enqueue the buffer for capture.
  res = device_->EnqueueRequest(request);
  if (res) {
    HAL_LOGE("Device failed to enqueue buffer.");
    completeRequest(request, res);
    return true;
  }

  // Make sure the stream is on (no effect if already on).
  res = device_->StreamOn();
  if (res) {
    HAL_LOGE("Device failed to turn on stream.");
    // Don't really want to send an error for only the request here,
    // since this is a full device error.
    // TODO: Should trigger full flush.
    return true;
  }

  std::unique_lock<std::mutex> lock(in_flight_lock_);
  in_flight_buffer_count_++;
  buffers_in_flight_.notify_one();
  return true;
}

bool V4L2Camera::dequeueRequestBuffers() {
  // Dequeue a buffer.
  std::shared_ptr<default_camera_hal::CaptureRequest> request;
  int res;

  {
    std::unique_lock<std::mutex> lock(in_flight_lock_);
    res = device_->DequeueRequest(&request);
    if (!res) {
      if (request) {
        completeRequest(request, res);
        in_flight_buffer_count_--;
      }
      return true;
    }
  }

  if (res == -EAGAIN) {
    // EAGAIN just means nothing to dequeue right now.
    // Wait until something is available before looping again.
    std::unique_lock<std::mutex> lock(in_flight_lock_);
    while (in_flight_buffer_count_ == 0) {
      buffers_in_flight_.wait(lock);
    }
  } else {
    HAL_LOGW("Device failed to dequeue buffer: %d", res);
  }
  return true;
}

bool V4L2Camera::validateDataspacesAndRotations(
    const camera3_stream_configuration_t* stream_config) {
  HAL_LOG_ENTER();

  for (uint32_t i = 0; i < stream_config->num_streams; ++i) {
    if (stream_config->streams[i]->rotation != CAMERA3_STREAM_ROTATION_0) {
      HAL_LOGV("Rotation %d for stream %d not supported",
               stream_config->streams[i]->rotation,
               i);
      return false;
    }
    // Accept all dataspaces, as it will just be overwritten below anyways.
  }
  return true;
}

int V4L2Camera::setupStreams(camera3_stream_configuration_t* stream_config) {
  HAL_LOG_ENTER();

  std::lock_guard<std::mutex> guard(in_flight_lock_);
  // The framework should be enforcing this, but doesn't hurt to be safe.
  if (device_->GetInFlightBufferCount() != 0) {
    HAL_LOGE("Can't set device format while frames are in flight.");
    return -EINVAL;
  }
  in_flight_buffer_count_ = 0;

  // stream_config should have been validated; assume at least 1 stream.
  camera3_stream_t* stream = stream_config->streams[0];
  int format = stream->format;
  uint32_t width = stream->width;
  uint32_t height = stream->height;

  if (stream_config->num_streams > 1) {
    // TODO(b/29939583):  V4L2 doesn't actually support more than 1
    // stream at a time. If not all streams are the same format
    // and size, error. Note that this means the HAL is not spec-compliant.
    // Technically, this error should be thrown during validation, but
    // since it isn't a spec-valid error validation isn't set up to check it.
    for (uint32_t i = 1; i < stream_config->num_streams; ++i) {
      stream = stream_config->streams[i];
      if (stream->format != format || stream->width != width ||
          stream->height != height) {
        HAL_LOGE(
            "V4L2 only supports 1 stream configuration at a time "
            "(stream 0 is format %d, width %u, height %u, "
            "stream %d is format %d, width %u, height %u).",
            format,
            width,
            height,
            i,
            stream->format,
            stream->width,
            stream->height);
        return -EINVAL;
      }
    }
  }

  // Ensure the stream is off.
  int res = device_->StreamOff();
  if (res) {
    HAL_LOGE("Device failed to turn off stream for reconfiguration: %d.", res);
    return -ENODEV;
  }

  StreamFormat stream_format(format, width, height);
  uint32_t max_buffers = 0;
  res = device_->SetFormat(stream_format, &max_buffers);
  if (res) {
    HAL_LOGE("Failed to set device to correct format for stream: %d.", res);
    return -ENODEV;
  }

  // Sanity check.
  if (max_buffers < 1) {
    HAL_LOGE("Setting format resulted in an invalid maximum of %u buffers.",
             max_buffers);
    return -ENODEV;
  }

  // Set all the streams dataspaces, usages, and max buffers.
  for (uint32_t i = 0; i < stream_config->num_streams; ++i) {
    stream = stream_config->streams[i];

    // Override HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED format.
    if (stream->format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED) {
      stream->format = HAL_PIXEL_FORMAT_RGBA_8888;
    }

    // Max buffers as reported by the device.
    stream->max_buffers = max_buffers;

    // Usage: currently using sw graphics.
    switch (stream->stream_type) {
      case CAMERA3_STREAM_INPUT:
        stream->usage = GRALLOC_USAGE_SW_READ_OFTEN;
        break;
      case CAMERA3_STREAM_OUTPUT:
        stream->usage = GRALLOC_USAGE_SW_WRITE_OFTEN;
        break;
      case CAMERA3_STREAM_BIDIRECTIONAL:
        stream->usage =
            GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN;
        break;
      default:
        // nothing to do.
        break;
    }

    // Doesn't matter what was requested, we always use dataspace V0_JFIF.
    // Note: according to camera3.h, this isn't allowed, but the camera
    // framework team claims it's underdocumented; the implementation lets the
    // HAL overwrite it. If this is changed, change the validation above.
    stream->data_space = HAL_DATASPACE_V0_JFIF;
  }

  return 0;
}

bool V4L2Camera::isValidRequestSettings(
    const android::CameraMetadata& settings) {
  if (!metadata_->IsValidRequest(settings)) {
    HAL_LOGE("Invalid request settings.");
    return false;
  }
  return true;
}

}  // namespace v4l2_camera_hal
