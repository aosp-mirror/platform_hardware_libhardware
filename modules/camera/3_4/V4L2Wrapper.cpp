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

#include "V4L2Wrapper.h"

#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <mutex>

#include <nativehelper/ScopedFd.h>

#include "Common.h"
#include "Stream.h"
#include "StreamFormat.h"

namespace v4l2_camera_hal {

V4L2Wrapper::V4L2Wrapper(const std::string device_path)
    : device_path_(device_path), max_buffers_(0) {
  HAL_LOG_ENTER();
}

V4L2Wrapper::~V4L2Wrapper() { HAL_LOG_ENTER(); }

int V4L2Wrapper::Connect() {
  HAL_LOG_ENTER();
  std::lock_guard<std::mutex> lock(device_lock_);

  if (connected()) {
    HAL_LOGE("Camera device %s is already connected. Close it first",
             device_path_.c_str());
    return -EIO;
  }

  int fd = TEMP_FAILURE_RETRY(open(device_path_.c_str(), O_RDWR));
  if (fd < 0) {
    HAL_LOGE("failed to open %s (%s)", device_path_.c_str(), strerror(errno));
    return -errno;
  }
  device_fd_.reset(fd);

  // Check if this connection has the extended control query capability.
  v4l2_query_ext_ctrl query;
  query.id = V4L2_CTRL_FLAG_NEXT_CTRL | V4L2_CTRL_FLAG_NEXT_COMPOUND;
  // Already holding the lock, so don't call ioctlLocked.
  int res = TEMP_FAILURE_RETRY(
      ioctl(device_fd_.get(), VIDIOC_QUERY_EXT_CTRL, &query));
  extended_query_supported_ = (res == 0);

  // TODO(b/29185945): confirm this is a supported device.
  // This is checked by the HAL, but the device at device_path_ may
  // not be the same one that was there when the HAL was loaded.
  // (Alternatively, better hotplugging support may make this unecessary
  // by disabling cameras that get disconnected and checking newly connected
  // cameras, so Connect() is never called on an unsupported camera)
  return 0;
}

void V4L2Wrapper::Disconnect() {
  HAL_LOG_ENTER();
  std::lock_guard<std::mutex> lock(device_lock_);

  device_fd_.reset();  // Includes close().
  format_.reset();
  max_buffers_ = 0;
}

// Helper function. Should be used instead of ioctl throughout this class.
template <typename T>
int V4L2Wrapper::IoctlLocked(int request, T data) {
  HAL_LOG_ENTER();
  std::lock_guard<std::mutex> lock(device_lock_);

  if (!connected()) {
    HAL_LOGE("Device %s not connected.", device_path_.c_str());
    return -ENODEV;
  }
  return TEMP_FAILURE_RETRY(ioctl(device_fd_.get(), request, data));
}

int V4L2Wrapper::StreamOn() {
  HAL_LOG_ENTER();

  if (!format_) {
    HAL_LOGE("Stream format must be set before turning on stream.");
    return -EINVAL;
  }

  int32_t type = format_->get_type();
  if (IoctlLocked(VIDIOC_STREAMON, &type) < 0) {
    HAL_LOGE("STREAMON fails: %s", strerror(errno));
    return -ENODEV;
  }

  return 0;
}

int V4L2Wrapper::StreamOff() {
  HAL_LOG_ENTER();

  int32_t type = format_->get_type();
  if (IoctlLocked(VIDIOC_STREAMOFF, &type) < 0) {
    HAL_LOGE("STREAMOFF fails: %s", strerror(errno));
    return -ENODEV;
  }

  return 0;
}

int V4L2Wrapper::QueryControl(uint32_t control_id,
                              v4l2_query_ext_ctrl* result) {
  HAL_LOG_ENTER();
  int res;

  memset(result, 0, sizeof(*result));

  if (extended_query_supported_) {
    result->id = control_id;
    res = IoctlLocked(VIDIOC_QUERY_EXT_CTRL, result);
    // Assuming the operation was supported (not ENOTTY), no more to do.
    if (errno != ENOTTY) {
      if (res) {
        HAL_LOGE("QUERY_EXT_CTRL fails: %s", strerror(errno));
        return -ENODEV;
      }
      return 0;
    }
  }

  // Extended control querying not supported, fall back to basic control query.
  v4l2_queryctrl query;
  query.id = control_id;
  if (IoctlLocked(VIDIOC_QUERYCTRL, &query)) {
    HAL_LOGE("QUERYCTRL fails: %s", strerror(errno));
    return -ENODEV;
  }

  // Convert the basic result to the extended result.
  result->id = query.id;
  result->type = query.type;
  memcpy(result->name, query.name, sizeof(query.name));
  result->minimum = query.minimum;
  if (query.type == V4L2_CTRL_TYPE_BITMASK) {
    // According to the V4L2 documentation, when type is BITMASK,
    // max and default should be interpreted as __u32. Practically,
    // this means the conversion from 32 bit to 64 will pad with 0s not 1s.
    result->maximum = static_cast<uint32_t>(query.maximum);
    result->default_value = static_cast<uint32_t>(query.default_value);
  } else {
    result->maximum = query.maximum;
    result->default_value = query.default_value;
  }
  result->step = static_cast<uint32_t>(query.step);
  result->flags = query.flags;
  result->elems = 1;
  switch (result->type) {
    case V4L2_CTRL_TYPE_INTEGER64:
      result->elem_size = sizeof(int64_t);
      break;
    case V4L2_CTRL_TYPE_STRING:
      result->elem_size = result->maximum + 1;
      break;
    default:
      result->elem_size = sizeof(int32_t);
      break;
  }

  return 0;
}

int V4L2Wrapper::GetControl(uint32_t control_id, int32_t* value) {
  HAL_LOG_ENTER();

  v4l2_control control;
  control.id = control_id;
  if (IoctlLocked(VIDIOC_G_CTRL, &control) < 0) {
    HAL_LOGE("G_CTRL fails: %s", strerror(errno));
    return -ENODEV;
  }
  *value = control.value;
  return 0;
}

int V4L2Wrapper::SetControl(uint32_t control_id, int32_t desired,
                            int32_t* result) {
  HAL_LOG_ENTER();

  v4l2_control control{control_id, desired};
  if (IoctlLocked(VIDIOC_S_CTRL, &control) < 0) {
    HAL_LOGE("S_CTRL fails: %s", strerror(errno));
    return -ENODEV;
  }
  *result = control.value;
  return 0;
}

int V4L2Wrapper::SetFormat(const default_camera_hal::Stream& stream) {
  HAL_LOG_ENTER();

  // Should be checked earlier; sanity check.
  if (stream.isInputType()) {
    HAL_LOGE("Input streams not supported.");
    return -EINVAL;
  }

  StreamFormat desired_format(stream);
  if (desired_format == *format_) {
    HAL_LOGV("Already in correct format, skipping format setting.");
    return 0;
  }

  // Not in the correct format, set our format.
  v4l2_format new_format;
  desired_format.FillFormatRequest(&new_format);
  // TODO(b/29334616): When async, this will need to check if the stream
  // is on, and if so, lock it off while setting format.
  if (IoctlLocked(VIDIOC_S_FMT, &new_format) < 0) {
    HAL_LOGE("S_FMT failed: %s", strerror(errno));
    return -ENODEV;
  }

  // Check that the driver actually set to the requested values.
  if (desired_format != new_format) {
    HAL_LOGE("Device doesn't support desired stream configuration.");
    return -EINVAL;
  }

  // Keep track of our new format.
  format_.reset(new StreamFormat(new_format));

  // Format changed, setup new buffers.
  SetupBuffers();
  return 0;
}

int V4L2Wrapper::SetupBuffers() {
  HAL_LOG_ENTER();

  // "Request" a buffer (since we're using a userspace buffer, this just
  // tells V4L2 to switch into userspace buffer mode).
  v4l2_requestbuffers req_buffers;
  memset(&req_buffers, 0, sizeof(req_buffers));
  req_buffers.type = format_->get_type();
  req_buffers.memory = V4L2_MEMORY_USERPTR;
  req_buffers.count = 1;
  if (IoctlLocked(VIDIOC_REQBUFS, &req_buffers) < 0) {
    HAL_LOGE("REQBUFS failed: %s", strerror(errno));
    return -ENODEV;
  }

  // V4L2 will set req_buffers.count to a number of buffers it can handle.
  max_buffers_ = req_buffers.count;
  return 0;
}

}  // namespace v4l2_camera_hal
