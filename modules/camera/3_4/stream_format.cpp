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
#define LOG_TAG "StreamFormat"

#include "stream_format.h"

#include <system/graphics.h>
#include "arc/image_processor.h"
#include "common.h"

namespace v4l2_camera_hal {

using arc::SupportedFormat;
using arc::SupportedFormats;

static const std::vector<uint32_t> GetSupportedFourCCs() {
  // The preference of supported fourccs in the list is from high to low.
  static const std::vector<uint32_t> kSupportedFourCCs = {V4L2_PIX_FMT_YUYV,
                                                          V4L2_PIX_FMT_MJPEG};
  return kSupportedFourCCs;
}

StreamFormat::StreamFormat(int format, uint32_t width, uint32_t height)
    // TODO(b/30000211): multiplanar support.
    : type_(V4L2_BUF_TYPE_VIDEO_CAPTURE),
      v4l2_pixel_format_(StreamFormat::HalToV4L2PixelFormat(format)),
      width_(width),
      height_(height),
      bytes_per_line_(0) {}

StreamFormat::StreamFormat(const v4l2_format& format)
    : type_(format.type),
      // TODO(b/30000211): multiplanar support.
      v4l2_pixel_format_(format.fmt.pix.pixelformat),
      width_(format.fmt.pix.width),
      height_(format.fmt.pix.height),
      bytes_per_line_(format.fmt.pix.bytesperline) {}

StreamFormat::StreamFormat(const arc::SupportedFormat& format)
    : type_(V4L2_BUF_TYPE_VIDEO_CAPTURE),
      v4l2_pixel_format_(format.fourcc),
      width_(format.width),
      height_(format.height),
      bytes_per_line_(0) {}

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
  switch (v4l2_pixel_format) {
    case V4L2_PIX_FMT_BGR32:
      return HAL_PIXEL_FORMAT_RGBA_8888;
    case V4L2_PIX_FMT_JPEG:
      return HAL_PIXEL_FORMAT_BLOB;
    case V4L2_PIX_FMT_NV21:
      return HAL_PIXEL_FORMAT_YCrCb_420_SP;
    case V4L2_PIX_FMT_YUV420:
      return HAL_PIXEL_FORMAT_YCbCr_420_888;
    case V4L2_PIX_FMT_YUYV:
      return HAL_PIXEL_FORMAT_YCbCr_422_I;
    case V4L2_PIX_FMT_YVU420:
      return HAL_PIXEL_FORMAT_YV12;
    default:
      // Unrecognized format.
      HAL_LOGV("Unrecognized v4l2 pixel format %u", v4l2_pixel_format);
      break;
  }
  return -1;
}

uint32_t StreamFormat::HalToV4L2PixelFormat(int hal_pixel_format) {
  switch (hal_pixel_format) {
    case HAL_PIXEL_FORMAT_BLOB:
      return V4L2_PIX_FMT_JPEG;
    case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:  // Fall-through
    case HAL_PIXEL_FORMAT_RGBA_8888:
      return V4L2_PIX_FMT_BGR32;
    case HAL_PIXEL_FORMAT_YCbCr_420_888:
      // This is a flexible YUV format that depends on platform. Different
      // platform may have different format. It can be YVU420 or NV12. Now we
      // return YVU420 first.
      // TODO(): call drm_drv.get_fourcc() to get correct format.
      return V4L2_PIX_FMT_YUV420;
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
      return V4L2_PIX_FMT_YUYV;
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
      return V4L2_PIX_FMT_NV21;
    case HAL_PIXEL_FORMAT_YV12:
      return V4L2_PIX_FMT_YVU420;
    default:
      HAL_LOGV("Pixel format 0x%x is unsupported.", hal_pixel_format);
      break;
  }
  return -1;
}

// Copy the qualified format into out_format and return true if there is a
// proper and fitting format in the given format lists.
bool StreamFormat::FindBestFitFormat(const SupportedFormats& supported_formats,
                                     const SupportedFormats& qualified_formats,
                                     uint32_t fourcc, uint32_t width,
                                     uint32_t height,
                                     SupportedFormat* out_format) {
  // Match exact format and resolution if possible.
  for (const auto& format : supported_formats) {
    if (format.fourcc == fourcc && format.width == width &&
        format.height == height) {
      if (out_format != NULL) {
        *out_format = format;
      }
      return true;
    }
  }
  // All conversions will be done through CachedFrame for now, which will
  // immediately convert the qualified format into YU12 (YUV420). We check
  // here that the conversion between YU12 and |fourcc| is supported.
  if (!arc::ImageProcessor::SupportsConversion(V4L2_PIX_FMT_YUV420, fourcc)) {
    HAL_LOGE("Conversion between YU12 and 0x%x not supported.", fourcc);
    return false;
  }

  // Choose the qualified format with a matching resolution.
  for (const auto& format : qualified_formats) {
    if (format.width == width && format.height == height) {
      if (out_format != NULL) {
        *out_format = format;
      }
      return true;
    }
  }
  return false;
}

// Copy corresponding format into out_format and return true by matching
// resolution |width|x|height| in |formats|.
bool StreamFormat::FindFormatByResolution(const SupportedFormats& formats,
                                          uint32_t width, uint32_t height,
                                          SupportedFormat* out_format) {
  for (const auto& format : formats) {
    if (format.width == width && format.height == height) {
      if (out_format != NULL) {
        *out_format = format;
      }
      return true;
    }
  }
  return false;
}

SupportedFormats StreamFormat::GetQualifiedFormats(
    const SupportedFormats& supported_formats) {
  // The preference of supported fourccs in the list is from high to low.
  const std::vector<uint32_t> supported_fourccs = GetSupportedFourCCs();
  SupportedFormats qualified_formats;
  for (const auto& supported_fourcc : supported_fourccs) {
    for (const auto& supported_format : supported_formats) {
      if (supported_format.fourcc != supported_fourcc) {
        continue;
      }

      // Skip if |qualified_formats| already has the same resolution with a more
      // preferred fourcc.
      if (FindFormatByResolution(qualified_formats, supported_format.width,
                                 supported_format.height, NULL)) {
        continue;
      }
      qualified_formats.push_back(supported_format);
    }
  }
  return qualified_formats;
}

}  // namespace v4l2_camera_hal
