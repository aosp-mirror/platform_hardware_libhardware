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

#include "v4l2_metadata_factory.h"

#include <camera/CameraMetadata.h>

#include "common.h"
#include "format_metadata_factory.h"
#include "metadata/control.h"
#include "metadata/control_factory.h"
#include "metadata/enum_converter.h"
#include "metadata/metadata_common.h"
#include "metadata/property.h"

namespace v4l2_camera_hal {

int GetV4L2Metadata(std::shared_ptr<V4L2Wrapper> device,
                    std::unique_ptr<Metadata>* result) {
  HAL_LOG_ENTER();

  // TODO: Temporarily connect to the device so that V4L2-specific components
  // can make any necessary queries.

  // TODO(b/30140438): Add all metadata components used by V4L2Camera here.
  // Currently these are all the fixed properties, ignored controls, and
  // V4L2 enum controls. Will add the other properties as more PartialMetadata
  // subclasses get implemented.

  PartialMetadataSet components;

  components.insert(NoEffectMenuControl<uint8_t>(
      ANDROID_COLOR_CORRECTION_ABERRATION_MODE,
      ANDROID_COLOR_CORRECTION_AVAILABLE_ABERRATION_MODES,
      {ANDROID_COLOR_CORRECTION_ABERRATION_MODE_FAST,
       ANDROID_COLOR_CORRECTION_ABERRATION_MODE_HIGH_QUALITY}));

  // TODO(b/30510395): subcomponents of 3A.
  // In general, default to ON/AUTO since they imply pretty much nothing,
  // while OFF implies guarantees about not hindering performance.
  components.insert(std::unique_ptr<PartialMetadataInterface>(
      new Property<std::array<int32_t, 3>>(ANDROID_CONTROL_MAX_REGIONS,
                                           {{/*AE*/ 0, /*AWB*/ 0, /*AF*/ 0}})));
  components.insert(V4L2ControlOrDefault<uint8_t>(
      ControlType::kMenu,
      ANDROID_CONTROL_AE_MODE,
      ANDROID_CONTROL_AE_AVAILABLE_MODES,
      device,
      V4L2_CID_EXPOSURE_AUTO,
      std::shared_ptr<ConverterInterface<uint8_t, int32_t>>(new EnumConverter(
          {{V4L2_EXPOSURE_AUTO, ANDROID_CONTROL_AE_MODE_ON},
           {V4L2_EXPOSURE_MANUAL, ANDROID_CONTROL_AE_MODE_OFF}})),
      ANDROID_CONTROL_AE_MODE_ON));
  components.insert(V4L2ControlOrDefault<uint8_t>(
      ControlType::kMenu,
      ANDROID_CONTROL_AE_ANTIBANDING_MODE,
      ANDROID_CONTROL_AE_AVAILABLE_ANTIBANDING_MODES,
      device,
      V4L2_CID_POWER_LINE_FREQUENCY,
      std::shared_ptr<ConverterInterface<uint8_t, int32_t>>(
          new EnumConverter({{V4L2_CID_POWER_LINE_FREQUENCY_DISABLED,
                              ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF},
                             {V4L2_CID_POWER_LINE_FREQUENCY_50HZ,
                              ANDROID_CONTROL_AE_ANTIBANDING_MODE_50HZ},
                             {V4L2_CID_POWER_LINE_FREQUENCY_60HZ,
                              ANDROID_CONTROL_AE_ANTIBANDING_MODE_60HZ},
                             {V4L2_CID_POWER_LINE_FREQUENCY_AUTO,
                              ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO}})),
      ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO));
  // V4L2 offers multiple white balance interfaces. Try the advanced one before
  // falling
  // back to the simpler version.
  // Modes from each API that don't match up:
  // Android: WARM_FLUORESCENT, TWILIGHT.
  // V4L2: FLUORESCENT_H, HORIZON, FLASH.
  std::unique_ptr<PartialMetadataInterface> awb(V4L2Control<uint8_t>(
      ControlType::kMenu,
      ANDROID_CONTROL_AWB_MODE,
      ANDROID_CONTROL_AWB_AVAILABLE_MODES,
      device,
      V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE,
      std::shared_ptr<ConverterInterface<uint8_t, int32_t>>(new EnumConverter(
          {{V4L2_WHITE_BALANCE_MANUAL, ANDROID_CONTROL_AWB_MODE_OFF},
           {V4L2_WHITE_BALANCE_AUTO, ANDROID_CONTROL_AWB_MODE_AUTO},
           {V4L2_WHITE_BALANCE_INCANDESCENT,
            ANDROID_CONTROL_AWB_MODE_INCANDESCENT},
           {V4L2_WHITE_BALANCE_FLUORESCENT,
            ANDROID_CONTROL_AWB_MODE_FLUORESCENT},
           {V4L2_WHITE_BALANCE_DAYLIGHT, ANDROID_CONTROL_AWB_MODE_DAYLIGHT},
           {V4L2_WHITE_BALANCE_CLOUDY,
            ANDROID_CONTROL_AWB_MODE_CLOUDY_DAYLIGHT},
           {V4L2_WHITE_BALANCE_SHADE, ANDROID_CONTROL_AWB_MODE_SHADE}}))));
  if (awb) {
    components.insert(std::move(awb));
  } else {
    // Fall back to simpler AWB or even just an ignored control.
    components.insert(V4L2ControlOrDefault<uint8_t>(
        ControlType::kMenu,
        ANDROID_CONTROL_AWB_MODE,
        ANDROID_CONTROL_AWB_AVAILABLE_MODES,
        device,
        V4L2_CID_AUTO_WHITE_BALANCE,
        std::shared_ptr<ConverterInterface<uint8_t, int32_t>>(
            new EnumConverter({{0, ANDROID_CONTROL_AWB_MODE_OFF},
                               {1, ANDROID_CONTROL_AWB_MODE_AUTO}})),
        ANDROID_CONTROL_AWB_MODE_AUTO));
  }
  // TODO(b/30510395): subcomponents of scene modes
  // (may itself be a subcomponent of 3A).
  // Modes from each API that don't match up:
  // Android: FACE_PRIORITY, ACTION, NIGHT_PORTRAIT, THEATRE, STEADYPHOTO,
  // BARCODE, HIGH_SPEED_VIDEO.
  // V4L2: BACKLIGHT, DAWN_DUSK, FALL_COLORS, TEXT.
  components.insert(V4L2ControlOrDefault<uint8_t>(
      ControlType::kMenu,
      ANDROID_CONTROL_SCENE_MODE,
      ANDROID_CONTROL_AVAILABLE_SCENE_MODES,
      device,
      V4L2_CID_SCENE_MODE,
      std::shared_ptr<ConverterInterface<uint8_t, int32_t>>(new EnumConverter(
          {{V4L2_SCENE_MODE_NONE, ANDROID_CONTROL_SCENE_MODE_DISABLED},
           {V4L2_SCENE_MODE_BEACH_SNOW, ANDROID_CONTROL_SCENE_MODE_BEACH},
           {V4L2_SCENE_MODE_BEACH_SNOW, ANDROID_CONTROL_SCENE_MODE_SNOW},
           {V4L2_SCENE_MODE_CANDLE_LIGHT,
            ANDROID_CONTROL_SCENE_MODE_CANDLELIGHT},
           {V4L2_SCENE_MODE_FIREWORKS, ANDROID_CONTROL_SCENE_MODE_FIREWORKS},
           {V4L2_SCENE_MODE_LANDSCAPE, ANDROID_CONTROL_SCENE_MODE_LANDSCAPE},
           {V4L2_SCENE_MODE_NIGHT, ANDROID_CONTROL_SCENE_MODE_NIGHT},
           {V4L2_SCENE_MODE_PARTY_INDOOR, ANDROID_CONTROL_SCENE_MODE_PARTY},
           {V4L2_SCENE_MODE_SPORTS, ANDROID_CONTROL_SCENE_MODE_SPORTS},
           {V4L2_SCENE_MODE_SUNSET, ANDROID_CONTROL_SCENE_MODE_SUNSET}})),
      ANDROID_CONTROL_SCENE_MODE_DISABLED));
  // Modes from each API that don't match up:
  // Android: POSTERIZE, WHITEBOARD, BLACKBOARD.
  // V4L2: ANTIQUE, ART_FREEZE, EMBOSS, GRASS_GREEN, SKETCH, SKIN_WHITEN,
  // SKY_BLUE, SILHOUETTE, VIVID, SET_CBCR.
  components.insert(V4L2ControlOrDefault<uint8_t>(
      ControlType::kMenu,
      ANDROID_CONTROL_EFFECT_MODE,
      ANDROID_CONTROL_AVAILABLE_EFFECTS,
      device,
      V4L2_CID_COLORFX,
      std::shared_ptr<ConverterInterface<uint8_t, int32_t>>(new EnumConverter(
          {{V4L2_COLORFX_NONE, ANDROID_CONTROL_EFFECT_MODE_OFF},
           {V4L2_COLORFX_BW, ANDROID_CONTROL_EFFECT_MODE_MONO},
           {V4L2_COLORFX_NEGATIVE, ANDROID_CONTROL_EFFECT_MODE_NEGATIVE},
           {V4L2_COLORFX_SOLARIZATION, ANDROID_CONTROL_EFFECT_MODE_SOLARIZE},
           {V4L2_COLORFX_SEPIA, ANDROID_CONTROL_EFFECT_MODE_SEPIA},
           {V4L2_COLORFX_AQUA, ANDROID_CONTROL_EFFECT_MODE_AQUA}})),
      ANDROID_CONTROL_EFFECT_MODE_OFF));

  // Not sure if V4L2 does or doesn't do this, but HAL documentation says
  // all devices must support FAST, and FAST can be equivalent to OFF, so
  // either way it's fine to list.
  components.insert(
      NoEffectMenuControl<uint8_t>(ANDROID_EDGE_MODE,
                                   ANDROID_EDGE_AVAILABLE_EDGE_MODES,
                                   {ANDROID_EDGE_MODE_FAST}));

  // TODO(30510395): subcomponents of hotpixel.
  // No known V4L2 hot pixel correction. But it might be happening,
  // so we report FAST/HIGH_QUALITY.
  components.insert(NoEffectMenuControl<uint8_t>(
      ANDROID_HOT_PIXEL_MODE,
      ANDROID_HOT_PIXEL_AVAILABLE_HOT_PIXEL_MODES,
      {ANDROID_HOT_PIXEL_MODE_FAST, ANDROID_HOT_PIXEL_MODE_HIGH_QUALITY}));
  // ON only needs to be supported for RAW capable devices.
  components.insert(NoEffectMenuControl<uint8_t>(
      ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE,
      ANDROID_STATISTICS_INFO_AVAILABLE_HOT_PIXEL_MAP_MODES,
      {ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE_OFF}));

  // TODO(30510395): subcomponents focus/lens.
  // No way to actually get the aperture and focal length
  // in V4L2, but they're required keys, so fake them.
  components.insert(
      NoEffectMenuControl<float>(ANDROID_LENS_APERTURE,
                                 ANDROID_LENS_INFO_AVAILABLE_APERTURES,
                                 {2.0}));  // RPi camera v2 is f/2.0.
  components.insert(
      NoEffectMenuControl<float>(ANDROID_LENS_FOCAL_LENGTH,
                                 ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS,
                                 {3.04}));  // RPi camera v2 is 3.04mm.
  // No known way to get filter densities from V4L2,
  // report 0 to indicate this control is not supported.
  components.insert(
      NoEffectMenuControl<float>(ANDROID_LENS_FILTER_DENSITY,
                                 ANDROID_LENS_INFO_AVAILABLE_FILTER_DENSITIES,
                                 {0.0}));
  // V4L2 focal units do not correspond to a particular physical unit.
  components.insert(
      std::unique_ptr<PartialMetadataInterface>(new Property<uint8_t>(
          ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION,
          ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_UNCALIBRATED)));
  // info.hyperfocalDistance not required for UNCALIBRATED.
  // No known V4L2 lens shading. But it might be happening,
  // so report FAST/HIGH_QUALITY.
  components.insert(NoEffectMenuControl<uint8_t>(
      ANDROID_SHADING_MODE,
      ANDROID_SHADING_AVAILABLE_MODES,
      {ANDROID_SHADING_MODE_FAST, ANDROID_SHADING_MODE_HIGH_QUALITY}));
  // ON only needs to be supported for RAW capable devices.
  components.insert(NoEffectMenuControl<uint8_t>(
      ANDROID_STATISTICS_LENS_SHADING_MAP_MODE,
      ANDROID_STATISTICS_INFO_AVAILABLE_LENS_SHADING_MAP_MODES,
      {ANDROID_STATISTICS_LENS_SHADING_MAP_MODE_OFF}));
  // V4L2 doesn't differentiate between OPTICAL and VIDEO stabilization,
  // so only report one (and report the other as OFF).
  components.insert(V4L2ControlOrDefault<uint8_t>(
      ControlType::kMenu,
      ANDROID_CONTROL_VIDEO_STABILIZATION_MODE,
      ANDROID_CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES,
      device,
      V4L2_CID_IMAGE_STABILIZATION,
      std::shared_ptr<ConverterInterface<uint8_t, int32_t>>(new EnumConverter(
          {{0, ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_OFF},
           {1, ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON}})),
      ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_OFF));
  components.insert(NoEffectMenuControl<uint8_t>(
      ANDROID_LENS_OPTICAL_STABILIZATION_MODE,
      ANDROID_LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION,
      {ANDROID_LENS_OPTICAL_STABILIZATION_MODE_OFF}));

  // Unable to control noise reduction in V4L2 devices,
  // but FAST is allowed to be the same as OFF.
  components.insert(NoEffectMenuControl<uint8_t>(
      ANDROID_NOISE_REDUCTION_MODE,
      ANDROID_NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES,
      {ANDROID_NOISE_REDUCTION_MODE_FAST}));

  // TODO(30510395): subcomponents of formats/streams.
  // For now, no thumbnails available (only [0,0], the "no thumbnail" size).
  // TODO(b/29580107): Could end up with a mismatch between request & result,
  // since V4L2 doesn't actually allow for thumbnail size control.
  components.insert(NoEffectMenuControl<std::array<int32_t, 2>>(
      ANDROID_JPEG_THUMBNAIL_SIZE,
      ANDROID_JPEG_AVAILABLE_THUMBNAIL_SIZES,
      {{{0, 0}}}));
  // TODO(b/29939583): V4L2 can only support 1 stream at a time.
  // For now, just reporting minimum allowable for LIMITED devices.
  components.insert(std::unique_ptr<PartialMetadataInterface>(
      new Property<std::array<int32_t, 3>>(
          ANDROID_REQUEST_MAX_NUM_OUTPUT_STREAMS,
          {{/* Raw */ 0, /* Non-stalling */ 2, /* Stalling */ 1}})));
  // Reprocessing not supported.
  components.insert(std::unique_ptr<PartialMetadataInterface>(
      new Property<int32_t>(ANDROID_REQUEST_MAX_NUM_INPUT_STREAMS, 0)));
  // No way to know pipeline depth for V4L2, so fake with max allowable latency.
  // Doesn't mean much without per-frame controls anyways.
  components.insert(std::unique_ptr<PartialMetadataInterface>(
      new Property<uint8_t>(ANDROID_REQUEST_PIPELINE_MAX_DEPTH, 4)));
  // "LIMITED devices are strongly encouraged to use a non-negative value.
  // If UNKNOWN is used here then app developers do not have a way to know
  // when sensor settings have been applied." - Unfortunately, V4L2 doesn't
  // really help here either. Could even be that adjusting settings mid-stream
  // blocks in V4L2, and should be avoided.
  components.insert(
      std::unique_ptr<PartialMetadataInterface>(new Property<int32_t>(
          ANDROID_SYNC_MAX_LATENCY, ANDROID_SYNC_MAX_LATENCY_UNKNOWN)));

  // TODO(30510395): subcomponents of cropping/sensors.
  // V4L2 VIDIOC_CROPCAP doesn't give a way to query this;
  // it's driver dependent. For now, assume freeform, and
  // some cameras may just behave badly.
  // TODO(b/29579652): Figure out a way to determine this.
  components.insert(std::unique_ptr<PartialMetadataInterface>(
      new Property<uint8_t>(ANDROID_SCALER_CROPPING_TYPE,
                            ANDROID_SCALER_CROPPING_TYPE_FREEFORM)));
  // No way to get in V4L2, so faked. RPi camera v2 is 3.674 x 2.760 mm.
  // Physical size is used in framework calculations (field of view,
  // pixel pitch, etc.), so faking it may have unexpected results.
  components.insert(std::unique_ptr<PartialMetadataInterface>(
      new Property<std::array<float, 2>>(ANDROID_SENSOR_INFO_PHYSICAL_SIZE,
                                         {{3.674, 2.760}})));
  // HAL uses BOOTTIME timestamps.
  // TODO(b/29457051): make sure timestamps are consistent throughout the HAL.
  components.insert(std::unique_ptr<PartialMetadataInterface>(
      new Property<uint8_t>(ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE,
                            ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE_UNKNOWN)));
  // Noo way to actually get orientation from V4L2.
  components.insert(std::unique_ptr<PartialMetadataInterface>(
      new Property<int32_t>(ANDROID_SENSOR_ORIENTATION, 0)));

  // TODO(30510395): subcomponents of face detection.
  // Face detection not supported.
  components.insert(NoEffectMenuControl<uint8_t>(
      ANDROID_STATISTICS_FACE_DETECT_MODE,
      ANDROID_STATISTICS_INFO_AVAILABLE_FACE_DETECT_MODES,
      {ANDROID_STATISTICS_FACE_DETECT_MODE_OFF}));
  components.insert(std::unique_ptr<PartialMetadataInterface>(
      new Property<int32_t>(ANDROID_STATISTICS_INFO_MAX_FACE_COUNT, 0)));

  /* Capabilities. */
  // The V4L2Metadata pretends to at least meet the
  // "LIMITED" and "BACKWARD_COMPATIBLE" functionality requirements.
  components.insert(std::unique_ptr<PartialMetadataInterface>(
      new Property<uint8_t>(ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL,
                            ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_LIMITED)));
  components.insert(std::unique_ptr<PartialMetadataInterface>(
      new Property<std::vector<uint8_t>>(
          ANDROID_REQUEST_AVAILABLE_CAPABILITIES,
          {ANDROID_REQUEST_AVAILABLE_CAPABILITIES_BACKWARD_COMPATIBLE})));

  int res =
      AddFormatComponents(device, std::inserter(components, components.end()));
  if (res) {
    HAL_LOGE("Failed to initialize format components.");
    return res;
  }

  *result = std::make_unique<Metadata>(std::move(components));
  return 0;
}

}  // namespace v4l2_camera_hal
