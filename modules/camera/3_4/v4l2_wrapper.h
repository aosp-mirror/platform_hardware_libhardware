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

#ifndef V4L2_CAMERA_HAL_V4L2_WRAPPER_H_
#define V4L2_CAMERA_HAL_V4L2_WRAPPER_H_

#include <memory>
#include <mutex>
#include <string>

#include <nativehelper/ScopedFd.h>

#include "common.h"
#include "stream.h"
#include "stream_format.h"
#include "v4l2_gralloc.h"

namespace v4l2_camera_hal {
class V4L2Wrapper {
 public:
  // Use this method to create V4L2Wrapper objects. Functionally equivalent
  // to "new V4L2Wrapper", except that it may return nullptr in case of failure.
  static V4L2Wrapper* NewV4L2Wrapper(const std::string device_path);
  virtual ~V4L2Wrapper();

  // Connect or disconnect to the device.
  int Connect();
  void Disconnect();
  // Turn the stream on or off.
  int StreamOn();
  int StreamOff();
  // Manage controls.
  int QueryControl(uint32_t control_id, v4l2_query_ext_ctrl* result);
  int GetControl(uint32_t control_id, int32_t* value);
  int SetControl(uint32_t control_id, int32_t desired, int32_t* result);
  // Manage format.
  int SetFormat(const default_camera_hal::Stream& stream,
                uint32_t* result_max_buffers);
  // Manage buffers.
  int EnqueueBuffer(const camera3_stream_buffer_t* camera_buffer);
  int DequeueBuffer(v4l2_buffer* buffer);

  inline bool connected() { return device_fd_.get() >= 0; }

 private:
  // Constructor is private to allow failing on bad input.
  // Use NewV4L2Wrapper instead.
  V4L2Wrapper(const std::string device_path,
              std::unique_ptr<V4L2Gralloc> gralloc);

  // Perform an ioctl call in a thread-safe fashion.
  template <typename T>
  int IoctlLocked(int request, T data);
  // Adjust buffers any time a device is connected/reformatted.
  int SetupBuffers();

  // The camera device path. For example, /dev/video0.
  const std::string device_path_;
  // The opened device fd.
  ScopedFd device_fd_;
  // The underlying gralloc module.
  std::unique_ptr<V4L2Gralloc> gralloc_;
  // Whether or not the device supports the extended control query.
  bool extended_query_supported_;
  // The format this device is set up for.
  std::unique_ptr<StreamFormat> format_;
  // The maximum number of buffers this device can handle in its current format.
  uint32_t max_buffers_;
  // Lock protecting use of the device.
  std::mutex device_lock_;

  friend class V4L2WrapperMock;

  DISALLOW_COPY_AND_ASSIGN(V4L2Wrapper);
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_V4L2_WRAPPER_H_
