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

#ifndef V4L2_CAMERA_HAL_STREAM_FORMAT_H_
#define V4L2_CAMERA_HAL_STREAM_FORMAT_H_

#include <cstring>

#include <linux/videodev2.h>
#include "arc/common_types.h"

namespace v4l2_camera_hal {

enum FormatCategory {
  kFormatCategoryRaw,
  kFormatCategoryStalling,
  kFormatCategoryNonStalling,
  kFormatCategoryUnknown,
};

class StreamFormat {
 public:
  StreamFormat(int format, uint32_t width, uint32_t height);
  StreamFormat(const v4l2_format& format);
  StreamFormat(const arc::SupportedFormat& format);
  virtual ~StreamFormat() = default;
  // Only uint32_t members, use default generated copy and assign.

  void FillFormatRequest(v4l2_format* format) const;
  FormatCategory Category() const;

  // Accessors.
  inline uint32_t type() const { return type_; };
  inline uint32_t width() const { return width_; };
  inline uint32_t height() const { return height_; };
  inline uint32_t v4l2_pixel_format() const { return v4l2_pixel_format_; }
  inline uint32_t bytes_per_line() const { return bytes_per_line_; };

  bool operator==(const StreamFormat& other) const;
  bool operator!=(const StreamFormat& other) const;

  // HAL <-> V4L2 conversions
  // Returns 0 for unrecognized.
  static uint32_t HalToV4L2PixelFormat(int hal_pixel_format);
  // Returns -1 for unrecognized.
  static int V4L2ToHalPixelFormat(uint32_t v4l2_pixel_format);

  // ARC++ SupportedFormat Helpers
  static bool FindBestFitFormat(const arc::SupportedFormats& supported_formats,
                                const arc::SupportedFormats& qualified_formats,
                                uint32_t fourcc, uint32_t width,
                                uint32_t height,
                                arc::SupportedFormat* out_format);
  static bool FindFormatByResolution(const arc::SupportedFormats& formats,
                                     uint32_t width, uint32_t height,
                                     arc::SupportedFormat* out_format);
  static arc::SupportedFormats GetQualifiedFormats(
      const arc::SupportedFormats& supported_formats);

 private:
  uint32_t type_;
  uint32_t v4l2_pixel_format_;
  uint32_t width_;
  uint32_t height_;
  uint32_t bytes_per_line_;
};

}  // namespace v4l2_camera_hal

#endif  // V4L2_CAMERA_HAL_STREAM_FORMAT_H_
