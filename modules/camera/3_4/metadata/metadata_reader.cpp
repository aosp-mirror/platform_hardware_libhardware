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

// #define LOG_NDEBUG 0
#define LOG_TAG "MetadataReader"

#include "metadata_reader.h"

#include <cutils/log.h>
#include <system/camera.h>

#include "metadata_common.h"

namespace default_camera_hal {

MetadataReader::MetadataReader(
    std::unique_ptr<const android::CameraMetadata> metadata)
    : metadata_(std::move(metadata)) {}

MetadataReader::~MetadataReader() {}

int MetadataReader::Facing(int* facing) const {
  uint8_t metadata_facing = 0;
  int res = v4l2_camera_hal::SingleTagValue(
      *metadata_, ANDROID_LENS_FACING, &metadata_facing);
  if (res) {
    ALOGE("%s: Failed to get facing from static metadata.", __func__);
    return res;
  }

  switch (metadata_facing) {
    case (ANDROID_LENS_FACING_FRONT):
      *facing = CAMERA_FACING_FRONT;
      break;
    case (ANDROID_LENS_FACING_BACK):
      *facing = CAMERA_FACING_BACK;
      break;
    case (ANDROID_LENS_FACING_EXTERNAL):
      *facing = CAMERA_FACING_EXTERNAL;
      break;
    default:
      ALOGE("%s: Invalid facing from static metadata: %d.",
            __func__,
            metadata_facing);
      return -EINVAL;
  }
  return 0;
}

int MetadataReader::Orientation(int* orientation) const {
  int32_t metadata_orientation = 0;
  int res = v4l2_camera_hal::SingleTagValue(
      *metadata_, ANDROID_SENSOR_ORIENTATION, &metadata_orientation);
  if (res) {
    ALOGE("%s: Failed to get orientation from static metadata.", __func__);
    return res;
  }

  // Orientation must be 0, 90, 180, or 270.
  if (metadata_orientation < 0 || metadata_orientation > 270 ||
      metadata_orientation % 90 != 0) {
    ALOGE(
        "%s: Invalid orientation %d "
        "(must be a 90-degree increment in [0, 360)).",
        __func__,
        metadata_orientation);
    return -EINVAL;
  }

  *orientation = static_cast<int>(metadata_orientation);
  return 0;
}

int MetadataReader::MaxInputStreams(int32_t* max_input) const {
  int res = v4l2_camera_hal::SingleTagValue(
      *metadata_, ANDROID_REQUEST_MAX_NUM_INPUT_STREAMS, max_input);
  if (res == -ENOENT) {
    // Not required; default to 0.
    *max_input = 0;
  } else if (res) {
    ALOGE("%s: Failed to get max output streams from static metadata.",
          __func__);
    return res;
  }
  return 0;
}

int MetadataReader::MaxOutputStreams(int32_t* max_raw,
                                     int32_t* max_non_stalling,
                                     int32_t* max_stalling) const {
  std::array<int32_t, 3> max_output_streams;
  int res = v4l2_camera_hal::SingleTagValue(
      *metadata_, ANDROID_REQUEST_MAX_NUM_OUTPUT_STREAMS, &max_output_streams);
  if (res) {
    ALOGE("%s: Failed to get max output streams from static metadata.",
          __func__);
    return res;
  }
  *max_raw = max_output_streams[0];
  *max_non_stalling = max_output_streams[1];
  *max_stalling = max_output_streams[2];
  return 0;
}

int MetadataReader::RequestCapabilities(std::set<uint8_t>* capabilities) const {
  std::vector<uint8_t> raw_capabilities;
  int res = v4l2_camera_hal::VectorTagValue(
      *metadata_, ANDROID_REQUEST_AVAILABLE_CAPABILITIES, &raw_capabilities);
  if (res) {
    ALOGE("%s: Failed to get request capabilities from static metadata.",
          __func__);
    return res;
  }

  // Move from vector to set.
  capabilities->insert(raw_capabilities.begin(), raw_capabilities.end());
  return 0;
}

int MetadataReader::StreamConfigurations(
    std::vector<StreamConfiguration>* configs) const {
  std::vector<RawStreamConfiguration> raw_stream_configs;
  int res = v4l2_camera_hal::VectorTagValue(
      *metadata_,
      ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
      &raw_stream_configs);
  if (res) {
    ALOGE("%s: Failed to get stream configs from static metadata.", __func__);
    return res;
  }

  // TODO(b/31384253): check for required configs.

  // Convert from raw.
  configs->insert(
      configs->end(), raw_stream_configs.begin(), raw_stream_configs.end());

  // Check that all configs are valid.
  for (const auto& config : *configs) {
    // Must have positive dimensions.
    if (config.spec.width < 1 || config.spec.height < 1) {
      ALOGE("%s: Invalid stream config: non-positive dimensions (%d, %d).",
            __func__,
            config.spec.width,
            config.spec.height);
      return -EINVAL;
    }
    // Must have a known direction enum.
    switch (config.direction) {
      case ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT:
      case ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_INPUT:
        break;
      default:
        ALOGE("%s: Invalid stream config direction: %d.",
              __func__,
              config.direction);
        return -EINVAL;
    }
  }
  return 0;
}

int MetadataReader::StreamStallDurations(
    std::vector<StreamStallDuration>* stalls) const {
  std::vector<RawStreamStallDuration> raw_stream_stall_durations;
  int res =
      v4l2_camera_hal::VectorTagValue(*metadata_,
                                      ANDROID_SCALER_AVAILABLE_STALL_DURATIONS,
                                      &raw_stream_stall_durations);
  if (res) {
    ALOGE("%s: Failed to get stall durations from static metadata.", __func__);
    return res;
  }

  // Convert from raw.
  stalls->insert(stalls->end(),
                 raw_stream_stall_durations.begin(),
                 raw_stream_stall_durations.end());
  // Check that all stalls are valid.
  for (const auto& stall : *stalls) {
    // Must have positive dimensions.
    if (stall.spec.width < 1 || stall.spec.height < 1) {
      ALOGE("%s: Invalid stall duration: non-positive dimensions (%d, %d).",
            __func__,
            stall.spec.width,
            stall.spec.height);
      return -EINVAL;
    }
    // Must have a non-negative stall.
    if (stall.duration < 0) {
      ALOGE("%s: Invalid stall duration: negative stall %lld.",
            __func__,
            static_cast<long long>(stall.duration));
      return -EINVAL;
    }
    // TODO(b/31384253): YUV_420_888, RAW10, RAW12, RAW_OPAQUE,
    // and IMPLEMENTATION_DEFINED must have 0 stall duration.
  }

  return 0;
}

int MetadataReader::ReprocessFormats(ReprocessFormatMap* reprocess_map) const {
  std::vector<int32_t> input_output_formats;
  int res = v4l2_camera_hal::VectorTagValue(
      *metadata_,
      ANDROID_SCALER_AVAILABLE_INPUT_OUTPUT_FORMATS_MAP,
      &input_output_formats);
  if (res) {
    ALOGE("%s: Failed to get input output format map from static metadata.",
          __func__);
    return res;
  }

  // Convert from the raw vector.
  for (size_t i = 0; i < input_output_formats.size();) {
    // The map is represented as variable-length entries of the format
    // input, num_outputs, <outputs>.

    // Get the input format.
    int32_t input_format = input_output_formats[i++];

    // Find the output begin and end for this format.
    int32_t num_output_formats = input_output_formats[i++];
    if (num_output_formats < 1) {
      ALOGE(
          "%s: No output formats for input format %d.", __func__, input_format);
      return -EINVAL;
    }
    size_t outputs_end = i + num_output_formats;
    if (outputs_end > input_output_formats.size()) {
      ALOGE("%s: Input format %d requests more data than available.",
            __func__,
            input_format);
      return -EINVAL;
    }

    // Copy all the output formats into the map.
    (*reprocess_map)[input_format].insert(
        input_output_formats.data() + i,
        input_output_formats.data() + outputs_end);

    // Move on to the next entry.
    i = outputs_end;
  }

  // TODO(b/31384253): check for required mappings.

  return 0;
}

}  // namespace default_camera_hal
