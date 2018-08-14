/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_USB_COMMON_TYPES_H_
#define HAL_USB_COMMON_TYPES_H_

#include <string>
#include <vector>

namespace arc {

struct DeviceInfo {
  // ex: /dev/video0
  std::string device_path;
  // USB vender id
  std::string usb_vid;
  // USB product id
  std::string usb_pid;
  // Some cameras need to wait several frames to output correct images.
  uint32_t frames_to_skip_after_streamon;

  // Member definitions can be found in https://developer.android.com/
  // reference/android/hardware/camera2/CameraCharacteristics.html
  uint32_t lens_facing;
  int32_t sensor_orientation;
  float horizontal_view_angle_16_9;
  float horizontal_view_angle_4_3;
  std::vector<float> lens_info_available_focal_lengths;
  float lens_info_minimum_focus_distance;
  float lens_info_optimal_focus_distance;
  float vertical_view_angle_16_9;
  float vertical_view_angle_4_3;
};

typedef std::vector<DeviceInfo> DeviceInfos;

struct SupportedFormat {
  uint32_t width;
  uint32_t height;
  uint32_t fourcc;
  // All the supported frame rates in fps with given width, height, and
  // pixelformat. This is not sorted. For example, suppose width, height, and
  // fourcc are 640x480 YUYV. If frameRates are 15.0 and 30.0, the camera
  // supports outputting 640X480 YUYV in 15fps or 30fps.
  std::vector<float> frameRates;
};

typedef std::vector<SupportedFormat> SupportedFormats;

}  // namespace arc

#endif  // HAL_USB_COMMON_TYPES_H_
