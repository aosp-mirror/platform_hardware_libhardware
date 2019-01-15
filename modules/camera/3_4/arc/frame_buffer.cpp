/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "arc/frame_buffer.h"

#include <utility>

#include <sys/mman.h>
#include "arc/common.h"
#include "arc/image_processor.h"

namespace arc {

FrameBuffer::FrameBuffer()
    : data_(nullptr),
      data_size_(0),
      buffer_size_(0),
      width_(0),
      height_(0),
      fourcc_(0) {}

FrameBuffer::~FrameBuffer() {}

int FrameBuffer::SetDataSize(size_t data_size) {
  if (data_size > buffer_size_) {
    LOGF(ERROR) << "Buffer overflow: Buffer only has " << buffer_size_
                << ", but data needs " << data_size;
    return -EINVAL;
  }
  data_size_ = data_size;
  return 0;
}

AllocatedFrameBuffer::AllocatedFrameBuffer(int buffer_size) {
  buffer_.reset(new uint8_t[buffer_size]);
  buffer_size_ = buffer_size;
  data_ = buffer_.get();
}

AllocatedFrameBuffer::AllocatedFrameBuffer(uint8_t* buffer, int buffer_size) {
  buffer_.reset(buffer);
  buffer_size_ = buffer_size;
  data_ = buffer_.get();
}

AllocatedFrameBuffer::~AllocatedFrameBuffer() {}

int AllocatedFrameBuffer::SetDataSize(size_t size) {
  if (size > buffer_size_) {
    buffer_.reset(new uint8_t[size]);
    buffer_size_ = size;
    data_ = buffer_.get();
  }
  data_size_ = size;
  return 0;
}

void AllocatedFrameBuffer::Reset() { memset(data_, 0, buffer_size_); }

V4L2FrameBuffer::V4L2FrameBuffer(base::ScopedFD fd, int buffer_size,
                                 uint32_t width, uint32_t height,
                                 uint32_t fourcc)
    : fd_(std::move(fd)), is_mapped_(false) {
  buffer_size_ = buffer_size;
  width_ = width;
  height_ = height;
  fourcc_ = fourcc;
}

V4L2FrameBuffer::~V4L2FrameBuffer() {
  if (Unmap()) {
    LOGF(ERROR) << "Unmap failed";
  }
}

int V4L2FrameBuffer::Map() {
  base::AutoLock l(lock_);
  if (is_mapped_) {
    LOGF(ERROR) << "The buffer is already mapped";
    return -EINVAL;
  }
  void* addr = mmap(NULL, buffer_size_, PROT_READ, MAP_SHARED, fd_.get(), 0);
  if (addr == MAP_FAILED) {
    LOGF(ERROR) << "mmap() failed: " << strerror(errno);
    return -EINVAL;
  }
  data_ = static_cast<uint8_t*>(addr);
  is_mapped_ = true;
  return 0;
}

int V4L2FrameBuffer::Unmap() {
  base::AutoLock l(lock_);
  if (is_mapped_ && munmap(data_, buffer_size_)) {
    LOGF(ERROR) << "mummap() failed: " << strerror(errno);
    return -EINVAL;
  }
  is_mapped_ = false;
  return 0;
}

GrallocFrameBuffer::GrallocFrameBuffer(buffer_handle_t buffer, uint32_t width,
                                       uint32_t height, uint32_t fourcc,
                                       uint32_t device_buffer_length,
                                       uint32_t stream_usage)
    : buffer_(buffer),
      is_mapped_(false),
      device_buffer_length_(device_buffer_length),
      stream_usage_(stream_usage) {
  const hw_module_t* module = nullptr;
  int ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
  if (ret || !module) {
    LOGF(ERROR) << "Failed to get gralloc module.";
    return;
  }
  gralloc_module_ = reinterpret_cast<const gralloc_module_t*>(module);
  width_ = width;
  height_ = height;
  fourcc_ = fourcc;
}

GrallocFrameBuffer::~GrallocFrameBuffer() {
  if (Unmap()) {
    LOGF(ERROR) << "Unmap failed";
  }
}

int GrallocFrameBuffer::Map() {
  base::AutoLock l(lock_);
  if (is_mapped_) {
    LOGF(ERROR) << "The buffer is already mapped";
    return -EINVAL;
  }

  void* addr;
  int ret = 0;
  switch (fourcc_) {
    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_YVU420:
    case V4L2_PIX_FMT_YUYV:
      android_ycbcr yuv_data;
      ret = gralloc_module_->lock_ycbcr(gralloc_module_, buffer_, stream_usage_,
                                        0, 0, width_, height_, &yuv_data);
      addr = yuv_data.y;
      break;
    case V4L2_PIX_FMT_JPEG:
      ret = gralloc_module_->lock(gralloc_module_, buffer_, stream_usage_, 0, 0,
                                  device_buffer_length_, 1, &addr);
      break;
    case V4L2_PIX_FMT_BGR32:
    case V4L2_PIX_FMT_RGB32:
      ret = gralloc_module_->lock(gralloc_module_, buffer_, stream_usage_, 0, 0,
                                  width_, height_, &addr);
      break;
    default:
      return -EINVAL;
  }

  if (ret) {
    LOGF(ERROR) << "Failed to gralloc lock buffer: " << ret;
    return ret;
  }

  data_ = static_cast<uint8_t*>(addr);
  if (fourcc_ == V4L2_PIX_FMT_YVU420 || fourcc_ == V4L2_PIX_FMT_YUV420 ||
      fourcc_ == V4L2_PIX_FMT_NV21 || fourcc_ == V4L2_PIX_FMT_RGB32 ||
      fourcc_ == V4L2_PIX_FMT_BGR32) {
    buffer_size_ = ImageProcessor::GetConvertedSize(fourcc_, width_, height_);
  }

  is_mapped_ = true;
  return 0;
}

int GrallocFrameBuffer::Unmap() {
  base::AutoLock l(lock_);
  if (is_mapped_ && gralloc_module_->unlock(gralloc_module_, buffer_)) {
    LOGF(ERROR) << "Failed to unmap buffer: ";
    return -EINVAL;
  }
  is_mapped_ = false;
  return 0;
}

}  // namespace arc
