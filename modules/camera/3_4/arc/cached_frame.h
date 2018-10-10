/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_USB_CACHED_FRAME_H_
#define HAL_USB_CACHED_FRAME_H_

#include <memory>

#include <camera/CameraMetadata.h>
#include "arc/image_processor.h"

namespace arc {

// CachedFrame contains a source FrameBuffer and a cached, converted
// FrameBuffer. The incoming frames would be converted to YU12, the default
// format of libyuv, to allow convenient processing.
class CachedFrame {
 public:
  CachedFrame();
  ~CachedFrame();

  // SetSource() doesn't take ownership of |frame|. The caller can only release
  // |frame| after calling UnsetSource(). SetSource() immediately converts
  // incoming frame into YU12. Return non-zero values if it encounters errors.
  // If |rotate_degree| is 90 or 270, |frame| will be cropped, rotated by the
  // specified amount and scaled.
  // If |rotate_degree| is -1, |frame| will not be cropped, rotated, and scaled.
  // This function will return an error if |rotate_degree| is not -1, 90, or
  // 270.
  int SetSource(const FrameBuffer* frame, int rotate_degree);
  void UnsetSource();

  uint8_t* GetSourceBuffer() const;
  size_t GetSourceDataSize() const;
  uint32_t GetSourceFourCC() const;
  uint8_t* GetCachedBuffer() const;
  uint32_t GetCachedFourCC() const;

  uint32_t GetWidth() const;
  uint32_t GetHeight() const;

  // Calculate the output buffer size when converting to the specified pixel
  // format. |fourcc| is defined as V4L2_PIX_FMT_* in linux/videodev2.h. Return
  // 0 on error.
  size_t GetConvertedSize(int fourcc) const;

  // Caller should fill everything except |data_size| and |fd| of |out_frame|.
  // The function will do format conversion and scale to fit |out_frame|
  // requirement.
  // If |video_hack| is true, it outputs YU12 when |hal_pixel_format| is YV12
  // (swapping U/V planes). Caller should fill |fourcc|, |data|, and
  // Return non-zero error code on failure; return 0 on success.
  int Convert(const android::CameraMetadata& metadata, FrameBuffer* out_frame,
              bool video_hack = false);

 private:
  int ConvertToYU12();
  // When we have a landscape mounted camera and the current camera activity is
  // portrait, the frames shown in the activity would be stretched. Therefore,
  // we want to simulate a native portrait camera. That's why we want to crop,
  // rotate |rotate_degree| clockwise and scale the frame. HAL would not change
  // CameraInfo.orientation. Instead, framework would fake the
  // CameraInfo.orientation. Framework would then tell HAL how much the frame
  // needs to rotate clockwise by |rotate_degree|.
  int CropRotateScale(int rotate_degree);

  const FrameBuffer* source_frame_;
  // const V4L2FrameBuffer* source_frame_;

  // Temporary buffer for cropped and rotated results.
  std::unique_ptr<uint8_t[]> cropped_buffer_;
  size_t cropped_buffer_capacity_;

  // Cache YU12 decoded results.
  std::unique_ptr<AllocatedFrameBuffer> yu12_frame_;

  // Temporary buffer for scaled results.
  std::unique_ptr<AllocatedFrameBuffer> scaled_frame_;
};

}  // namespace arc

#endif  // HAL_USB_CACHED_FRAME_H_
