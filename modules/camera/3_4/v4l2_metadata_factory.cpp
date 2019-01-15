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
#define LOG_TAG "V4L2MetadataFactory"

#include "v4l2_metadata_factory.h"

#include <camera/CameraMetadata.h>
#include "common.h"
#include "format_metadata_factory.h"
#include "metadata/boottime_state_delegate.h"
#include "metadata/control.h"
#include "metadata/enum_converter.h"
#include "metadata/partial_metadata_factory.h"
#include "metadata/property.h"
#include "metadata/scaling_converter.h"

namespace v4l2_camera_hal {

// According to spec, each unit of V4L2_CID_AUTO_EXPOSURE_BIAS is 0.001 EV.
const camera_metadata_rational_t kAeCompensationUnit = {1, 1000};
// According to spec, each unit of V4L2_CID_EXPOSURE_ABSOLUTE is 100 us.
const int64_t kV4L2ExposureTimeStepNs = 100000;
// According to spec, each unit of V4L2_CID_ISO_SENSITIVITY is ISO/1000.
const int32_t kV4L2SensitivityDenominator = 1000;
// Generously allow up to 6MB (the largest size on the RPi Camera is about 5MB).
const size_t kV4L2MaxJpegSize = 6000000;

int GetV4L2Metadata(std::shared_ptr<V4L2Wrapper> device,
                    std::unique_ptr<Metadata>* result) {
  HAL_LOG_ENTER();

  // Open a temporary connection to the device for all the V4L2 querying
  // that will be happening (this could be done for each component individually,
  // but doing it here prevents connecting and disconnecting for each one).
  V4L2Wrapper::Connection temp_connection = V4L2Wrapper::Connection(device);
  if (temp_connection.status()) {
    HAL_LOGE("Failed to connect to device: %d.", temp_connection.status());
    return temp_connection.status();
  }

  // TODO(b/30035628): Add states.

  PartialMetadataSet components;

  components.insert(NoEffectMenuControl<uint8_t>(
      ANDROID_COLOR_CORRECTION_ABERRATION_MODE,
      ANDROID_COLOR_CORRECTION_AVAILABLE_ABERRATION_MODES,
      {ANDROID_COLOR_CORRECTION_ABERRATION_MODE_FAST,
       ANDROID_COLOR_CORRECTION_ABERRATION_MODE_HIGH_QUALITY},
      {{CAMERA3_TEMPLATE_STILL_CAPTURE,
        ANDROID_COLOR_CORRECTION_ABERRATION_MODE_HIGH_QUALITY},
       {OTHER_TEMPLATES, ANDROID_COLOR_CORRECTION_ABERRATION_MODE_FAST}}));

  // TODO(b/30510395): subcomponents of 3A.
  // In general, default to ON/AUTO since they imply pretty much nothing,
  // while OFF implies guarantees about not hindering performance.
  components.insert(std::unique_ptr<PartialMetadataInterface>(
      new Property<std::array<int32_t, 3>>(ANDROID_CONTROL_MAX_REGIONS,
                                           {{/*AE*/ 0, /*AWB*/ 0, /*AF*/ 0}})));
  // TODO(b/30921166): V4L2_CID_AUTO_EXPOSURE_BIAS is an int menu, so
  // this will be falling back to NoEffect until int menu support is added.
  components.insert(V4L2ControlOrDefault<int32_t>(
      ControlType::kSlider,
      ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION,
      ANDROID_CONTROL_AE_COMPENSATION_RANGE,
      device,
      V4L2_CID_AUTO_EXPOSURE_BIAS,
      // No scaling necessary, AE_COMPENSATION_STEP handles this.
      std::make_shared<ScalingConverter<int32_t, int32_t>>(1, 1),
      0,
      {{OTHER_TEMPLATES, 0}}));
  components.insert(std::unique_ptr<PartialMetadataInterface>(
      new Property<camera_metadata_rational_t>(
          ANDROID_CONTROL_AE_COMPENSATION_STEP, kAeCompensationUnit)));
  // TODO(b/31021522): Autofocus subcomponent.
  components.insert(
      NoEffectMenuControl<uint8_t>(ANDROID_CONTROL_AF_MODE,
                                   ANDROID_CONTROL_AF_AVAILABLE_MODES,
                                   {ANDROID_CONTROL_AF_MODE_OFF}));
  // TODO(b/31021522): Should read autofocus state from
  // V4L2_CID_AUTO_FOCUS_STATUS bitmask. The framework gets a little more
  // complex than that does; there's a whole state-machine table in
  // the docs (system/media/camera/docs/docs.html).
  components.insert(FixedState<uint8_t>(ANDROID_CONTROL_AF_STATE,
                                        ANDROID_CONTROL_AF_STATE_INACTIVE));
  // TODO(b/31022735): Correctly implement AE & AF triggers that
  // actually do something. These no effect triggers are even worse than most
  // of the useless controls in this class, since technically they should
  // revert back to IDLE eventually after START/CANCEL, but for now they won't
  // unless IDLE is requested.
  components.insert(
      NoEffectMenuControl<uint8_t>(ANDROID_CONTROL_AF_TRIGGER,
                                   DO_NOT_REPORT_OPTIONS,
                                   {ANDROID_CONTROL_AF_TRIGGER_IDLE,
                                    ANDROID_CONTROL_AF_TRIGGER_START,
                                    ANDROID_CONTROL_AF_TRIGGER_CANCEL}));
  components.insert(NoEffectMenuControl<uint8_t>(
      ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER,
      DO_NOT_REPORT_OPTIONS,
      {ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_IDLE,
       ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_START,
       ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_CANCEL}));
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
      ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO,
      {{CAMERA3_TEMPLATE_MANUAL, ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF},
       {OTHER_TEMPLATES, ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO}}));
  std::unique_ptr<PartialMetadataInterface> exposure_time =
      V4L2Control<int64_t>(ControlType::kSlider,
                           ANDROID_SENSOR_EXPOSURE_TIME,
                           ANDROID_SENSOR_INFO_EXPOSURE_TIME_RANGE,
                           device,
                           V4L2_CID_EXPOSURE_ABSOLUTE,
                           std::make_shared<ScalingConverter<int64_t, int32_t>>(
                               kV4L2ExposureTimeStepNs, 1));
  // TODO(b/31037072): Sensitivity has additional V4L2 controls
  // (V4L2_CID_ISO_SENSITIVITY_AUTO), so this control currently has
  // undefined behavior.
  // TODO(b/30921166): V4L2_CID_ISO_SENSITIVITY is an int menu, so
  // this will return nullptr until that is added.
  std::unique_ptr<PartialMetadataInterface> sensitivity =
      V4L2Control<int32_t>(ControlType::kSlider,
                           ANDROID_SENSOR_SENSITIVITY,
                           ANDROID_SENSOR_INFO_SENSITIVITY_RANGE,
                           device,
                           V4L2_CID_ISO_SENSITIVITY,
                           std::make_shared<ScalingConverter<int32_t, int32_t>>(
                               1, kV4L2SensitivityDenominator));
  std::multimap<int32_t, uint8_t> ae_mode_mapping = {
      {V4L2_EXPOSURE_AUTO, ANDROID_CONTROL_AE_MODE_ON}};
  if (exposure_time && sensitivity) {
    // TODO(b/30510395): as part of coordinated 3A component,
    // if these aren't available don't advertise AE mode OFF, only AUTO.
    components.insert(std::move(exposure_time));
    components.insert(std::move(sensitivity));
    ae_mode_mapping.emplace(V4L2_EXPOSURE_MANUAL, ANDROID_CONTROL_AE_MODE_OFF);
  }
  components.insert(V4L2ControlOrDefault<uint8_t>(
      ControlType::kMenu,
      ANDROID_CONTROL_AE_MODE,
      ANDROID_CONTROL_AE_AVAILABLE_MODES,
      device,
      V4L2_CID_EXPOSURE_AUTO,
      std::shared_ptr<ConverterInterface<uint8_t, int32_t>>(
          new EnumConverter(ae_mode_mapping)),
      ANDROID_CONTROL_AE_MODE_ON,
      {{CAMERA3_TEMPLATE_MANUAL, ANDROID_CONTROL_AE_MODE_OFF},
       {OTHER_TEMPLATES, ANDROID_CONTROL_AE_MODE_ON}}));
  // Can't get AE status from V4L2.
  // TODO(b/30510395): If AE mode is OFF, this should switch to INACTIVE.
  components.insert(FixedState<uint8_t>(ANDROID_CONTROL_AE_STATE,
                                        ANDROID_CONTROL_AE_STATE_CONVERGED));
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
           {V4L2_WHITE_BALANCE_SHADE, ANDROID_CONTROL_AWB_MODE_SHADE}})),
      {{CAMERA3_TEMPLATE_MANUAL, ANDROID_CONTROL_AWB_MODE_OFF},
       {OTHER_TEMPLATES, ANDROID_CONTROL_AWB_MODE_AUTO}}));
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
        ANDROID_CONTROL_AWB_MODE_AUTO,
        {{CAMERA3_TEMPLATE_MANUAL, ANDROID_CONTROL_AWB_MODE_OFF},
         {OTHER_TEMPLATES, ANDROID_CONTROL_AWB_MODE_AUTO}}));
  }
  // TODO(b/31041577): Handle AWB state machine correctly.
  components.insert(FixedState<uint8_t>(ANDROID_CONTROL_AWB_STATE,
                                        ANDROID_CONTROL_AWB_STATE_CONVERGED));
  // TODO(b/31022153): 3A locks.
  components.insert(std::unique_ptr<PartialMetadataInterface>(
      new Property<uint8_t>(ANDROID_CONTROL_AE_LOCK_AVAILABLE,
                            ANDROID_CONTROL_AE_LOCK_AVAILABLE_FALSE)));
  components.insert(
      NoEffectMenuControl<uint8_t>(ANDROID_CONTROL_AE_LOCK,
                                   DO_NOT_REPORT_OPTIONS,
                                   {ANDROID_CONTROL_AE_LOCK_OFF}));
  components.insert(std::unique_ptr<PartialMetadataInterface>(
      new Property<uint8_t>(ANDROID_CONTROL_AWB_LOCK_AVAILABLE,
                            ANDROID_CONTROL_AWB_LOCK_AVAILABLE_FALSE)));
  components.insert(
      NoEffectMenuControl<uint8_t>(ANDROID_CONTROL_AWB_LOCK,
                                   DO_NOT_REPORT_OPTIONS,
                                   {ANDROID_CONTROL_AWB_LOCK_OFF}));
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
  // TODO(b/31022612): Scene mode overrides.
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
  // TODO(b/31021654): This should behave as a top level switch, not no effect.
  // Should enforce being set to USE_SCENE_MODE when a scene mode is requested.
  components.insert(NoEffectMenuControl<uint8_t>(
      ANDROID_CONTROL_MODE,
      ANDROID_CONTROL_AVAILABLE_MODES,
      {ANDROID_CONTROL_MODE_AUTO, ANDROID_CONTROL_MODE_USE_SCENE_MODE}));

  // Not sure if V4L2 does or doesn't do this, but HAL documentation says
  // all devices must support FAST, and FAST can be equivalent to OFF, so
  // either way it's fine to list. And if FAST is included, HIGH_QUALITY
  // is supposed to be included as well.
  components.insert(NoEffectMenuControl<uint8_t>(
      ANDROID_EDGE_MODE,
      ANDROID_EDGE_AVAILABLE_EDGE_MODES,
      {ANDROID_EDGE_MODE_FAST, ANDROID_EDGE_MODE_HIGH_QUALITY},
      {{CAMERA3_TEMPLATE_STILL_CAPTURE, ANDROID_EDGE_MODE_HIGH_QUALITY},
       {OTHER_TEMPLATES, ANDROID_EDGE_MODE_FAST}}));

  // TODO(b/31023454): subcomponents of flash.
  components.insert(
      std::unique_ptr<PartialMetadataInterface>(new Property<uint8_t>(
          ANDROID_FLASH_INFO_AVAILABLE, ANDROID_FLASH_INFO_AVAILABLE_FALSE)));
  components.insert(FixedState<uint8_t>(ANDROID_FLASH_STATE,
                                        ANDROID_FLASH_STATE_UNAVAILABLE));
  components.insert(NoEffectMenuControl<uint8_t>(
      ANDROID_FLASH_MODE, DO_NOT_REPORT_OPTIONS, {ANDROID_FLASH_MODE_OFF}));

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
  // Always assume external-facing.
  components.insert(
      std::unique_ptr<PartialMetadataInterface>(new Property<uint8_t>(
          ANDROID_LENS_FACING, ANDROID_LENS_FACING_EXTERNAL)));
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
  // TODO(b/31022711): Focus distance component.
  // Using a NoEffectMenuControl for now because for
  // fixed-focus it meets expectations. Framework may allow
  // setting any value and expect it to be clamped to 0, in which
  // case this will have unexpected behavior (failing on non-0 settings).
  components.insert(
      NoEffectMenuControl<float>(ANDROID_LENS_FOCUS_DISTANCE,
                                 ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE,
                                 {0}));
  // Hypefocal distance doesn't mean much for a fixed-focus uncalibrated device.
  components.insert(std::make_unique<Property<float>>(
      ANDROID_LENS_INFO_HYPERFOCAL_DISTANCE, 0));

  // No way to know when the lens is moving or not in V4L2.
  components.insert(
      FixedState<uint8_t>(ANDROID_LENS_STATE, ANDROID_LENS_STATE_STATIONARY));
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
  // TODO(b/31017806): This should definitely have a different default depending
  // on template.
  components.insert(NoEffectMenuControl<uint8_t>(
      ANDROID_CONTROL_CAPTURE_INTENT,
      DO_NOT_REPORT_OPTIONS,
      {ANDROID_CONTROL_CAPTURE_INTENT_CUSTOM,
       ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW,
       ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE,
       ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_RECORD,
       ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_SNAPSHOT,
       ANDROID_CONTROL_CAPTURE_INTENT_ZERO_SHUTTER_LAG,
       ANDROID_CONTROL_CAPTURE_INTENT_MANUAL},
      {{CAMERA3_TEMPLATE_PREVIEW, ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW},
       {CAMERA3_TEMPLATE_STILL_CAPTURE,
        ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE},
       {CAMERA3_TEMPLATE_VIDEO_RECORD,
        ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_RECORD},
       {CAMERA3_TEMPLATE_VIDEO_SNAPSHOT,
        ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_SNAPSHOT},
       {CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG,
        ANDROID_CONTROL_CAPTURE_INTENT_ZERO_SHUTTER_LAG},
       {CAMERA3_TEMPLATE_MANUAL, ANDROID_CONTROL_CAPTURE_INTENT_MANUAL},
       {OTHER_TEMPLATES, ANDROID_CONTROL_CAPTURE_INTENT_CUSTOM}}));

  // Unable to control noise reduction in V4L2 devices,
  // but FAST is allowed to be the same as OFF,
  // and HIGH_QUALITY can be the same as FAST.
  components.insert(NoEffectMenuControl<uint8_t>(
      ANDROID_NOISE_REDUCTION_MODE,
      ANDROID_NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES,
      {ANDROID_NOISE_REDUCTION_MODE_FAST,
       ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY},
      {{CAMERA3_TEMPLATE_STILL_CAPTURE,
        ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY},
       {OTHER_TEMPLATES, ANDROID_NOISE_REDUCTION_MODE_FAST}}));

  // TODO(30510395): subcomponents of formats/streams.
  // For now, no thumbnails available (only [0,0], the "no thumbnail" size).
  // TODO(b/29580107): Could end up with a mismatch between request & result,
  // since V4L2 doesn't actually allow for thumbnail size control.
  components.insert(NoEffectMenuControl<std::array<int32_t, 2>>(
      ANDROID_JPEG_THUMBNAIL_SIZE,
      ANDROID_JPEG_AVAILABLE_THUMBNAIL_SIZES,
      {{{0, 0}}}));
  // TODO(b/31022752): Get this from the device, not constant.
  components.insert(std::unique_ptr<PartialMetadataInterface>(
      new Property<int32_t>(ANDROID_JPEG_MAX_SIZE, kV4L2MaxJpegSize)));
  // TODO(b/31021672): Other JPEG controls (GPS, quality, orientation).
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
  components.insert(FixedState<uint8_t>(ANDROID_REQUEST_PIPELINE_DEPTH, 4));
  // "LIMITED devices are strongly encouraged to use a non-negative value.
  // If UNKNOWN is used here then app developers do not have a way to know
  // when sensor settings have been applied." - Unfortunately, V4L2 doesn't
  // really help here either. Could even be that adjusting settings mid-stream
  // blocks in V4L2, and should be avoided.
  components.insert(
      std::unique_ptr<PartialMetadataInterface>(new Property<int32_t>(
          ANDROID_SYNC_MAX_LATENCY, ANDROID_SYNC_MAX_LATENCY_UNKNOWN)));
  // Never know when controls are synced.
  components.insert(FixedState<int64_t>(ANDROID_SYNC_FRAME_NUMBER,
                                        ANDROID_SYNC_FRAME_NUMBER_UNKNOWN));

  // TODO(b/31022480): subcomponents of cropping/sensors.
  // Need ANDROID_SCALER_CROP_REGION control support.
  // V4L2 VIDIOC_CROPCAP doesn't give a way to query this;
  // it's driver dependent. For now, assume freeform, and
  // some cameras may just behave badly.
  // TODO(b/29579652): Figure out a way to determine this.
  components.insert(std::unique_ptr<PartialMetadataInterface>(
      new Property<float>(ANDROID_SCALER_AVAILABLE_MAX_DIGITAL_ZOOM, 1)));
  components.insert(std::unique_ptr<PartialMetadataInterface>(
      new Property<uint8_t>(ANDROID_SCALER_CROPPING_TYPE,
                            ANDROID_SCALER_CROPPING_TYPE_FREEFORM)));
  // Spoof pixel array size for now, eventually get from CROPCAP.
  std::array<int32_t, 2> pixel_array_size = {{3280, 2464}};
  components.insert(std::unique_ptr<PartialMetadataInterface>(
      new Property<std::array<int32_t, 2>>(ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE,
                                           pixel_array_size)));
  // Active array size is {x-offset, y-offset, width, height}, relative to
  // the pixel array size, with {0, 0} being the top left. Since there's no way
  // to get this in V4L2, assume the full pixel array is the active array.
  std::array<int32_t, 4> active_array_size = {
      {0, 0, pixel_array_size[0], pixel_array_size[1]}};
  components.insert(std::unique_ptr<PartialMetadataInterface>(
      new Property<std::array<int32_t, 4>>(
          ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE, active_array_size)));
  // This is really more freeform than a menu control, but since we're
  // restricting it to not being used anyways this works for now.
  components.insert(NoEffectMenuControl<std::array<int32_t, 4>>(
      ANDROID_SCALER_CROP_REGION, DO_NOT_REPORT_OPTIONS, {active_array_size}));
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
  components.insert(std::make_unique<State<int64_t>>(
      ANDROID_SENSOR_TIMESTAMP, std::make_unique<BoottimeStateDelegate>()));
  // No way to actually get shutter skew from V4L2.
  components.insert(
      FixedState<int64_t>(ANDROID_SENSOR_ROLLING_SHUTTER_SKEW, 0));
  // No way to actually get orientation from V4L2.
  components.insert(std::unique_ptr<PartialMetadataInterface>(
      new Property<int32_t>(ANDROID_SENSOR_ORIENTATION, 0)));
  // TODO(b/31023611): Sensor frame duration. Range should
  // be dependent on the stream configuration being used.
  // No test patterns supported.
  components.insert(
      NoEffectMenuControl<int32_t>(ANDROID_SENSOR_TEST_PATTERN_MODE,
                                   ANDROID_SENSOR_AVAILABLE_TEST_PATTERN_MODES,
                                   {ANDROID_SENSOR_TEST_PATTERN_MODE_OFF}));

  // TODO(b/30510395): subcomponents of face detection.
  // Face detection not supported.
  components.insert(NoEffectMenuControl<uint8_t>(
      ANDROID_STATISTICS_FACE_DETECT_MODE,
      ANDROID_STATISTICS_INFO_AVAILABLE_FACE_DETECT_MODES,
      {ANDROID_STATISTICS_FACE_DETECT_MODE_OFF}));
  components.insert(std::unique_ptr<PartialMetadataInterface>(
      new Property<int32_t>(ANDROID_STATISTICS_INFO_MAX_FACE_COUNT, 0)));

  // No way to get detected scene flicker from V4L2.
  components.insert(FixedState<uint8_t>(ANDROID_STATISTICS_SCENE_FLICKER,
                                        ANDROID_STATISTICS_SCENE_FLICKER_NONE));

  // TOOD(b/31023265): V4L2_CID_FLASH_INDICATOR_INTENSITY could be queried
  // to see if there's a transmit LED. Would need to translate HAL off/on
  // enum to slider min/max value. For now, no LEDs available.
  components.insert(std::unique_ptr<PartialMetadataInterface>(
      new Property<uint8_t>(ANDROID_LED_AVAILABLE_LEDS, {})));

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

  // Request is unused, and can be any value,
  // but that value needs to be propagated.
  components.insert(NoEffectOptionlessControl<int32_t>(ANDROID_REQUEST_ID, 0));

  // Metadata is returned in a single result; not multiple pieces.
  components.insert(std::make_unique<Property<int32_t>>(
      ANDROID_REQUEST_PARTIAL_RESULT_COUNT, 1));

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
