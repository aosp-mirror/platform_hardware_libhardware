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

#include "v4l2_camera.h"

#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cstdlib>

#include <camera/CameraMetadata.h>
#include <hardware/camera3.h>

#include "common.h"
#include "metadata/metadata_common.h"
#include "stream_format.h"
#include "v4l2_metadata_factory.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

namespace v4l2_camera_hal {

// Helper function for managing metadata.
static std::vector<int32_t> getMetadataKeys(
    const camera_metadata_t* metadata) {
  std::vector<int32_t> keys;
  size_t num_entries = get_camera_metadata_entry_count(metadata);
  for (size_t i = 0; i < num_entries; ++i) {
    camera_metadata_ro_entry_t entry;
    get_camera_metadata_ro_entry(metadata, i, &entry);
    keys.push_back(entry.tag);
  }
  return keys;
}

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

V4L2Camera::V4L2Camera(int id, std::shared_ptr<V4L2Wrapper> v4l2_wrapper,
                       std::unique_ptr<Metadata> metadata)
    : default_camera_hal::Camera(id),
      device_(std::move(v4l2_wrapper)),
      metadata_(std::move(metadata)),
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

  // TODO(b/29158098): Inform service of any flashes that are no longer available
  // because this camera is in use.
  return 0;
}

void V4L2Camera::disconnect() {
  HAL_LOG_ENTER();

  connection_.reset();

  // TODO(b/29158098): Inform service of any flashes that are available again
  // because this camera is no longer in use.
}

int V4L2Camera::initStaticInfo(android::CameraMetadata* out) {
  HAL_LOG_ENTER();

  int res = metadata_->FillStaticMetadata(out);
  if (res) {
    HAL_LOGE("Failed to get static metadata.");
    return res;
  }

  // Extract max streams for use in verifying stream configs.
  res = SingleTagValue(*out, ANDROID_REQUEST_MAX_NUM_INPUT_STREAMS,
                       &max_input_streams_);
  if (res) {
    HAL_LOGE("Failed to get max num input streams from static metadata.");
    return res;
  }
  res = SingleTagValue(*out, ANDROID_REQUEST_MAX_NUM_OUTPUT_STREAMS,
                       &max_output_streams_);
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
  // Nothing to do.
  return 0;
}

int V4L2Camera::enqueueBuffer(const camera3_stream_buffer_t* camera_buffer) {
  HAL_LOG_ENTER();

  int res = device_->EnqueueBuffer(camera_buffer);
  if (res) {
    HAL_LOGE("Device failed to enqueue buffer.");
    return res;
  }

  // Turn on the stream.
  // TODO(b/29334616): Lock around stream on/off access, only start stream
  // if not already on. (For now, since it's synchronous, it will always be
  // turned off before another call to this function).
  res = device_->StreamOn();
  if (res) {
    HAL_LOGE("Device failed to turn on stream.");
    return res;
  }

  // TODO(b/29334616): Enqueueing and dequeueing should be separate worker
  // threads, not in the same function.

  // Dequeue the buffer.
  v4l2_buffer result_buffer;
  res = device_->DequeueBuffer(&result_buffer);
  if (res) {
    HAL_LOGE("Device failed to dequeue buffer.");
    return res;
  }

  // All done, cleanup.
  // TODO(b/29334616): Lock around stream on/off access, only stop stream if
  // buffer queue is empty (synchronously, there's only ever 1 buffer in the
  // queue at a time, so this is safe).
  res = device_->StreamOff();
  if (res) {
    HAL_LOGE("Device failed to turn off stream.");
    return res;
  }

  return 0;
}

int V4L2Camera::getResultSettings(android::CameraMetadata* metadata,
                                  uint64_t* timestamp) {
  HAL_LOG_ENTER();

  // Get the results.
  int res = metadata_->FillResultMetadata(metadata);
  if (res) {
    HAL_LOGE("Failed to fill result metadata.");
    return res;
  }

  // Extract the timestamp.
  int64_t frame_time = 0;
  res = SingleTagValue(*metadata, ANDROID_SENSOR_TIMESTAMP, &frame_time);
  if (res) {
    HAL_LOGE("Failed to extract timestamp from result metadata");
    return res;
  }
  *timestamp = static_cast<uint64_t>(frame_time);

  return 0;
}

