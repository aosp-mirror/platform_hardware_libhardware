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

#include "format_metadata_factory.h"

#include "metadata/array_vector.h"
#include "metadata/partial_metadata_factory.h"
#include "metadata/property.h"

namespace v4l2_camera_hal {

static int GetHalFormats(const std::shared_ptr<V4L2Wrapper>& device,
                         std::set<int32_t>* result_formats) {
  if (!result_formats) {
    HAL_LOGE("Null result formats pointer passed");
    return -EINVAL;
  }

  std::set<uint32_t> v4l2_formats;
  int res = device->GetFormats(&v4l2_formats);
  if (res) {
    HAL_LOGE("Failed to get device formats.");
    return res;
  }
  for (auto v4l2_format : v4l2_formats) {
    int32_t hal_format = StreamFormat::V4L2ToHalPixelFormat(v4l2_format);
    if (hal_format < 0) {
      // Unrecognized/unused format. Skip it.
      continue;
    }
    result_formats->insert(hal_format);
  }

  // In addition to well-defined formats, there may be an
  // "Implementation Defined" format chosen by the HAL (in this
  // case what that means is managed by the StreamFormat class).

  // Get the V4L2 format for IMPLEMENTATION_DEFINED.
  int v4l2_format = StreamFormat::HalToV4L2PixelFormat(
      HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED);
  // If it's available, add IMPLEMENTATION_DEFINED to the result set.
  if (v4l2_format && v4l2_formats.count(v4l2_format) > 0) {
    result_formats->insert(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED);
  }

  return 0;
}

int AddFormatComponents(
    std::shared_ptr<V4L2Wrapper> device,
    std::insert_iterator<PartialMetadataSet> insertion_point) {
  HAL_LOG_ENTER();

  // Get all supported formats.
  std::set<int32_t> hal_formats;
  int res = GetHalFormats(device, &hal_formats);
  if (res) {
    return res;
  }

  // Requirements check: need to support YCbCr_420_888, JPEG,
  // and "Implementation Defined".
  if (hal_formats.find(HAL_PIXEL_FORMAT_YCbCr_420_888) == hal_formats.end()) {
    HAL_LOGE("YCbCr_420_888 not supported by device.");
    return -ENODEV;
  } else if (hal_formats.find(HAL_PIXEL_FORMAT_BLOB) == hal_formats.end()) {
    HAL_LOGE("JPEG not supported by device.");
    return -ENODEV;
  } else if (hal_formats.find(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED) ==
             hal_formats.end()) {
    HAL_LOGE("HAL implementation defined format not supported by device.");
    return -ENODEV;
  }

  // Find sizes and frame/stall durations for all formats.
  // We also want to find the smallest max frame duration amongst all formats,
  // And the largest min frame duration amongst YUV (i.e. largest max frame rate
  // supported by all YUV sizes).
  // Stream configs are {format, width, height, direction} (input or output).
  ArrayVector<int32_t, 4> stream_configs;
  // Frame durations are {format, width, height, duration} (duration in ns).
  ArrayVector<int64_t, 4> min_frame_durations;
  // Stall durations are {format, width, height, duration} (duration in ns).
  ArrayVector<int64_t, 4> stall_durations;
  int64_t min_max_frame_duration = std::numeric_limits<int64_t>::max();
  int64_t max_min_frame_duration_yuv = std::numeric_limits<int64_t>::min();
  for (auto hal_format : hal_formats) {
    // Get the corresponding V4L2 format.
    uint32_t v4l2_format = StreamFormat::HalToV4L2PixelFormat(hal_format);
    if (v4l2_format == 0) {
      // Unrecognized/unused format. Should never happen since hal_formats
      // came from translating a bunch of V4L2 formats above.
      HAL_LOGE("Couldn't find V4L2 format for HAL format %d", hal_format);
      return -ENODEV;
    }

    // Get the available sizes for this format.
    std::set<std::array<int32_t, 2>> frame_sizes;
    res = device->GetFormatFrameSizes(v4l2_format, &frame_sizes);
    if (res) {
      HAL_LOGE("Failed to get all frame sizes for format %d", v4l2_format);
      return res;
    }

    for (const auto& frame_size : frame_sizes) {
      // Note the format and size combination in stream configs.
      stream_configs.push_back(
          {{hal_format,
            frame_size[0],
            frame_size[1],
            ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT}});

      // Find the duration range for this format and size.
      std::array<int64_t, 2> duration_range;
      res = device->GetFormatFrameDurationRange(
          v4l2_format, frame_size, &duration_range);
      if (res) {
        HAL_LOGE(
            "Failed to get frame duration range for format %d, "
            "size %u x %u",
            v4l2_format,
            frame_size[0],
            frame_size[1]);
        return res;
      }
      int64_t size_min_frame_duration = duration_range[0];
      int64_t size_max_frame_duration = duration_range[1];
      min_frame_durations.push_back({{hal_format,
                                      frame_size[0],
                                      frame_size[1],
                                      size_min_frame_duration}});

      // Note the stall duration for this format and size.
      // Usually 0 for non-jpeg, non-zero for JPEG.
      // Randomly choosing absurd 1 sec for JPEG. Unsure what this breaks.
      int64_t stall_duration = 0;
      if (hal_format == HAL_PIXEL_FORMAT_BLOB) {
        stall_duration = 1000000000;
      }
      stall_durations.push_back(
          {{hal_format, frame_size[0], frame_size[1], stall_duration}});

      // Update our search for general min & max frame durations.
      // In theory max frame duration (min frame rate) should be consistent
      // between all formats, but we check and only advertise the smallest
      // available max duration just in case.
      if (size_max_frame_duration < min_max_frame_duration) {
        min_max_frame_duration = size_max_frame_duration;
      }
      // We only care about the largest min frame duration
      // (smallest max frame rate) for YUV sizes.
      if (hal_format == HAL_PIXEL_FORMAT_YCbCr_420_888 &&
          size_min_frame_duration > max_min_frame_duration_yuv) {
        max_min_frame_duration_yuv = size_min_frame_duration;
      }
    }
  }

  // Convert from frame durations measured in ns.
  // Min fps supported by all formats.
  int32_t min_fps = 1000000000 / min_max_frame_duration;
  if (min_fps > 15) {
    HAL_LOGE("Minimum FPS %d is larger than HAL max allowable value of 15",
             min_fps);
    return -ENODEV;
  }
  // Max fps supported by all YUV formats.
  int32_t max_yuv_fps = 1000000000 / max_min_frame_duration_yuv;
  // ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES should be at minimum
  // {mi, ma}, {ma, ma} where mi and ma are min and max frame rates for
  // YUV_420_888. Min should be at most 15.
  std::vector<std::array<int32_t, 2>> fps_ranges;
  fps_ranges.push_back({{min_fps, max_yuv_fps}});

  std::array<int32_t, 2> video_fps_range;
  int32_t video_fps = 30;
  if (video_fps >= max_yuv_fps) {
    video_fps_range = {{max_yuv_fps, max_yuv_fps}};
  } else {
    video_fps_range = {{video_fps, video_fps}};
  }
  fps_ranges.push_back(video_fps_range);

  // Construct the metadata components.
  insertion_point = std::make_unique<Property<ArrayVector<int32_t, 4>>>(
      ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
      std::move(stream_configs));
  insertion_point = std::make_unique<Property<ArrayVector<int64_t, 4>>>(
      ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS,
      std::move(min_frame_durations));
  insertion_point = std::make_unique<Property<ArrayVector<int64_t, 4>>>(
      ANDROID_SCALER_AVAILABLE_STALL_DURATIONS, std::move(stall_durations));
  insertion_point = std::make_unique<Property<int64_t>>(
      ANDROID_SENSOR_INFO_MAX_FRAME_DURATION, min_max_frame_duration);
  // TODO(b/31019725): This should probably not be a NoEffect control.
  insertion_point = NoEffectMenuControl<std::array<int32_t, 2>>(
      ANDROID_CONTROL_AE_TARGET_FPS_RANGE,
      ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES,
      fps_ranges,
      {{CAMERA3_TEMPLATE_VIDEO_RECORD, video_fps_range},
       {OTHER_TEMPLATES, fps_ranges[0]}});

  return 0;
}

}  // namespace v4l2_camera_hal
