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

#include "V4L2Camera.h"

#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cstdlib>

#include <camera/CameraMetadata.h>
#include <hardware/camera3.h>
#include <nativehelper/ScopedFd.h>

#include "Common.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

namespace v4l2_camera_hal {

V4L2Camera::V4L2Camera(int id, std::string path)
    : default_camera_hal::Camera(id),
      mDevicePath(std::move(path)),
      mTemplatesInitialized(false),
      mCharacteristicsInitialized(false) {
  HAL_LOG_ENTER();
}

V4L2Camera::~V4L2Camera() {
  HAL_LOG_ENTER();
}

int V4L2Camera::connect() {
  HAL_LOG_ENTER();

  if (mDeviceFd.get() >= 0) {
    HAL_LOGE("Camera device %s is opened. Close it first", mDevicePath.c_str());
    return -EIO;
  }

  int fd = TEMP_FAILURE_RETRY(open(mDevicePath.c_str(), O_RDWR));
  if (fd < 0) {
    HAL_LOGE("failed to open %s (%s)", mDevicePath.c_str(), strerror(errno));
    return -errno;
  }
  mDeviceFd.reset(fd);

  // TODO(b/29185945): confirm this is a supported device.
  // This is checked by the HAL, but the device at mDevicePath may
  // not be the same one that was there when the HAL was loaded.
  // (Alternatively, better hotplugging support may make this unecessary
  // by disabling cameras that get disconnected and checking newly connected
  // cameras, so connect() is never called on an unsupported camera)

  // TODO(b/29158098): Inform service of any flashes that are no longer available
  // because this camera is in use.
  return 0;
}

void V4L2Camera::disconnect() {
  HAL_LOG_ENTER();
  // TODO(b/29158098): Inform service of any flashes that are available again
  // because this camera is no longer in use.

  mDeviceFd.reset();
}

int V4L2Camera::initStaticInfo(camera_metadata_t** out) {
  HAL_LOG_ENTER();

  android::status_t res;
  // Device characteristics need to be queried prior
  // to static info setup.
  if (!mCharacteristicsInitialized) {
    res = initCharacteristics();
    if (res) {
      return res;
    }
  }

  android::CameraMetadata info;

  std::vector<int32_t> avail_characteristics_keys;

#define ADD_STATIC_ENTRY(name, varptr, count) \
  avail_characteristics_keys.push_back(name); \
  res = info.update(name, varptr, count); \
  if (res != android::OK) return res

  // Static metadata characteristics from /system/media/camera/docs/docs.html.

  /* android.colorCorrection. */

  // No easy way to turn chromatic aberration correction OFF in v4l2,
  // though this may be supportable via a collection of other user controls.
  uint8_t avail_aberration_modes[] = {
    ANDROID_COLOR_CORRECTION_ABERRATION_MODE_FAST,
    ANDROID_COLOR_CORRECTION_ABERRATION_MODE_HIGH_QUALITY};
  ADD_STATIC_ENTRY(ANDROID_COLOR_CORRECTION_AVAILABLE_ABERRATION_MODES,
                   avail_aberration_modes, ARRAY_SIZE(avail_aberration_modes));

  /* android.control. */

  /*   3As */

  ADD_STATIC_ENTRY(ANDROID_CONTROL_AE_AVAILABLE_ANTIBANDING_MODES,
                   mAeAntibandingModes.data(), mAeAntibandingModes.size());

  ADD_STATIC_ENTRY(ANDROID_CONTROL_AE_AVAILABLE_MODES,
                   mAeModes.data(), mAeModes.size());

  // Flatten mFpsRanges.
  ADD_STATIC_ENTRY(ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES,
                   mFpsRanges.data(), mFpsRanges.total_num_elements());

  ADD_STATIC_ENTRY(ANDROID_CONTROL_AE_COMPENSATION_RANGE,
                   mAeCompensationRange.data(), mAeCompensationRange.size());

  ADD_STATIC_ENTRY(ANDROID_CONTROL_AE_COMPENSATION_STEP,
                   &mAeCompensationStep, 1);

  ADD_STATIC_ENTRY(ANDROID_CONTROL_AF_AVAILABLE_MODES,
                   mAfModes.data(), mAfModes.size());

  ADD_STATIC_ENTRY(ANDROID_CONTROL_AWB_AVAILABLE_MODES,
                   mAwbModes.data(), mAwbModes.size());

  // Couldn't find any V4L2 support for regions, though maybe it's out there.
  int32_t max_regions[] = {/*AE*/ 0,/*AWB*/ 0,/*AF*/ 0};
  ADD_STATIC_ENTRY(ANDROID_CONTROL_MAX_REGIONS,
                   max_regions, ARRAY_SIZE(max_regions));

  ADD_STATIC_ENTRY(ANDROID_CONTROL_AE_LOCK_AVAILABLE,
                   &mAeLockAvailable, 1);
  ADD_STATIC_ENTRY(ANDROID_CONTROL_AWB_LOCK_AVAILABLE,
                   &mAwbLockAvailable, 1);

  /*   Scene modes. */

  ADD_STATIC_ENTRY(ANDROID_CONTROL_AVAILABLE_SCENE_MODES,
                   mSceneModes.data(), mSceneModes.size());

  // A 3-tuple of AE, AWB, AF overrides for each scene mode.
  // Ignored for DISABLED, FACE_PRIORITY and FACE_PRIORITY_LOW_LIGHT.
  uint8_t scene_mode_overrides[] = {
    /*SCENE_MODE_DISABLED*/ /*AE*/0, /*AW*/0, /*AF*/0};
  ADD_STATIC_ENTRY(ANDROID_CONTROL_SCENE_MODE_OVERRIDES,
                   scene_mode_overrides, ARRAY_SIZE(scene_mode_overrides));

  /*   Top level 3A/Scenes switch. */

  ADD_STATIC_ENTRY(ANDROID_CONTROL_AVAILABLE_MODES,
                   mControlModes.data(), mControlModes.size());

  /*   Other android.control configuration. */

  ADD_STATIC_ENTRY(ANDROID_CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES,
                   mVideoStabilizationModes.data(),
                   mVideoStabilizationModes.size());

  ADD_STATIC_ENTRY(ANDROID_CONTROL_AVAILABLE_EFFECTS,
                   mEffects.data(), mEffects.size());

  // AVAILABLE_HIGH_SPEED_VIDEO_CONFIGURATIONS only necessary
  // for devices supporting CONSTRAINED_HIGH_SPEED_VIDEO,
  // which this HAL doesn't support.

  // POST_RAW_SENSITIVITY_BOOST_RANGE only necessary
  // for devices supporting RAW format outputs.

  /* android.edge. */

  // Not sure if V4L2 does or doesn't do this, but HAL documentation says
  // all devices must support FAST, and FAST can be equivalent to OFF, so
  // either way it's fine to list.
  uint8_t avail_edge_modes[] = {
    ANDROID_EDGE_MODE_FAST};
  ADD_STATIC_ENTRY(ANDROID_EDGE_AVAILABLE_EDGE_MODES,
                   avail_edge_modes, ARRAY_SIZE(avail_edge_modes));

  /* android.flash. */

  ADD_STATIC_ENTRY(ANDROID_FLASH_INFO_AVAILABLE,
                   &mFlashAvailable, 1);

  // info.chargeDuration, color.Temperature, maxEnergy marked FUTURE.

  /* android.hotPixel. */

  // No known V4L2 hot pixel correction. But it might be happening,
  // so we report FAST/HIGH_QUALITY.
  uint8_t avail_hot_pixel_modes[] = {
    ANDROID_HOT_PIXEL_MODE_FAST,
    ANDROID_HOT_PIXEL_MODE_HIGH_QUALITY};
  ADD_STATIC_ENTRY(ANDROID_HOT_PIXEL_AVAILABLE_HOT_PIXEL_MODES,
                   avail_hot_pixel_modes, ARRAY_SIZE(avail_hot_pixel_modes));

  /* android.jpeg. */

  // For now, no thumbnails available (only [0,0], the "no thumbnail" size).
  // TODO(b/29580107): Could end up with a mismatch between request & result,
  // since V4L2 doesn't actually allow for thumbnail size control.
  int32_t thumbnail_sizes[] = {
    0, 0};
  ADD_STATIC_ENTRY(ANDROID_JPEG_AVAILABLE_THUMBNAIL_SIZES,
                   thumbnail_sizes, ARRAY_SIZE(thumbnail_sizes));

  // V4L2 doesn't support querying this, so we generously assume up to 3 MB.
  int32_t max_jpeg_size = 3000000;
  ADD_STATIC_ENTRY(ANDROID_JPEG_MAX_SIZE,
                   &max_jpeg_size, 1);

  /* android.lens. */

  /*   Misc. lens control. */

  ADD_STATIC_ENTRY(ANDROID_LENS_INFO_AVAILABLE_APERTURES,
                   &mAperture, 1);

  ADD_STATIC_ENTRY(ANDROID_LENS_INFO_AVAILABLE_FILTER_DENSITIES,
                   &mFilterDensity, 1);

  ADD_STATIC_ENTRY(ANDROID_LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION,
                   mOpticalStabilizationModes.data(),
                   mOpticalStabilizationModes.size());

  // No known V4L2 shading map info.
  int32_t shading_map_size[] = {1, 1};
  ADD_STATIC_ENTRY(ANDROID_LENS_INFO_SHADING_MAP_SIZE,
                   shading_map_size, ARRAY_SIZE(shading_map_size));

  // All V4L2 devices are considered to be external facing.
  uint8_t facing = ANDROID_LENS_FACING_EXTERNAL;
  ADD_STATIC_ENTRY(ANDROID_LENS_FACING, &facing, 1);

  /*   Zoom/Focus. */

  // No way to actually get the focal length in V4L2, but it's a required key,
  // so we just fake it.
  ADD_STATIC_ENTRY(ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS,
                   &mFocalLength, 1);

  // V4L2 focal units do not correspond to a particular physical unit.
  uint8_t focus_calibration =
      ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_UNCALIBRATED;
  ADD_STATIC_ENTRY(ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION,
                   &focus_calibration, 1);

  // info.hyperfocalDistance not required for UNCALIBRATED.

  ADD_STATIC_ENTRY(ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE,
                   &mFocusDistance, 1);

  /*   Depth. */

  // DEPTH capability not supported by this HAL. Not implemented:
  // poseRotation
  // poseTranslation
  // intrinsicCalibration
  // radialDistortion

  /* anroid.noise. */

  // Unable to control noise reduction in V4L2 devices,
  // but FAST is allowed to be the same as OFF.
  uint8_t avail_noise_reduction_modes[] = {ANDROID_NOISE_REDUCTION_MODE_FAST};
  ADD_STATIC_ENTRY(ANDROID_NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES,
                   avail_noise_reduction_modes,
                   ARRAY_SIZE(avail_noise_reduction_modes));

  /* android.request. */

  // Resources may be an issue, so just using minimum allowable
  // for LIMITED devices.
  int32_t max_num_output_streams[] = {
    /*Raw*/0, /*YUV*/2, /*JPEG*/1};
  ADD_STATIC_ENTRY(ANDROID_REQUEST_MAX_NUM_OUTPUT_STREAMS,
                   max_num_output_streams, ARRAY_SIZE(max_num_output_streams));

  // Reprocessing not supported, so no maxNumInputStreams.

  // No way to know for V4L2, so fake with max allowable latency.
  // Doesn't mean much without per-frame controls.
  uint8_t pipeline_max_depth = 4;
  ADD_STATIC_ENTRY(ANDROID_REQUEST_PIPELINE_MAX_DEPTH,
                   &pipeline_max_depth, 1);

  // Partial results not supported; partialResultCount defaults to 1.

  // Available capabilities & keys queried at very end of this method.

  /* android.scaler. */

  /*   Cropping. */

  ADD_STATIC_ENTRY(ANDROID_SCALER_AVAILABLE_MAX_DIGITAL_ZOOM,
                   &mMaxZoom, 1);

  ADD_STATIC_ENTRY(ANDROID_SCALER_CROPPING_TYPE, &mCropType, 1);

  /*   Streams. */

  // availableInputOutputFormatsMap only required for reprocessing capability.

  // Flatten mStreamConfigs.
  ADD_STATIC_ENTRY(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
                   mStreamConfigs.data(),
                   mStreamConfigs.total_num_elements());

  // Flatten mMinFrameDurations.
  ADD_STATIC_ENTRY(ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS,
                   mMinFrameDurations.data(),
                   mMinFrameDurations.total_num_elements());

  // Flatten mStallDurations.
  ADD_STATIC_ENTRY(ANDROID_SCALER_AVAILABLE_STALL_DURATIONS,
                   mStallDurations.data(),
                   mStallDurations.total_num_elements());

  /* android.sensor. */

  /*   Sizes. */

  ADD_STATIC_ENTRY(ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE,
                   mPixelArraySize.data(), mPixelArraySize.size());
  // No V4L2 way to differentiate active vs. inactive parts of the rectangle.
  ADD_STATIC_ENTRY(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE,
                   mPixelArraySize.data(), mPixelArraySize.size());

  ADD_STATIC_ENTRY(ANDROID_SENSOR_INFO_PHYSICAL_SIZE,
                   mPhysicalSize.data(), mPhysicalSize.size());

  /*   Misc sensor information. */

  ADD_STATIC_ENTRY(ANDROID_SENSOR_INFO_MAX_FRAME_DURATION,
                   &mMaxFrameDuration, 1);

  // HAL uses BOOTTIME timestamps.
  // TODO(b/29457051): make sure timestamps are consistent throughout the HAL.
  uint8_t timestamp_source = ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE_UNKNOWN;
  ADD_STATIC_ENTRY(ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE,
                   &timestamp_source, 1);

  // As in initDeviceInfo, no way to actually get orientation.
  ADD_STATIC_ENTRY(ANDROID_SENSOR_ORIENTATION, &mOrientation, 1);

  // availableTestPatternModes just defaults to OFF, which is fine.

  // info.exposureTimeRange, info.sensitivityRange:
  // exposure/sensitivity manual control not supported.
  // Could query V4L2_CID_ISO_SENSITIVITY to support sensitivity if desired.

  // info.whiteLevel, info.lensShadingApplied,
  // info.preCorrectionPixelArraySize, referenceIlluminant1/2,
  // calibrationTransform1/2, colorTransform1/2, forwardMatrix1/2,
  // blackLevelPattern, profileHueSatMapDimensions
  // all only necessary for RAW.

  // baseGainFactor marked FUTURE.

  // maxAnalogSensitivity optional for LIMITED device.

  // opticalBlackRegions: No known way to get in V4L2, but not necessary.

  // opaqueRawSize not necessary since RAW_OPAQUE format not supported.

  /* android.shading */

  // No known V4L2 lens shading. But it might be happening,
  // so we report FAST/HIGH_QUALITY.
  uint8_t avail_shading_modes[] = {
    ANDROID_SHADING_MODE_FAST,
    ANDROID_SHADING_MODE_HIGH_QUALITY};
  ADD_STATIC_ENTRY(ANDROID_SHADING_AVAILABLE_MODES,
                   avail_shading_modes, ARRAY_SIZE(avail_shading_modes));

  /* android.statistics */

  // Face detection not supported.
  uint8_t avail_face_detect_modes[] = {
    ANDROID_STATISTICS_FACE_DETECT_MODE_OFF};
  ADD_STATIC_ENTRY(ANDROID_STATISTICS_INFO_AVAILABLE_FACE_DETECT_MODES,
                   avail_face_detect_modes,
                   ARRAY_SIZE(avail_face_detect_modes));

  int32_t max_face_count = 0;
  ADD_STATIC_ENTRY(ANDROID_STATISTICS_INFO_MAX_FACE_COUNT,
                   &max_face_count, 1);

  // info.histogramBucketCount, info.maxHistogramCount,
  // info.maxSharpnessMapValue, info.sharpnessMapSizemarked FUTURE.

  // ON only needs to be supported for RAW capable devices.
  uint8_t avail_hot_pixel_map_modes[] = {
    ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE_OFF};
  ADD_STATIC_ENTRY(ANDROID_STATISTICS_INFO_AVAILABLE_HOT_PIXEL_MAP_MODES,
                   avail_hot_pixel_map_modes,
                   ARRAY_SIZE(avail_hot_pixel_map_modes));

  // ON only needs to be supported for RAW capable devices.
  uint8_t avail_lens_shading_map_modes[] = {
    ANDROID_STATISTICS_LENS_SHADING_MAP_MODE_OFF};
  ADD_STATIC_ENTRY(ANDROID_STATISTICS_INFO_AVAILABLE_LENS_SHADING_MAP_MODES,
                   avail_lens_shading_map_modes,
                   ARRAY_SIZE(avail_lens_shading_map_modes));

  /* android.tonemap. */

  // tonemapping only required for MANUAL_POST_PROCESSING capability.

  /* android.led. */

  // May or may not have LEDs available.
  if (!mLeds.empty()) {
    ADD_STATIC_ENTRY(ANDROID_LED_AVAILABLE_LEDS, mLeds.data(), mLeds.size());
  }

  /* android.sync. */

  // "LIMITED devices are strongly encouraged to use a non-negative value.
  // If UNKNOWN is used here then app developers do not have a way to know
  // when sensor settings have been applied." - Unfortunately, V4L2 doesn't
  // really help here either. Could even be that adjusting settings mid-stream
  // blocks in V4L2, and should be avoided.
  int32_t max_latency = ANDROID_SYNC_MAX_LATENCY_UNKNOWN;
  ADD_STATIC_ENTRY(ANDROID_SYNC_MAX_LATENCY,
                   &max_latency, 1);

  /* android.reprocess. */

  // REPROCESSING not supported by this HAL.

  /* android.depth. */

  // DEPTH not supported by this HAL.

  /* Capabilities and android.info. */

  uint8_t hw_level = ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_LIMITED;
  ADD_STATIC_ENTRY(ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL,
                   &hw_level, 1);

  uint8_t capabilities[] = {
    ANDROID_REQUEST_AVAILABLE_CAPABILITIES_BACKWARD_COMPATIBLE};
  ADD_STATIC_ENTRY(ANDROID_REQUEST_AVAILABLE_CAPABILITIES,
                   capabilities, ARRAY_SIZE(capabilities));

  // Scan a default request template for included request keys.
  if (!mTemplatesInitialized) {
    res = initTemplates();
    if (res) {
      return res;
    }
  }
  std::vector<int32_t> avail_request_keys;
  const camera_metadata_t *preview_request = nullptr;
  // Search templates from the beginning for a supported one.
  for (uint8_t template_id = 1; template_id < CAMERA3_TEMPLATE_COUNT;
       ++template_id) {
    preview_request = constructDefaultRequestSettings(template_id);
    if (preview_request != nullptr) {
      break;
    }
  }
  if (preview_request == nullptr) {
    HAL_LOGE("No valid templates, can't get request keys.");
    return -ENODEV;
  }
  size_t num_entries = get_camera_metadata_entry_count(preview_request);
  for (size_t i = 0; i < num_entries; ++i) {
    camera_metadata_ro_entry_t entry;
    get_camera_metadata_ro_entry(preview_request, i, &entry);
    avail_request_keys.push_back(entry.tag);
  }
  ADD_STATIC_ENTRY(ANDROID_REQUEST_AVAILABLE_REQUEST_KEYS,
                   avail_request_keys.data(), avail_request_keys.size());

  // Result keys will be duplicated from the request, plus a few extras.
  // TODO(b/29335262): additonal available result keys.
  std::vector<int32_t> avail_result_keys(avail_request_keys);
  ADD_STATIC_ENTRY(ANDROID_REQUEST_AVAILABLE_RESULT_KEYS,
                   avail_result_keys.data(), avail_result_keys.size());

  // Last thing, once all the available characteristics have been added.
  info.update(ANDROID_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS,
              avail_characteristics_keys.data(),
              avail_characteristics_keys.size());

  *out = info.release();
  return 0;
}

void V4L2Camera::initDeviceInfo(camera_info_t* info) {
  HAL_LOG_ENTER();

  // For now, just constants.
  info->facing = CAMERA_FACING_EXTERNAL;
  info->orientation = mOrientation;
  info->resource_cost = 100;
  info->conflicting_devices = nullptr;
  info->conflicting_devices_length = 0;
}

int V4L2Camera::initDevice() {
  HAL_LOG_ENTER();
  int res;

  // Templates should be set up if they haven't already been.
  if (!mTemplatesInitialized) {
    res = initTemplates();
    if (res) {
      return res;
    }
  }

  return 0;
}

int V4L2Camera::initTemplates() {
  HAL_LOG_ENTER();
  int res;

  // Device characteristics need to be queried prior
  // to template setup.
  if (!mCharacteristicsInitialized) {
    res = initCharacteristics();
    if (res) {
      return res;
    }
  }

  // Note: static metadata expects all templates/requests
  // to provide values for all supported keys.

  android::CameraMetadata template_metadata;
#define ADD_REQUEST_ENTRY(name, varptr, count) \
  res = template_metadata.update(name, varptr, count); \
  if (res != android::OK) return res

  // Start with defaults for all templates.

  /* android.colorCorrection. */

  uint8_t aberration_mode = ANDROID_COLOR_CORRECTION_ABERRATION_MODE_FAST;
  ADD_REQUEST_ENTRY(ANDROID_COLOR_CORRECTION_ABERRATION_MODE,
                    &aberration_mode, 1);

  uint8_t color_correction_mode = ANDROID_COLOR_CORRECTION_MODE_FAST;
  ADD_REQUEST_ENTRY(ANDROID_COLOR_CORRECTION_MODE,
                    &color_correction_mode, 1);

  // transform and gains are for the unsupported MANUAL_POST_PROCESSING only.

  /* android.control. */

  /*   AE. */
  uint8_t ae_antibanding_mode = ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO;
  ADD_REQUEST_ENTRY(ANDROID_CONTROL_AE_ANTIBANDING_MODE,
                    &ae_antibanding_mode, 1);

  // Only matters if AE_MODE = OFF
  int32_t ae_exposure_compensation = 0;
  ADD_REQUEST_ENTRY(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION,
                    &ae_exposure_compensation, 1);

  uint8_t ae_lock = ANDROID_CONTROL_AE_LOCK_OFF;
  ADD_REQUEST_ENTRY(ANDROID_CONTROL_AE_LOCK, &ae_lock, 1);

  uint8_t ae_mode = ANDROID_CONTROL_AE_MODE_ON;
  ADD_REQUEST_ENTRY(ANDROID_CONTROL_AE_MODE, &ae_mode, 1);

  // AE regions not supported.

  // FPS set per-template.

  uint8_t ae_precapture_trigger = ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_IDLE;
  ADD_REQUEST_ENTRY(ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER,
                    &ae_precapture_trigger, 1);

  /*   AF. */

  // AF mode set per-template.

  // AF regions not supported.

  uint8_t af_trigger = ANDROID_CONTROL_AF_TRIGGER_IDLE;
  ADD_REQUEST_ENTRY(ANDROID_CONTROL_AF_TRIGGER, &af_trigger, 1);

  /*   AWB. */

  // Priority: auto > off > Whatever is available.
  uint8_t default_awb_mode = mAwbModes[0];
  if (std::count(mAwbModes.begin(), mAwbModes.end(),
                ANDROID_CONTROL_AWB_MODE_AUTO)) {
    default_awb_mode = ANDROID_CONTROL_AWB_MODE_AUTO;
  } else if (std::count(mAwbModes.begin(), mAwbModes.end(),
                       ANDROID_CONTROL_AWB_MODE_OFF)) {
    default_awb_mode = ANDROID_CONTROL_AWB_MODE_OFF;
  }
  ADD_REQUEST_ENTRY(ANDROID_CONTROL_AWB_MODE, &default_awb_mode, 1);

  // AWB regions not supported.

  /*   Other controls. */

  uint8_t effect_mode = ANDROID_CONTROL_EFFECT_MODE_OFF;
  ADD_REQUEST_ENTRY(ANDROID_CONTROL_EFFECT_MODE, &effect_mode, 1);

  uint8_t control_mode = ANDROID_CONTROL_MODE_AUTO;
  ADD_REQUEST_ENTRY(ANDROID_CONTROL_MODE, &control_mode, 1);

  uint8_t scene_mode = ANDROID_CONTROL_SCENE_MODE_DISABLED;
  ADD_REQUEST_ENTRY(ANDROID_CONTROL_SCENE_MODE,
                    &scene_mode, 1);

  uint8_t video_stabilization = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_OFF;
  ADD_REQUEST_ENTRY(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE,
                    &video_stabilization, 1);

  // postRawSensitivityBoost: RAW not supported, leave null.

  /* android.demosaic. */

  // mode marked FUTURE.

  /* android.edge. */

  uint8_t edge_mode = ANDROID_EDGE_MODE_FAST;
  ADD_REQUEST_ENTRY(ANDROID_EDGE_MODE, &edge_mode, 1);

  // strength marked FUTURE.

  /* android.flash. */

  // firingPower, firingTime marked FUTURE.

  uint8_t flash_mode = ANDROID_FLASH_MODE_OFF;
  ADD_REQUEST_ENTRY(ANDROID_FLASH_MODE, &flash_mode, 1);

  /* android.hotPixel. */

  uint8_t hp_mode = ANDROID_HOT_PIXEL_MODE_FAST;
  ADD_REQUEST_ENTRY(ANDROID_HOT_PIXEL_MODE, &hp_mode, 1);

  /* android.jpeg. */

  double gps_coords[] = {/*latitude*/0, /*longitude*/0, /*altitude*/0};
  ADD_REQUEST_ENTRY(ANDROID_JPEG_GPS_COORDINATES, gps_coords, 3);

  uint8_t gps_processing_method[] = "none";
  ADD_REQUEST_ENTRY(ANDROID_JPEG_GPS_PROCESSING_METHOD,
                    gps_processing_method, ARRAY_SIZE(gps_processing_method));

  int64_t gps_timestamp = 0;
  ADD_REQUEST_ENTRY(ANDROID_JPEG_GPS_TIMESTAMP, &gps_timestamp, 1);

  // JPEG orientation is relative to sensor orientation (mOrientation).
  int32_t jpeg_orientation = 0;
  ADD_REQUEST_ENTRY(ANDROID_JPEG_ORIENTATION, &jpeg_orientation, 1);

  // 1-100, larger is higher quality.
  uint8_t jpeg_quality = 80;
  ADD_REQUEST_ENTRY(ANDROID_JPEG_QUALITY, &jpeg_quality, 1);

  // TODO(b/29580107): If thumbnail quality actually matters/can be adjusted,
  // adjust this.
  uint8_t thumbnail_quality = 80;
  ADD_REQUEST_ENTRY(ANDROID_JPEG_THUMBNAIL_QUALITY, &thumbnail_quality, 1);

  // TODO(b/29580107): Choose a size matching the resolution.
  int32_t thumbnail_size[] = {0, 0};
  ADD_REQUEST_ENTRY(ANDROID_JPEG_THUMBNAIL_SIZE, thumbnail_size, 2);

  /* android.lens. */

  // Fixed values.
  ADD_REQUEST_ENTRY(ANDROID_LENS_APERTURE, &mAperture, 1);
  ADD_REQUEST_ENTRY(ANDROID_LENS_FILTER_DENSITY, &mFilterDensity, 1);
  ADD_REQUEST_ENTRY(ANDROID_LENS_FOCAL_LENGTH, &mFocalLength, 1);
  ADD_REQUEST_ENTRY(ANDROID_LENS_FOCUS_DISTANCE, &mFocusDistance, 1);

  uint8_t optical_stabilization = ANDROID_LENS_OPTICAL_STABILIZATION_MODE_OFF;
  ADD_REQUEST_ENTRY(ANDROID_LENS_OPTICAL_STABILIZATION_MODE,
                    &optical_stabilization, 1);

  /* android.noiseReduction. */

  uint8_t noise_reduction_mode = ANDROID_NOISE_REDUCTION_MODE_FAST;
  ADD_REQUEST_ENTRY(ANDROID_NOISE_REDUCTION_MODE, &noise_reduction_mode, 1);

  // strength marked FUTURE.

  /* android.request. */

  // Request Id unused by the HAL for now, and these are just
  // templates, so just fill it in with a dummy.
  int32_t id = 0;
  ADD_REQUEST_ENTRY(ANDROID_REQUEST_ID, &id, 1);

  // metadataMode marked FUTURE.

  /* android.scaler. */

  // No cropping by default; use the full active array.
  ADD_REQUEST_ENTRY(ANDROID_SCALER_CROP_REGION, mPixelArraySize.data(),
                    mPixelArraySize.size());

  /* android.sensor. */

  // exposureTime, sensitivity, testPattern[Data,Mode] not supported.

  // Ignored when AE is OFF.
  int64_t frame_duration = 33333333L; // 1/30 s.
  ADD_REQUEST_ENTRY(ANDROID_SENSOR_FRAME_DURATION, &frame_duration, 1);

  /* android.shading. */

  uint8_t shading_mode = ANDROID_SHADING_MODE_FAST;
  ADD_REQUEST_ENTRY(ANDROID_SHADING_MODE, &shading_mode, 1);

  /* android.statistics. */

  uint8_t face_detect_mode = ANDROID_STATISTICS_FACE_DETECT_MODE_OFF;
  ADD_REQUEST_ENTRY(ANDROID_STATISTICS_FACE_DETECT_MODE, &face_detect_mode, 1);

  // histogramMode, sharpnessMapMode marked FUTURE.

  uint8_t hp_map_mode = ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE_OFF;
  ADD_REQUEST_ENTRY(ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE, &hp_map_mode, 1);

  uint8_t lens_shading_map_mode = ANDROID_STATISTICS_LENS_SHADING_MAP_MODE_OFF;
  ADD_REQUEST_ENTRY(ANDROID_STATISTICS_LENS_SHADING_MAP_MODE,
                    &lens_shading_map_mode, 1);

  /* android.tonemap. */

  // Tonemap only required for MANUAL_POST_PROCESSING capability.

  /* android.led. */

  uint8_t transmit = ANDROID_LED_TRANSMIT_ON;
  ADD_REQUEST_ENTRY(ANDROID_LED_TRANSMIT, &transmit, 1);

  /* android.reprocess */

  // Only needed for REPROCESS capability.


  /* Template variable values. */

  // Find the FPS ranges "closest" to a desired range
  // (minimum abs distance from min to min and max to max).
  // Find both a fixed rate and a variable rate, for different purposes.
  std::array<int32_t, 2> desired_flat_fps_range = {{30, 30}};
  std::array<int32_t, 2> desired_variable_fps_range = {{5, 30}};
  std::array<int32_t, 2> flat_fps_range;
  std::array<int32_t, 2> variable_fps_range;
  int32_t best_flat_distance = std::numeric_limits<int32_t>::max();
  int32_t best_variable_distance = std::numeric_limits<int32_t>::max();
  size_t num_fps_ranges = mFpsRanges.num_arrays();
  for (size_t i = 0; i < num_fps_ranges; ++i) {
    const int32_t* range = mFpsRanges[i];
    // Variable fps.
    int32_t distance = std::abs(range[0] - desired_variable_fps_range[0]) +
        std::abs(range[1] - desired_variable_fps_range[1]);
    if (distance < best_variable_distance) {
      variable_fps_range[0] = range[0];
      variable_fps_range[1] = range[1];
      best_variable_distance = distance;
    }
    // Flat fps. Only do if range is actually flat.
    // Note at least one flat range is required,
    // so something will always be filled in.
    if (range[0] == range[1]) {
      distance = std::abs(range[0] - desired_flat_fps_range[0]) +
          std::abs(range[1] - desired_flat_fps_range[1]);
      if (distance < best_flat_distance) {
        flat_fps_range[0] = range[0];
        flat_fps_range[1] = range[1];
        best_flat_distance = distance;
      }
    }
  }

  // Priority: continuous > auto > off > whatever is available.
  bool continuous_still_avail = std::count(
      mAfModes.begin(), mAfModes.end(),
      ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE);
  bool continuous_video_avail = std::count(
      mAfModes.begin(), mAfModes.end(),
      ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO);
  uint8_t non_continuous_af_mode = mAfModes[0];
  if (std::count(mAfModes.begin(), mAfModes.end(),
                       ANDROID_CONTROL_AF_MODE_AUTO)) {
    non_continuous_af_mode = ANDROID_CONTROL_AF_MODE_AUTO;
  } else if (std::count(mAfModes.begin(), mAfModes.end(),
                       ANDROID_CONTROL_AF_MODE_OFF)) {
    non_continuous_af_mode = ANDROID_CONTROL_AF_MODE_OFF;
  }
  uint8_t still_af_mode = continuous_still_avail ?
      ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE : non_continuous_af_mode;
  uint8_t video_af_mode =  continuous_video_avail ?
      ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO : non_continuous_af_mode;

  // Copy our base metadata (note: we do it in this direction so we don't have
  // to redefine our ADD_REQUEST_ENTRY macro).
  android::CameraMetadata base_metadata(template_metadata);

  for (uint8_t template_id = 1; template_id < CAMERA3_TEMPLATE_COUNT; ++template_id) {
    // General differences/support.
    uint8_t intent;
    uint8_t af_mode;
    std::array<int32_t, 2> fps_range;
    switch(template_id) {
      case CAMERA3_TEMPLATE_PREVIEW:
        intent = ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW;
        af_mode = still_af_mode;
        fps_range = flat_fps_range;
        break;
      case CAMERA3_TEMPLATE_STILL_CAPTURE:
        intent = ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE;
        af_mode = still_af_mode;
        fps_range = variable_fps_range;
        break;
      case CAMERA3_TEMPLATE_VIDEO_RECORD:
        intent = ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_RECORD;
        af_mode = video_af_mode;
        fps_range = flat_fps_range;
        break;
      case CAMERA3_TEMPLATE_VIDEO_SNAPSHOT:
        intent = ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_SNAPSHOT;
        af_mode = video_af_mode;
        fps_range = flat_fps_range;
        break;
      case CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG:  // Fall through.
      case CAMERA3_TEMPLATE_MANUAL:  // Fall though.
      default:
        // Unsupported/unrecognized. Don't add this template; skip it.
        continue;
    }

    ADD_REQUEST_ENTRY(ANDROID_CONTROL_CAPTURE_INTENT, &intent, 1);
    ADD_REQUEST_ENTRY(ANDROID_CONTROL_AE_TARGET_FPS_RANGE,
                      fps_range.data(), fps_range.size());
    ADD_REQUEST_ENTRY(ANDROID_CONTROL_AF_MODE, &af_mode, 1);

    const camera_metadata_t* template_raw_metadata =
        template_metadata.getAndLock();
    res = setTemplate(template_id, template_raw_metadata);
    if (res != android::OK) {
      return res;
    }
    res = template_metadata.unlock(template_raw_metadata);
    if (res != android::OK) {
      return res;
    }

    // Reset the template metadata to the base.
    template_metadata = base_metadata;
  }

  mTemplatesInitialized = true;
  return 0;
}

bool V4L2Camera::isValidCaptureSettings(const camera_metadata_t* settings) {
  HAL_LOG_ENTER();

  // TODO(b/29335262): reject capture settings this camera isn't capable of.
  return true;
}

int V4L2Camera::initCharacteristics() {
  HAL_LOG_ENTER();

  /* Physical characteristics. */
  // No way to get these in V4L2, so faked.
  // Note: While many of these are primarily informative for post-processing
  // calculations by the app and will potentially cause bad results there,
  // focal length and physical size are actually used in framework
  // calculations (field of view, pixel pitch, etc), so faking them may
  // have unexpected results.
  mAperture = 2.0;  // RPi camera v2 is f/2.0.
  mFilterDensity = 0.0;
  mFocalLength = 3.04;  // RPi camera v2 is 3.04mm.
  mOrientation = 0;
  mPhysicalSize = {{3.674, 2.760}};  // RPi camera v2 is 3.674 x 2.760 mm.

  /* Fixed features. */

  // TODO(b/29394024): query VIDIOC_CROPCAP to get pixel rectangle.
  // Spoofing as 640 x 480 for now.
  mPixelArraySize = {{/*xmin*/0, /*ymin*/0, /*width*/640, /*height*/480}};

  // V4L2 VIDIOC_CROPCAP doesn't give a way to query this;
  // it's driver dependent. For now, assume freeform, and
  // some cameras may just behave badly.
  // TODO(b/29579652): Figure out a way to determine this.
  mCropType = ANDROID_SCALER_CROPPING_TYPE_FREEFORM;

  // TODO(b/29394024): query VIDIOC_CROPCAP to get cropping ranges,
  // and VIDIOC_G_CROP to determine if cropping is supported.
  // If the ioctl isn't available (or cropping has non-square pixelaspect),
  // assume no cropping/scaling.
  // May need to try setting some crops to determine what the driver actually
  // supports (including testing center vs freeform).
  mMaxZoom = 1;

  // TODO(b/29394024): query V4L2_CID_EXPOSURE_BIAS.
  mAeCompensationRange = {{0, 0}};
  mAeCompensationStep = {1, 1};

  // TODO(b/29394024): query V4L2_CID_3A_LOCK.
  mAeLockAvailable = ANDROID_CONTROL_AE_LOCK_AVAILABLE_FALSE;
  mAwbLockAvailable = ANDROID_CONTROL_AWB_LOCK_AVAILABLE_FALSE;

  // TODO(b/29394024): query V4L2_CID_FLASH_LED_MODE.
  mFlashAvailable = 0;

  // TODO(b/29394024): query V4L2_CID_FOCUS_ABSOLUTE for focus range.
  mFocusDistance = 0;  // Fixed focus.

  /* Features with (potentially) multiple options. */

  // TODO(b/29394024): query V4L2_CID_EXPOSURE_AUTO for ae modes.
  mAeModes.push_back(ANDROID_CONTROL_AE_MODE_ON);

  // TODO(b/29394024): query V4L2_CID_POWER_LINE_FREQUENCY.
  // Auto as the default, since it could mean anything, while OFF would
  // require guaranteeing no antibanding happens.
  mAeAntibandingModes.push_back(ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO);

  // TODO(b/29394024): query V4L2_CID_FOCUS_AUTO for
  // CONTINUOUS_VIDEO/CONTINUOUS_PICTURE. V4L2_CID_AUTO_FOCUS_START
  // supports what Android thinks of as auto focus (single auto focus).
  // V4L2_CID_AUTO_FOCUS_RANGE allows MACRO.
  mAfModes.push_back(ANDROID_CONTROL_AF_MODE_OFF);

  // TODO(b/29394024): query V4L2_CID_AUTO_WHITE_BALANCE, or
  // V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE if available.
  mAwbModes.push_back(ANDROID_CONTROL_AWB_MODE_AUTO);

  // TODO(b/29394024): query V4L2_CID_SCENE_MODE.
  mSceneModes.push_back(ANDROID_CONTROL_SCENE_MODE_DISABLED);

  mControlModes.push_back(ANDROID_CONTROL_MODE_AUTO);
  if (mSceneModes.size() > 1) {
    // We have some mode other than just DISABLED available.
    mControlModes.push_back(ANDROID_CONTROL_MODE_USE_SCENE_MODE);
  }

  // TODO(b/29394024): query V4L2_CID_COLORFX.
  mEffects.push_back(ANDROID_CONTROL_EFFECT_MODE_OFF);

  // TODO(b/29394024): query V4L2_CID_FLASH_INDICATOR_INTENSITY.
  // For now, no indicator LED available; nothing to push back.
  // When there is, push back ANDROID_LED_AVAILABLE_LEDS_TRANSMIT.

  // TODO(b/29394024): query V4L2_CID_IMAGE_STABILIZATION.
  mOpticalStabilizationModes.push_back(
      ANDROID_LENS_OPTICAL_STABILIZATION_MODE_OFF);
  mVideoStabilizationModes.push_back(
      ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_OFF);

  // Need to support YUV_420_888 and JPEG.
  // TODO(b/29394024): query VIDIOC_ENUM_FMT.
  std::vector<uint32_t> formats = {{V4L2_PIX_FMT_JPEG, V4L2_PIX_FMT_YUV420}};
  int32_t hal_format;
  // We want to find the smallest maximum frame duration amongst formats.
  mMaxFrameDuration = std::numeric_limits<int64_t>::max();
  int64_t min_yuv_frame_duration = std::numeric_limits<int64_t>::max();
  for (auto format : formats) {
    // Translate V4L2 format to HAL format.
    switch (format) {
      case V4L2_PIX_FMT_JPEG:
        hal_format = HAL_PIXEL_FORMAT_BLOB;
        break;
      case V4L2_PIX_FMT_YUV420:
        hal_format = HAL_PIXEL_FORMAT_YCbCr_420_888;
      default:
        // Unrecognized/unused format. Skip it.
        continue;
    }

    // TODO(b/29394024): query VIDIOC_ENUM_FRAMESIZES.
    // For now, just 640x480.
    ArrayVector<int32_t, 2> frame_sizes;
    frame_sizes.push_back({{640, 480}});
    size_t num_frame_sizes = frame_sizes.num_arrays();
    for (size_t i = 0; i < num_frame_sizes; ++i) {
      const int32_t* frame_size = frame_sizes[i];
      mStreamConfigs.push_back({{hal_format, frame_size[0], frame_size[1],
                ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT}});

      // TODO(b/29394024): query VIDIOC_ENUM_FRAMEINTERVALS.
      // For now, using the emulator max value of .3 sec.
      int64_t max_frame_duration = 300000000;
      // For now, min value of 30 fps = 1/30 spf ~= 33333333 ns.
      // For whatever reason the goldfish/qcom cameras report this as
      // 33331760, so copying that.
      int64_t min_frame_duration = 33331760;

      mMinFrameDurations.push_back(
          {{hal_format, frame_size[0], frame_size[1], min_frame_duration}});

      // In theory max frame duration (min frame rate) should be consistent
      // between all formats, but we check and only advertise the smallest
      // available max duration just in case.
      if (max_frame_duration < mMaxFrameDuration) {
        mMaxFrameDuration = max_frame_duration;
      }

      // We only care about min frame duration (max frame rate) for YUV.
      if (hal_format == HAL_PIXEL_FORMAT_YCbCr_420_888 &&
          min_frame_duration < min_yuv_frame_duration) {
        min_yuv_frame_duration = min_frame_duration;
      }

      // Usually 0 for non-jpeg, non-zero for JPEG.
      // Randomly choosing absurd 1 sec for JPEG. Unsure what this breaks.
      int64_t stall_duration = 0;
      if (hal_format == HAL_PIXEL_FORMAT_BLOB) {
        stall_duration =  1000000000;
      }
      mStallDurations.push_back(
          {{hal_format, frame_size[0], frame_size[1], stall_duration}});
    }
  }

  // This should be at minimum {mi, ma}, {ma, ma} where mi and ma
  // are min and max frame rates for YUV_420_888. Min should be at most 15.
  // Convert from frame durations measured in ns.
  int32_t min_yuv_fps = 1000000000 / mMaxFrameDuration;
  if (min_yuv_fps > 15) {
    return -ENODEV;
  }
  int32_t max_yuv_fps = 1000000000 / min_yuv_frame_duration;
  mFpsRanges.push_back({{min_yuv_fps, max_yuv_fps}});
  mFpsRanges.push_back({{max_yuv_fps, max_yuv_fps}});
  // Always advertise {30, 30} if max is even higher,
  // since this is what the default video requests use.
  if (max_yuv_fps > 30) {
    mFpsRanges.push_back({{30, 30}});
  }

  mCharacteristicsInitialized = true;
  return 0;
}

} // namespace v4l2_camera_hal
