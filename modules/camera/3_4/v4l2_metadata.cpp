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

#include "v4l2_metadata.h"

#include <camera/CameraMetadata.h>

#include "common.h"
#include "metadata/fixed_property.h"
#include "metadata/ignored_control.h"

namespace v4l2_camera_hal {

V4L2Metadata::V4L2Metadata(V4L2Wrapper* device) : device_(device) {
  HAL_LOG_ENTER();

  // TODO(b/30140438): Add all metadata components used by V4L2Camera here.
  // Currently these are all the fixed properties. Will add the other properties
  // as more PartialMetadata subclasses get implemented.

  AddComponent(
      std::unique_ptr<PartialMetadataInterface>(new IgnoredControl<uint8_t>(
          ANDROID_COLOR_CORRECTION_ABERRATION_MODE,
          ANDROID_COLOR_CORRECTION_AVAILABLE_ABERRATION_MODES,
          {ANDROID_COLOR_CORRECTION_ABERRATION_MODE_FAST,
           ANDROID_COLOR_CORRECTION_ABERRATION_MODE_HIGH_QUALITY},
          ANDROID_COLOR_CORRECTION_ABERRATION_MODE_FAST)));

  // TODO(b/30510395): subcomponents of 3A.
  AddComponent(std::unique_ptr<PartialMetadataInterface>(
      new FixedProperty<std::array<int32_t, 3>>(
          ANDROID_CONTROL_MAX_REGIONS, {{/*AE*/ 0, /*AWB*/ 0, /*AF*/ 0}})));

  // Not sure if V4L2 does or doesn't do this, but HAL documentation says
  // all devices must support FAST, and FAST can be equivalent to OFF, so
  // either way it's fine to list.
  AddComponent(
      std::unique_ptr<PartialMetadataInterface>(new IgnoredControl<uint8_t>(
          ANDROID_EDGE_MODE, ANDROID_EDGE_AVAILABLE_EDGE_MODES,
          {ANDROID_EDGE_MODE_FAST}, ANDROID_EDGE_MODE_FAST)));

  // TODO(30510395): subcomponents of hotpixel.
  // No known V4L2 hot pixel correction. But it might be happening,
  // so we report FAST/HIGH_QUALITY.
  AddComponent(
      std::unique_ptr<PartialMetadataInterface>(new IgnoredControl<uint8_t>(
          ANDROID_HOT_PIXEL_MODE, ANDROID_HOT_PIXEL_AVAILABLE_HOT_PIXEL_MODES,
          {ANDROID_HOT_PIXEL_MODE_FAST, ANDROID_HOT_PIXEL_MODE_HIGH_QUALITY},
          ANDROID_HOT_PIXEL_MODE_FAST)));
  // ON only needs to be supported for RAW capable devices.
  AddComponent(
      std::unique_ptr<PartialMetadataInterface>(new IgnoredControl<uint8_t>(
          ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE,
          ANDROID_STATISTICS_INFO_AVAILABLE_HOT_PIXEL_MAP_MODES,
          {ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE_OFF},
          ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE_OFF)));

  // TODO(30510395): subcomponents focus/lens.
  // No way to actually get the aperture and focal length
  // in V4L2, but they're required keys, so fake them.
  AddComponent(
      std::unique_ptr<PartialMetadataInterface>(new IgnoredControl<float>(
          ANDROID_LENS_APERTURE, ANDROID_LENS_INFO_AVAILABLE_APERTURES, {2.0},
          2.0)));  // RPi camera v2 is f/2.0.
  AddComponent(
      std::unique_ptr<PartialMetadataInterface>(new IgnoredControl<float>(
          ANDROID_LENS_FOCAL_LENGTH, ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS,
          {3.04}, 3.04)));  // RPi camera v2 is 3.04mm.
  // No known way to get filter densities from V4L2,
  // report 0 to indicate this control is not supported.
  AddComponent(
      std::unique_ptr<PartialMetadataInterface>(new IgnoredControl<float>(
          ANDROID_LENS_FILTER_DENSITY,
          ANDROID_LENS_INFO_AVAILABLE_FILTER_DENSITIES, {0.0}, 0.0)));
  // V4L2 focal units do not correspond to a particular physical unit.
  AddComponent(
      std::unique_ptr<PartialMetadataInterface>(new FixedProperty<uint8_t>(
          ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION,
          ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_UNCALIBRATED)));
  // info.hyperfocalDistance not required for UNCALIBRATED.
  // No known V4L2 lens shading. But it might be happening,
  // so report FAST/HIGH_QUALITY.
  AddComponent(
      std::unique_ptr<PartialMetadataInterface>(new IgnoredControl<uint8_t>(
          ANDROID_SHADING_MODE, ANDROID_SHADING_AVAILABLE_MODES,
          {ANDROID_SHADING_MODE_FAST, ANDROID_SHADING_MODE_HIGH_QUALITY},
          ANDROID_SHADING_MODE_FAST)));
  // ON only needs to be supported for RAW capable devices.
  AddComponent(
      std::unique_ptr<PartialMetadataInterface>(new IgnoredControl<uint8_t>(
          ANDROID_STATISTICS_LENS_SHADING_MAP_MODE,
          ANDROID_STATISTICS_INFO_AVAILABLE_LENS_SHADING_MAP_MODES,
          {ANDROID_STATISTICS_LENS_SHADING_MAP_MODE_OFF},
          ANDROID_STATISTICS_LENS_SHADING_MAP_MODE_OFF)));

  // Unable to control noise reduction in V4L2 devices,
  // but FAST is allowed to be the same as OFF.
  AddComponent(
      std::unique_ptr<PartialMetadataInterface>(new IgnoredControl<uint8_t>(
          ANDROID_NOISE_REDUCTION_MODE,
          ANDROID_NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES,
          {ANDROID_NOISE_REDUCTION_MODE_FAST},
          ANDROID_NOISE_REDUCTION_MODE_FAST)));

  // TODO(30510395): subcomponents of formats/streams.
  // For now, no thumbnails available (only [0,0], the "no thumbnail" size).
  // TODO(b/29580107): Could end up with a mismatch between request & result,
  // since V4L2 doesn't actually allow for thumbnail size control.
  AddComponent(std::unique_ptr<PartialMetadataInterface>(
      new IgnoredControl<std::array<int32_t, 2>>(
          ANDROID_JPEG_THUMBNAIL_SIZE, ANDROID_JPEG_AVAILABLE_THUMBNAIL_SIZES,
          {{{0, 0}}}, {{0, 0}})));
  // TODO(b/29939583): V4L2 can only support 1 stream at a time.
  // For now, just reporting minimum allowable for LIMITED devices.
  AddComponent(std::unique_ptr<PartialMetadataInterface>(
      new FixedProperty<std::array<int32_t, 3>>(
          ANDROID_REQUEST_MAX_NUM_OUTPUT_STREAMS,
          {{/* Raw */ 0, /* Non-stalling */ 2, /* Stalling */ 1}})));
  // Reprocessing not supported.
  AddComponent(std::unique_ptr<PartialMetadataInterface>(
      new FixedProperty<int32_t>(ANDROID_REQUEST_MAX_NUM_INPUT_STREAMS, 0)));
  // No way to know pipeline depth for V4L2, so fake with max allowable latency.
  // Doesn't mean much without per-frame controls anyways.
  AddComponent(std::unique_ptr<PartialMetadataInterface>(
      new FixedProperty<uint8_t>(ANDROID_REQUEST_PIPELINE_MAX_DEPTH, 4)));
  // "LIMITED devices are strongly encouraged to use a non-negative value.
  // If UNKNOWN is used here then app developers do not have a way to know
  // when sensor settings have been applied." - Unfortunately, V4L2 doesn't
  // really help here either. Could even be that adjusting settings mid-stream
  // blocks in V4L2, and should be avoided.
  AddComponent(
      std::unique_ptr<PartialMetadataInterface>(new FixedProperty<int32_t>(
          ANDROID_SYNC_MAX_LATENCY, ANDROID_SYNC_MAX_LATENCY_UNKNOWN)));

  // TODO(30510395): subcomponents of cropping/sensors.
  // V4L2 VIDIOC_CROPCAP doesn't give a way to query this;
  // it's driver dependent. For now, assume freeform, and
  // some cameras may just behave badly.
  // TODO(b/29579652): Figure out a way to determine this.
  AddComponent(std::unique_ptr<PartialMetadataInterface>(
      new FixedProperty<uint8_t>(ANDROID_SCALER_CROPPING_TYPE,
                                 ANDROID_SCALER_CROPPING_TYPE_FREEFORM)));
  // No way to get in V4L2, so faked. RPi camera v2 is 3.674 x 2.760 mm.
  // Physical size is used in framework calculations (field of view,
  // pixel pitch, etc.), so faking it may have unexpected results.
  AddComponent(std::unique_ptr<PartialMetadataInterface>(
      new FixedProperty<std::array<float, 2>>(ANDROID_SENSOR_INFO_PHYSICAL_SIZE,
                                              {{3.674, 2.760}})));
  // HAL uses BOOTTIME timestamps.
  // TODO(b/29457051): make sure timestamps are consistent throughout the HAL.
  AddComponent(
      std::unique_ptr<PartialMetadataInterface>(new FixedProperty<uint8_t>(
          ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE,
          ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE_UNKNOWN)));
  // Noo way to actually get orientation from V4L2.
  AddComponent(std::unique_ptr<PartialMetadataInterface>(
      new FixedProperty<int32_t>(ANDROID_SENSOR_ORIENTATION, 0)));

  // TODO(30510395): subcomponents of face detection.
  // Face detection not supported.
  AddComponent(
      std::unique_ptr<PartialMetadataInterface>(new IgnoredControl<uint8_t>(
          ANDROID_STATISTICS_FACE_DETECT_MODE,
          ANDROID_STATISTICS_INFO_AVAILABLE_FACE_DETECT_MODES,
          {ANDROID_STATISTICS_FACE_DETECT_MODE_OFF},
          ANDROID_STATISTICS_FACE_DETECT_MODE_OFF)));
  AddComponent(std::unique_ptr<PartialMetadataInterface>(
      new FixedProperty<int32_t>(ANDROID_STATISTICS_INFO_MAX_FACE_COUNT, 0)));

  /* Capabilities. */
  // The V4L2Metadata pretends to at least meet the
  // "LIMITED" and "BACKWARD_COMPATIBLE" functionality requirements.
  AddComponent(
      std::unique_ptr<PartialMetadataInterface>(new FixedProperty<uint8_t>(
          ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL,
          ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_LIMITED)));
  AddComponent(std::unique_ptr<PartialMetadataInterface>(
      new FixedProperty<std::vector<uint8_t>>(
          ANDROID_REQUEST_AVAILABLE_CAPABILITIES,
          {ANDROID_REQUEST_AVAILABLE_CAPABILITIES_BACKWARD_COMPATIBLE})));
}

V4L2Metadata::~V4L2Metadata() { HAL_LOG_ENTER(); }

}  // namespace v4l2_camera_hal
