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

#include "stream_format.h"

#include <linux/videodev2.h>

#include <system/graphics.h>

#include "common.h"

namespace v4l2_camera_hal {

StreamFormat::StreamFormat(int format, uint32_t width, uint32_t height)
    // TODO(b/30000211): multiplanar support.
    : type_(V4L2_BUF_TYPE_VIDEO_CAPTURE),
      v4l2_pixel_format_(StreamFormat::HalToV4L2PixelFormat(format)),
      width_(width),
      height_(height),
      bytes_per_line_(0),
      min_buffer_size_(0) {}

StreamFormat::StreamFormat(const v4l2_format& format)
    : type_(format.type),
      // TODO(b/30000211): multiplanar support.
      v4l2_pixel_format_(format.fmt.pix.pixelformat),
      width_(format.fmt.pix.width),
      height_(format.fmt.pix.height),
      bytes_per_line_(format.fmt.pix.bytesperline),
      min_buffer_size_(format.fmt.pix.sizeimage) {}

void StreamFormat::FillFormatRequest(v4l2_format* format) const {
  memset(format, 0, sizeof(*format));
  format->type = type_;
  format->fmt.pix.pixelformat = v4l2_pixel_format_;
  format->fmt.pix.width = width_;
  format->fmt.pix.height = height_;
  // Bytes per line and min buffer size are outputs set by the driver,
  // not part of the request.
}

FormatCategory StreamFormat::Category() const {
  switch (v4l2_pixel_format_) {
    case V4L2_PIX_FMT_JPEG:
      return kFormatCategoryStalling;
    case V4L2_PIX_FMT_YUV420:  // Fall through.
    case V4L2_PIX_FMT_BGR32:
      return kFormatCategoryNonStalling;
    default:
      // Note: currently no supported RAW formats.
      return kFormatCategoryUnknown;
  }
}

bool StreamFormat::operator==(const StreamFormat& other) const {
  // Used to check that a requested format was actually set, so
  // don't compare bytes per line or min buffer size.
  return (type_ == other.type_ &&
          v4l2_pixel_format_ == other.v4l2_pixel_format_ &&
          width_ == other.width_ && height_ == other.height_);
}

bool StreamFormat::operator!=(const StreamFormat& other) const {
  return !(*this == other);
}

int StreamFormat::V4L2ToHalPixelFormat(uint32_t v4l2_pixel_format) {
  // Translate V4L2 format to HAL format.
  int hal_pixel_format = -1;
  switch (v4l2_pixel_format) {
    case V4L2_PIX_FMT_JPEG:
      hal_pixel_format = HAL_PIXEL_FORMAT_BLOB;
      break;
    case V4L2_PIX_FMT_YUV420:
      hal_pixel_format = HAL_PIXEL_FORMAT_YCbCr_420_888;
      break;
    case V4L2_PIX_FMT_BGR32:
      hal_pixel_format = HAL_PIXEL_FORMAT_RGBA_8888;
      break;
    default:
      // Unrecognized format.
      HAL_LOGV("Unrecognized v4l2 pixel format %u", v4l2_pixel_format);
      break;
  }
  return hal_pixel_format;
}

uint32_t StreamFormat::HalToV4L2PixelFormat(int hal_pixel_format) {
  // Translate HAL format to V4L2 format.
  uint32_t v4l2_pixel_format = 0;
  switch (hal_pixel_format) {
    case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:  // fall-through.
    case HAL_PIXEL_FORMAT_RGBA_8888:
      // Should be RGB32, but RPi doesn't support that.
      // For now we accept that the colors will be off.
      v4l2_pixel_format = V4L2_PIX_FMT_BGR32;
      break;
    case HAL_PIXEL_FORMAT_YCbCr_420_888:
      v4l2_pixel_format = V4L2_PIX_FMT_YUV420;
      break;
    case HAL_PIXEL_FORMAT_BLOB:
      v4l2_pixel_format = V4L2_PIX_FMT_JPEG;
      break;
    default:
      // Unrecognized format.
      HAL_LOGV("Unrecognized HAL pixel format %d", hal_pixel_format);
      break;
  }
  return v4l2_pixel_format;
}

}  // namespace v4l2_camera_hal
