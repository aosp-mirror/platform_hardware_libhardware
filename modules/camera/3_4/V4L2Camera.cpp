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
#include <sys/types.h>
#include <sys/stat.h>

#include <camera/CameraMetadata.h>
#include <hardware/camera3.h>
#include <nativehelper/ScopedFd.h>

#include "Common.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

namespace v4l2_camera_hal {

V4L2Camera::V4L2Camera(int id, std::string path)
    : default_camera_hal::Camera(id), mDevicePath(std::move(path)) {
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
  //   This is checked by the HAL, but the device at mDevicePath may
  //   not be the same one that was there when the HAL was loaded.
  //   (Alternatively, better hotplugging support may make this unecessary
  //   by disabling cameras that get disconnected and checking newly connected
  //   cameras, so connect() is never called on an unsupported camera)

  // TODO(b/29158098): Inform service of any flashes that are no longer available
  //   because this camera is in use.
  return 0;
}

void V4L2Camera::disconnect() {
  HAL_LOG_ENTER();
  // TODO(b/29158098): Inform service of any flashes that are available again
  //   because this camera is no longer in use.

  mDeviceFd.reset();
}

int V4L2Camera::initStaticInfo(camera_metadata_t** out) {
  HAL_LOG_ENTER();

  android::CameraMetadata info;

  std::vector<int32_t> avail_characteristics_keys;
  android::status_t res;

#define ADD_STATIC_ENTRY(name, varptr, count)   \
  avail_characteristics_keys.push_back(name); \
  res = info.update(name, varptr, count);     \
  if (res != android::OK) return res

  // Static metadata characteristics from /system/media/camera/docs/docs.html.

  /* android.color. */

  // No easy way to turn chromatic aberration correction OFF in v4l2,
  // though this may be supportable via a collection of other user controls.
  uint8_t avail_aberration_modes[] = {
    ANDROID_COLOR_CORRECTION_ABERRATION_MODE_FAST,
    ANDROID_COLOR_CORRECTION_ABERRATION_MODE_HIGH_QUALITY};
  ADD_STATIC_ENTRY(ANDROID_COLOR_CORRECTION_AVAILABLE_ABERRATION_MODES,
                   avail_aberration_modes, ARRAY_SIZE(avail_aberration_modes));

  /* android.control. */

  /*   3As */

  // TODO(b/29394024): query V4L2_CID_POWER_LINE_FREQUENCY
  uint8_t avail_ae_antibanding_modes[] = {
    ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF};
  ADD_STATIC_ENTRY(ANDROID_CONTROL_AE_AVAILABLE_ANTIBANDING_MODES,
                   avail_ae_antibanding_modes,
                   ARRAY_SIZE(avail_ae_antibanding_modes));

  // TODO(b/29394024): query V4L2_CID_EXPOSURE_AUTO for ae modes.
  uint8_t avail_ae_modes[] = {
    ANDROID_CONTROL_AE_MODE_ON};
  ADD_STATIC_ENTRY(ANDROID_CONTROL_AE_AVAILABLE_MODES,
                   avail_ae_modes, ARRAY_SIZE(avail_ae_modes));

  // TODO(b/29394034): query available YUV_420_888 frame rates.
  //   This should be {mi, ma, ma, ma} where mi is min(15, min frame rate),
  //   and ma is max frame rate (for YUV_420_888).
  int32_t avail_fps_ranges[] = {
    15, 30,
    30, 30};
  ADD_STATIC_ENTRY(ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES,
                   avail_fps_ranges, ARRAY_SIZE(avail_fps_ranges));

  // TODO(b/29394024): query V4L2_CID_EXPOSURE_BIAS.
  int32_t ae_comp_range[] = {0, 0};
  ADD_STATIC_ENTRY(ANDROID_CONTROL_AE_COMPENSATION_RANGE,
                   ae_comp_range, ARRAY_SIZE(ae_comp_range));

  // TODO(b/29394024): set based on V4L2_CID_EXPOSURE_BIAS step size.
  camera_metadata_rational ae_comp_step = {1, 1};
  ADD_STATIC_ENTRY(ANDROID_CONTROL_AE_COMPENSATION_STEP,
                   &ae_comp_step, 1);

  // TODO(b/29394024): query V4L2_CID_FOCUS_AUTO for
  //   CONTINUOUS_VIDEO/CONTINUOUS_PICTURE. V4L2_CID_AUTO_FOCUS_START
  //   supports what Android thinks of as auto focus (single auto focus).
  //   V4L2_CID_AUTO_FOCUS_RANGE allows MACRO.
  uint8_t avail_af_modes[] = {
    ANDROID_CONTROL_AF_MODE_OFF};
  ADD_STATIC_ENTRY(ANDROID_CONTROL_AF_AVAILABLE_MODES,
                   avail_af_modes, ARRAY_SIZE(avail_af_modes));

  // TODO(b/29394024): query V4L2_CID_AUTO_WHITE_BALANCE, or
  //   V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE if available.
  // If manual is supported, must look at android.colorCorrection
  //   transform and gains for the manual control. Not sure if this
  //   is supported by V4L2 or not, or if this is even a HAL job.
  uint8_t avail_awb_modes[] = {
    ANDROID_CONTROL_AWB_MODE_AUTO};
  ADD_STATIC_ENTRY(ANDROID_CONTROL_AWB_AVAILABLE_MODES,
                   avail_awb_modes, ARRAY_SIZE(avail_awb_modes));

  // Couldn't find any V4L2 support for regions, though maybe it's out there.
  int32_t max_regions[] = {/*AE*/ 0,/*AWB*/ 0,/*AF*/ 0};
  ADD_STATIC_ENTRY(ANDROID_CONTROL_MAX_REGIONS,
                   max_regions, ARRAY_SIZE(max_regions));

  // TODO(b/29394024): query V4L2_CID_3A_LOCK.
  uint8_t ae_lock_avail = ANDROID_CONTROL_AE_LOCK_AVAILABLE_FALSE;
  ADD_STATIC_ENTRY(ANDROID_CONTROL_AE_LOCK_AVAILABLE,
                   &ae_lock_avail, 1);
  uint8_t awb_lock_avail = ANDROID_CONTROL_AWB_LOCK_AVAILABLE_FALSE;
  ADD_STATIC_ENTRY(ANDROID_CONTROL_AWB_LOCK_AVAILABLE,
                   &awb_lock_avail, 1);

  /*   Scene modes. */

  // TODO(b/29394024): query V4L2_CID_SCENE_MODE.
  uint8_t avail_scene_modes[] = {
    ANDROID_CONTROL_SCENE_MODE_DISABLED};
  ADD_STATIC_ENTRY(ANDROID_CONTROL_AVAILABLE_SCENE_MODES,
                   avail_scene_modes, ARRAY_SIZE(avail_scene_modes));

  // A 3-tuple of AE, AWB, AF overrides for each scene mode.
  // Ignored for DISABLED, FACE_PRIORITY and FACE_PRIORITY_LOW_LIGHT.
  uint8_t scene_mode_overrides[] = {
    /*SCENE_MODE_DISABLED*/ /*AE*/0, /*AW*/0, /*AF*/0};
  ADD_STATIC_ENTRY(ANDROID_CONTROL_SCENE_MODE_OVERRIDES,
                   scene_mode_overrides, ARRAY_SIZE(scene_mode_overrides));

  /*   Top level 3A/Scenes switch. */

  // TODO(b/29394024): Add USE_SCENE_MODE if scene modes are enabled.
  uint8_t avail_modes[] = {
    ANDROID_CONTROL_MODE_AUTO};
  ADD_STATIC_ENTRY(ANDROID_CONTROL_AVAILABLE_MODES,
                   avail_modes, ARRAY_SIZE(avail_modes));

  /*   Other android.control configuration. */

  // TODO(b/29394024): query V4L2_CID_IMAGE_STABILIZATION.
  uint8_t avail_stabilization[] = {
    ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_OFF};
  ADD_STATIC_ENTRY(ANDROID_CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES,
                   avail_stabilization, ARRAY_SIZE(avail_stabilization));

  // TODO(b/29394024): query V4L2_CID_COLORFX.
  uint8_t avail_effects[] = {
    ANDROID_CONTROL_EFFECT_MODE_OFF};
  ADD_STATIC_ENTRY(ANDROID_CONTROL_AVAILABLE_EFFECTS,
                   avail_effects, ARRAY_SIZE(avail_effects));

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

  // TODO(b/29394024): query V4L2_CID_FLASH_LED_MODE.
  uint8_t flash_avail = 0;
  ADD_STATIC_ENTRY(ANDROID_FLASH_INFO_AVAILABLE,
                   &flash_avail, 1);

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
  //   since V4L2 doesn't actually allow for thumbnail size control.
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

  // No way to actually get the f-value in V4L2, but it's a required key,
  // so we just fake it. Raspberry Pi camera v2 is f/2.0.
  float avail_apertures[] = {2.0};
  ADD_STATIC_ENTRY(ANDROID_LENS_INFO_AVAILABLE_APERTURES,
                   avail_apertures, ARRAY_SIZE(avail_apertures));

  // No known V4L2 neutral density filter control.
  float avail_filter_densities[] = {0.0};
  ADD_STATIC_ENTRY(ANDROID_LENS_INFO_AVAILABLE_FILTER_DENSITIES,
                   avail_filter_densities, ARRAY_SIZE(avail_filter_densities));

  // TODO(b/29394024): query V4L2_CID_IMAGE_STABILIZATION.
  uint8_t avail_optical_stabilization[] = {
    ANDROID_LENS_OPTICAL_STABILIZATION_MODE_OFF};
  ADD_STATIC_ENTRY(ANDROID_LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION,
                   avail_optical_stabilization,
                   ARRAY_SIZE(avail_optical_stabilization));

  // No known V4L2 shading map info.
  int32_t shading_map_size[] = {1, 1};
  ADD_STATIC_ENTRY(ANDROID_LENS_INFO_SHADING_MAP_SIZE,
                   shading_map_size, ARRAY_SIZE(shading_map_size));

  // All V4L2 devices are considered to be external facing.
  uint8_t facing = ANDROID_LENS_FACING_EXTERNAL;
  ADD_STATIC_ENTRY(ANDROID_LENS_FACING, &facing, 1);

  /*   Zoom/Focus. */

  // No way to actually get the focal length in V4L2, but it's a required key,
  // so we just fake it. Raspberry Pi camera v2 is 3.04mm.
  // Note: unlike f values, this key is actually used in calculations
  //   (field of view), so other cameras may see inaccurate results.
  float focal_length = 3.04;
  ADD_STATIC_ENTRY(ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS,
                   &focal_length, 1);

  // V4L2 focal units do not correspond to a particular physical unit.
  uint8_t focus_calibration =
      ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_UNCALIBRATED;
  ADD_STATIC_ENTRY(ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION,
                   &focus_calibration, 1);

  // info.hyperfocalDistance not required for UNCALIBRATED.

  // TODO(b/29394024): query V4L2_CID_ZOOM_ABSOLUTE for focus range.
  // 0 is fixed focus.
  float min_focus_distance = 0.0;
  ADD_STATIC_ENTRY(ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE,
                   &min_focus_distance, 1);

  /*   Depth. */

  // DEPTH capability not supported by this HAL. Not implemented:
  //   poseRotation
  //   poseTranslation
  //   intrinsicCalibration
  //   radialDistortion

  /* anroid.noise. */

  // Unable to control noise reduction in V4L2 devices,
  // but FAST is allowed to be the same as OFF.
  uint8_t avail_noise_reduction_modes[] = {
    ANDROID_NOISE_REDUCTION_MODE_FAST};
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

  // TODO(b/29394024): query VIDIOC_CROPCAP to get cropping ranges,
  //   and VIDIOC_G_CROP to determine if cropping is supported.
  //   If the ioctl isn't available (or cropping has non-square pixelaspect),
  //   assume no cropping/scaling.
  //   May need to try setting some crops to determine what the driver actually
  //   supports (including testing center vs freeform).
  float max_zoom = 1;
  ADD_STATIC_ENTRY(ANDROID_SCALER_AVAILABLE_MAX_DIGITAL_ZOOM,
                   &max_zoom, 1);

  // V4L2 VIDIOC_CROPCAP doesn't give a way to query this;
  //   it's driver dependent. For now, assume freeform, and
  //   some cameras may just behave badly.
  // TODO(b/29579652): Figure out a way to determine this.
  uint8_t crop_type = ANDROID_SCALER_CROPPING_TYPE_FREEFORM;
  ADD_STATIC_ENTRY(ANDROID_SCALER_CROPPING_TYPE, &crop_type, 1);

  /*   Streams. */

  // availableInputOutputFormatsMap only required for reprocessing capability.

  // TODO(b/29394024): query VIDIOC_ENUM_FMT to get pixel formats, pull out
  //   YUV_420_888 and JPEG, query VIDIOC_ENUM_FRAMESIZES on those.
  // TODO(b/29581206): Need behavior if YUV_420_888 and JPEG aren't available.
  // For now, just 640x480 JPEG.
  int32_t avail_stream_configs[] = {
    HAL_PIXEL_FORMAT_BLOB, 640, 480,
    ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT};
  ADD_STATIC_ENTRY(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
                   avail_stream_configs, ARRAY_SIZE(avail_stream_configs));

  // Per format and size, minimum frame duration (max frame rate).
  // For now, just 30 fps = 1/30 spf ~= 33333333 ns.
  //   For whatever reason the goldfish/qcom cameras report this as
  //   33331760, so copying that.
  // TODO(b/29394024): query the available format frame rates.
  int64_t avail_min_frame_durations[] = {
    HAL_PIXEL_FORMAT_BLOB, 640, 480, 33331760};
  ADD_STATIC_ENTRY(ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS,
                   avail_min_frame_durations,
                   ARRAY_SIZE(avail_min_frame_durations));

  // Per format and size, usually 0 for non-jpeg, non-zero for JPEG.
  // Randomly choosing absurdly long 1 sec for JPEG. Unsure what this breaks.
  int64_t avail_stall_durations[] = {
    HAL_PIXEL_FORMAT_BLOB, 640, 480, 1000000000};
  ADD_STATIC_ENTRY(ANDROID_SCALER_AVAILABLE_STALL_DURATIONS,
                   avail_stall_durations, ARRAY_SIZE(avail_stall_durations));

  /* android.sensor. */

  /*   Sizes. */

  // Spoof as 640 x 480 for now.
  // TODO(b/29394024): query VIDIOC_CROPCAP to get pixel rectangle.
  int32_t array_rect[] = {
    /*xmin*/0, /*ymin*/0, /*width*/640, /*height*/480};
  ADD_STATIC_ENTRY(ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE,
                   array_rect, ARRAY_SIZE(array_rect));
  // No V4L2 way to differentiate active vs. inactive parts of the rectangle.
  ADD_STATIC_ENTRY(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE,
                   array_rect, ARRAY_SIZE(array_rect));

  // No way to get actual physical size from V4L2. Field of view/pixel pitch
  // calculations will be off. Using Raspberry Pi camera v2 measurements
  // (3.674 x 2.760 mm).
  float physical_size[] = {3.674, 2.760};
  ADD_STATIC_ENTRY(ANDROID_SENSOR_INFO_PHYSICAL_SIZE,
                   physical_size, ARRAY_SIZE(physical_size));

  /*   Misc sensor information. */

  // For now, using the emulator value of .3 sec.
  // TODO(b/29394024): query for min frame rate.
  int64_t max_frame_duration = 300000000;
  ADD_STATIC_ENTRY(ANDROID_SENSOR_INFO_MAX_FRAME_DURATION,
                   &max_frame_duration, 1);

  // HAL uses BOOTTIME timestamps.
  // TODO(b/29457051): make sure timestamps are consistent throughout the HAL.
  uint8_t timestamp_source = ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE_UNKNOWN;
  ADD_STATIC_ENTRY(ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE,
                   &timestamp_source, 1);

  // As in initDeviceInfo, no way to actually get orientation.
  int32_t orientation = 0;
  ADD_STATIC_ENTRY(ANDROID_SENSOR_ORIENTATION, &orientation, 1);

  // availableTestPatternModes just defaults to OFF, which is fine.

  // info.exposureTimeRange, info.sensitivityRange:
  //   exposure/sensitivity manual control not supported.
  //   Could query V4L2_CID_ISO_SENSITIVITY to support sensitivity if desired.

  // info.whiteLevel, info.lensShadingApplied,
  //   info.preCorrectionActiveArraySize, referenceIlluminant1/2,
  //   calibrationTransform1/2, colorTransform1/2, forwardMatrix1/2,
  //   blackLevelPattern, profileHueSatMapDimensions
  //   all only necessary for RAW.

  // baseGainFactor marked FUTURE.

  // maxAnalogSensitivity optional for LIMITED device.

  // opticalBlackRegions: No known way to get in V4L2, but not necessary.

  // opaqueRawSize not necessary since RAW_OPAQUE format not supported.

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
  //   info.maxSharpnessMapValue, info.sharpnessMapSizemarked FUTURE.

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

  // availableLeds: for now, none available, so no need to set.
  // TODO(b/29394024): query V4L2_CID_FLASH_INDICATOR_INTENSITY?
  //   if it's there, assume indicator exists?

  /* android.sync. */

  // "LIMITED devices are strongly encouraged to use a non-negative value.
  //   If UNKNOWN is used here then app developers do not have a way to know
  //   when sensor settings have been applied." - Unfortunately, V4L2 doesn't
  //   really help here either. Could even be that adjusting settings mid-stream
  //   blocks in V4L2, and should be avoided.
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

  // TODO(b/29221795): available request keys
  //   (fill in when writing default requests).

  // TODO(b/29335262): available result keys
  //   (fill in when writing actual result capture).

  // Last thing, once all the available characteristics have been added.
  info.update(ANDROID_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS,
              &avail_characteristics_keys[0],
              avail_characteristics_keys.size());

  *out = info.release();
  return 0;
}

void V4L2Camera::initDeviceInfo(camera_info_t* info) {
  HAL_LOG_ENTER();

  // For now, just constants.
  info->facing = CAMERA_FACING_EXTERNAL;
  info->orientation = 0;
  info->resource_cost = 100;
  info->conflicting_devices = nullptr;
  info->conflicting_devices_length = 0;
}

int V4L2Camera::initDevice() {
  HAL_LOG_ENTER();

  // TODO(b/29221795): fill in templates, etc.
  return 0;
}

bool V4L2Camera::isValidCaptureSettings(const camera_metadata_t* settings) {
  HAL_LOG_ENTER();

  // TODO(b): reject capture settings this camera isn't capable of.
  return true;
}

} // namespace v4l2_camera_hal