bool V4L2Camera::isSupportedStreamSet(default_camera_hal::Stream** streams,
                                      int count, uint32_t mode) {
  HAL_LOG_ENTER();

  if (mode != CAMERA3_STREAM_CONFIGURATION_NORMAL_MODE) {
    HAL_LOGE("Unsupported stream configuration mode: %d", mode);
    return false;
  }

  // This should be checked by the caller, but put here as a sanity check.
  if (count < 1) {
    HAL_LOGE("Must request at least 1 stream");
    return false;
  }

  // Count the number of streams of each type.
  int32_t num_input = 0;
  int32_t num_raw = 0;
  int32_t num_stalling = 0;
  int32_t num_non_stalling = 0;
  for (int i = 0; i < count; ++i) {
    default_camera_hal::Stream* stream = streams[i];

    if (stream->isInputType()) {
      ++num_input;
    }

    if (stream->isOutputType()) {
      StreamFormat format(*stream);
      switch (format.Category()) {
        case kFormatCategoryRaw:
          ++num_raw;
        case kFormatCategoryStalling:
          ++num_stalling;
          break;
        case kFormatCategoryNonStalling:
          ++num_non_stalling;
          break;
        case kFormatCategoryUnknown:  // Fall through.
        default:
          HAL_LOGE("Unsupported format for stream %d: %d", i, stream->getFormat());
          return false;
      }
    }
  }

  if (num_input > max_input_streams_ ||
      num_raw > max_output_streams_[0] ||
      num_non_stalling > max_output_streams_[1] ||
      num_stalling > max_output_streams_[2]) {
    HAL_LOGE("Invalid stream configuration: %d input, %d RAW, %d non-stalling, "
             "%d stalling (max supported: %d input, %d RAW, %d non-stalling, "
             "%d stalling)", max_input_streams_, max_output_streams_[0],
             max_output_streams_[1], max_output_streams_[2], num_input,
             num_raw, num_non_stalling, num_stalling);
    return false;
  }

  // TODO(b/29939583): The above logic should be all that's necessary,
  // but V4L2 doesn't actually support more than 1 stream at a time. So for now,
  // if not all streams are the same format and size, error. Note that this
  // means the HAL is not spec-compliant; the requested streams are technically
  // valid and it is not technically allowed to error once it has reached this
  // point.
  int format = streams[0]->getFormat();
  uint32_t width = streams[0]->getWidth();
  uint32_t height = streams[0]->getHeight();
  for (int i = 1; i < count; ++i) {
    const default_camera_hal::Stream* stream = streams[i];
    if (stream->getFormat() != format || stream->getWidth() != width ||
        stream->getHeight() != height) {
      HAL_LOGE("V4L2 only supports 1 stream configuration at a time "
               "(stream 0 is format %d, width %u, height %u, "
               "stream %d is format %d, width %u, height %u).",
               format, width, height, i, stream->getFormat(),
               stream->getWidth(), stream->getHeight());
      return false;
    }
  }

  return true;
}

int V4L2Camera::setupStream(default_camera_hal::Stream* stream,
                            uint32_t* max_buffers) {
  HAL_LOG_ENTER();

  if (stream->getRotation() != CAMERA3_STREAM_ROTATION_0) {
    HAL_LOGE("Rotation %d not supported", stream->getRotation());
    return -EINVAL;
  }

  // Doesn't matter what was requested, we always use dataspace V0_JFIF.
  // Note: according to camera3.h, this isn't allowed, but etalvala@google.com
  // claims it's underdocumented; the implementation lets the HAL overwrite it.
  stream->setDataSpace(HAL_DATASPACE_V0_JFIF);

  int res = device_->SetFormat(*stream, max_buffers);
  if (res) {
    HAL_LOGE("Failed to set device to correct format for stream.");
    return res;
  }
  // Sanity check.
  if (*max_buffers < 1) {
    HAL_LOGE("Setting format resulted in an invalid maximum of %u buffers.",
             *max_buffers);
    return -ENODEV;
  }

  return 0;
}

bool V4L2Camera::isValidCaptureSettings(const android::CameraMetadata& settings) {
  HAL_LOG_ENTER();

  return metadata_->IsValidRequest(settings);
}

int V4L2Camera::setSettings(const android::CameraMetadata& new_settings) {
  HAL_LOG_ENTER();

  return metadata_->SetRequestSettings(new_settings);
}

}  // namespace v4l2_camera_hal
