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

//#define LOG_NDEBUG 0
#define LOG_TAG "FormatMetadataFactory"

#include "format_metadata_factory.h"

#include <algorithm>
#include <set>

#include "arc/image_processor.h"
#include "common.h"
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

  return 0;
}

static int FpsRangesCompare(std::array<int32_t, 2> a,
                            std::array<int32_t, 2> b) {
  if (a[1] == b[1]) {
    return a[0] > b[0];
  }
  return a[1] > b[1];
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

  std::set<int32_t> unsupported_hal_formats;
  if (hal_formats.find(HAL_PIXEL_FORMAT_YCbCr_420_888) == hal_formats.end()) {
    HAL_LOGW("YCbCr_420_888 (0x%x) not directly supported by device.",
             HAL_PIXEL_FORMAT_YCbCr_420_888);
    hal_formats.insert(HAL_PIXEL_FORMAT_YCbCr_420_888);
    unsupported_hal_formats.insert(HAL_PIXEL_FORMAT_YCbCr_420_888);
  }
  if (hal_formats.find(HAL_PIXEL_FORMAT_BLOB) == hal_formats.end()) {
    HAL_LOGW("JPEG (0x%x) not directly supported by device.",
             HAL_PIXEL_FORMAT_BLOB);
    hal_formats.insert(HAL_PIXEL_FORMAT_BLOB);
    unsupported_hal_formats.insert(HAL_PIXEL_FORMAT_BLOB);
  }

  // As hal_formats is populated by reading and converting V4L2 formats to the
  // matching HAL formats, we will never see an implementation defined format in
  // the list. We populate it ourselves and map it to a qualified format. If no
  // qualified formats exist, this will be the first available format.
  hal_formats.insert(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED);
  unsupported_hal_formats.insert(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED);

  // Qualified formats are the set of formats supported by this camera that the
  // image processor can translate into the YU12 format. We additionally check
  // that the conversion from YU12 to the desired hal format is supported.
  std::vector<uint32_t> qualified_formats;
  res = device->GetQualifiedFormats(&qualified_formats);
  if (res && unsupported_hal_formats.size() > 1) {
    HAL_LOGE(
        "Failed to retrieve qualified formats, cannot perform conversions.");
    return res;
  }

  HAL_LOGI("Supports %zu qualified formats.", qualified_formats.size());

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
  std::vector<std::array<int32_t, 2>> fps_ranges;
  for (auto hal_format : hal_formats) {
    // Get the corresponding V4L2 format.
    uint32_t v4l2_format = StreamFormat::HalToV4L2PixelFormat(hal_format);
    if (v4l2_format == 0) {
      // Unrecognized/unused format. Should never happen since hal_formats
      // came from translating a bunch of V4L2 formats above.
      HAL_LOGE("Couldn't find V4L2 format for HAL format %d", hal_format);
      return -ENODEV;
    } else if (unsupported_hal_formats.find(hal_format) !=
               unsupported_hal_formats.end()) {
      if (hal_format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED) {
        if (qualified_formats.size() != 0) {
          v4l2_format = qualified_formats[0];
        } else if (unsupported_hal_formats.size() == 1) {
          v4l2_format = StreamFormat::HalToV4L2PixelFormat(
              HAL_PIXEL_FORMAT_YCbCr_420_888);
        } else {
          // No-op. If there are no qualified formats, and implementation
          // defined is not the only unsupported format, then other unsupported
          // formats will throw an error.
        }
        HAL_LOGW(
            "Implementation-defined format is set to V4L2 pixel format 0x%x",
            v4l2_format);
      } else if (qualified_formats.size() == 0) {
        HAL_LOGE(
            "Camera does not support required format: 0x%x, and there are no "
            "qualified"
            "formats to transform from.",
            hal_format);
        return -ENODEV;
      } else if (!arc::ImageProcessor::SupportsConversion(V4L2_PIX_FMT_YUV420,
                                                          v4l2_format)) {
        HAL_LOGE(
            "The image processor does not support conversion to required "
            "format: 0x%x",
            hal_format);
        return -ENODEV;
      } else {
        v4l2_format = qualified_formats[0];
        HAL_LOGW(
            "Hal format 0x%x will be converted from V4L2 pixel format 0x%x",
            hal_format, v4l2_format);
      }
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
      // ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES will contain all
      // the fps ranges for YUV_420_888 only since YUV_420_888 format is
      // the default camera format by Android.
      if (hal_format == HAL_PIXEL_FORMAT_YCbCr_420_888) {
        // Convert from frame durations measured in ns.
        // Min, max fps supported by all YUV formats.
        const int32_t min_fps = 1000000000 / size_max_frame_duration;
        const int32_t max_fps = 1000000000 / size_min_frame_duration;
        if (std::find(fps_ranges.begin(), fps_ranges.end(),
                      std::array<int32_t, 2>{min_fps, max_fps}) ==
            fps_ranges.end()) {
          fps_ranges.push_back({min_fps, max_fps});
        }
      }
    }
  }

  // Sort fps ranges in descending order.
  std::sort(fps_ranges.begin(), fps_ranges.end(), FpsRangesCompare);

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
      ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES, fps_ranges,
      {{CAMERA3_TEMPLATE_VIDEO_RECORD, fps_ranges.front()},
       {OTHER_TEMPLATES, fps_ranges.back()}});

  return 0;
}

}  // namespace v4l2_camera_hal
