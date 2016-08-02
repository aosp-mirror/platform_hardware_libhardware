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

namespace v4l2_camera_hal {

V4L2Metadata::V4L2Metadata(V4L2Wrapper* device) : device_(device) {
  HAL_LOG_ENTER();

  // TODO(b/30140438): Add all metadata components used by V4L2Camera here.
  // Currently these are all the fixed properties. Will add the other properties
  // as more PartialMetadata subclasses get implemented.

  // TODO(b/30510395): subcomponents of 3A.
  AddComponent(std::unique_ptr<PartialMetadataInterface>(
      new FixedProperty<std::array<int32_t, 3>>(
          ANDROID_CONTROL_MAX_REGIONS, {{/*AE*/ 0, /*AWB*/ 0, /*AF*/ 0}})));

  // TODO(30510395): subcomponents focus/lens.
  AddComponent(
      std::unique_ptr<PartialMetadataInterface>(new FixedProperty<uint8_t>(
          ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION,
          ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_UNCALIBRATED)));

  // TODO(30510395): subcomponents of formats/streams.
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
