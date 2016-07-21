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
#include "V4L2Gralloc.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

namespace v4l2_camera_hal {

// Helper function for managing metadata.
static std::vector<int32_t> getMetadataKeys(
    const camera_metadata_t* metadata) {
  std::vector<int32_t> keys;
  size_t num_entries = get_camera_metadata_entry_count(metadata);
  for (size_t i = 0; i < num_entries; ++i) {
    camera_metadata_ro_entry_t entry;
    get_camera_metadata_ro_entry(metadata, i, &entry);
    keys.push_back(entry.tag);
  }
  return keys;
}

V4L2Camera::V4L2Camera(int id, std::string path)
    : default_camera_hal::Camera(id),
      mDevicePath(std::move(path)),
      mOutStreamType(0),
      mOutStreamFormat(0),
      mOutStreamWidth(0),
      mOutStreamHeight(0),
      mOutStreamBytesPerLine(0),
      mOutStreamMaxBuffers(0),
      mTemplatesInitialized(false),
      mCharacteristicsInitialized(false) {
  HAL_LOG_ENTER();
}

V4L2Camera::~V4L2Camera() {
  HAL_LOG_ENTER();
}

// Helper function. Should be used instead of ioctl throughout this class.
template<typename T>
int V4L2Camera::ioctlLocked(int request, T data) {
  HAL_LOG_ENTER();

  android::Mutex::Autolock al(mDeviceLock);
  if (mDeviceFd.get() < 0) {
    return -ENODEV;
  }
  return TEMP_FAILURE_RETRY(ioctl(mDeviceFd.get(), request, data));
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

  // Clear out our variables tracking device settings.
  mOutStreamType = 0;
  mOutStreamFormat = 0;
  mOutStreamWidth = 0;
  mOutStreamHeight = 0;
  mOutStreamBytesPerLine = 0;
  mOutStreamMaxBuffers = 0;
}

int V4L2Camera::streamOn() {
  HAL_LOG_ENTER();

  if (!mOutStreamType) {
    HAL_LOGE("Stream format must be set before turning on stream.");
    return -EINVAL;
  }

  if (ioctlLocked(VIDIOC_STREAMON, &mOutStreamType) < 0) {
    HAL_LOGE("STREAMON fails: %s", strerror(errno));
    return -ENODEV;
  }

  return 0;
}

int V4L2Camera::streamOff() {
  HAL_LOG_ENTER();

  // TODO(b/30000211): support mplane as well.
  if (ioctlLocked(VIDIOC_STREAMOFF, &mOutStreamType) < 0) {
    HAL_LOGE("STREAMOFF fails: %s", strerror(errno));
    return -ENODEV;
  }

  return 0;
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

  // Static metadata characteristics from /system/media/camera/docs/docs.html.

  /* android.colorCorrection. */

  // No easy way to turn chromatic aberration correction OFF in v4l2,
  // though this may be supportable via a collection of other user controls.
  uint8_t avail_aberration_modes[] = {
    ANDROID_COLOR_CORRECTION_ABERRATION_MODE_FAST,
    ANDROID_COLOR_CORRECTION_ABERRATION_MODE_HIGH_QUALITY};
  res = info.update(ANDROID_COLOR_CORRECTION_AVAILABLE_ABERRATION_MODES,
                    avail_aberration_modes, ARRAY_SIZE(avail_aberration_modes));
  if (res != android::OK) {
    return res;
  }

  /* android.control. */

  /*   3As */

  res = info.update(ANDROID_CONTROL_AE_AVAILABLE_ANTIBANDING_MODES,
                    mAeAntibandingModes.data(), mAeAntibandingModes.size());
  if (res != android::OK) {
    return res;
  }

  res = info.update(ANDROID_CONTROL_AE_AVAILABLE_MODES,
                    mAeModes.data(), mAeModes.size());
  if (res != android::OK) {
    return res;
  }

  // Flatten mFpsRanges.
  res = info.update(ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES,
                    mFpsRanges.data(), mFpsRanges.total_num_elements());
  if (res != android::OK) {
    return res;
  }

  res = info.update(ANDROID_CONTROL_AE_COMPENSATION_RANGE,
                    mAeCompensationRange.data(), mAeCompensationRange.size());
  if (res != android::OK) {
    return res;
  }

  res = info.update(ANDROID_CONTROL_AE_COMPENSATION_STEP,
                    &mAeCompensationStep, 1);
  if (res != android::OK) {
    return res;
  }

  res = info.update(ANDROID_CONTROL_AF_AVAILABLE_MODES,
                    mAfModes.data(), mAfModes.size());
  if (res != android::OK) {
    return res;
  }

  res = info.update(ANDROID_CONTROL_AWB_AVAILABLE_MODES,
                    mAwbModes.data(), mAwbModes.size());
  if (res != android::OK) {
    return res;
  }

  // Couldn't find any V4L2 support for regions, though maybe it's out there.
  int32_t max_regions[] = {/*AE*/ 0,/*AWB*/ 0,/*AF*/ 0};
  res = info.update(ANDROID_CONTROL_MAX_REGIONS,
                    max_regions, ARRAY_SIZE(max_regions));
  if (res != android::OK) {
    return res;
  }

  res = info.update(ANDROID_CONTROL_AE_LOCK_AVAILABLE,
                    &mAeLockAvailable, 1);
  if (res != android::OK) {
    return res;
  }
  res = info.update(ANDROID_CONTROL_AWB_LOCK_AVAILABLE,
                    &mAwbLockAvailable, 1);
  if (res != android::OK) {
    return res;
  }

  /*   Scene modes. */

  res = info.update(ANDROID_CONTROL_AVAILABLE_SCENE_MODES,
                    mSceneModes.data(), mSceneModes.size());
  if (res != android::OK) {
    return res;
  }

  // A 3-tuple of AE, AWB, AF overrides for each scene mode.
  // Ignored for DISABLED, FACE_PRIORITY and FACE_PRIORITY_LOW_LIGHT.
  uint8_t scene_mode_overrides[] = {
    /*SCENE_MODE_DISABLED*/ /*AE*/0, /*AW*/0, /*AF*/0};
  res = info.update(ANDROID_CONTROL_SCENE_MODE_OVERRIDES,
                    scene_mode_overrides, ARRAY_SIZE(scene_mode_overrides));
  if (res != android::OK) {
    return res;
  }

  /*   Top level 3A/Scenes switch. */

  res = info.update(ANDROID_CONTROL_AVAILABLE_MODES,
                    mControlModes.data(), mControlModes.size());
  if (res != android::OK) {
    return res;
  }

  /*   Other android.control configuration. */

  res = info.update(ANDROID_CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES,
                    mVideoStabilizationModes.data(),
                    mVideoStabilizationModes.size());
  if (res != android::OK) {
    return res;
  }

  res = info.update(ANDROID_CONTROL_AVAILABLE_EFFECTS,
                    mEffects.data(), mEffects.size());
  if (res != android::OK) {
    return res;
  }

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
  res = info.update(ANDROID_EDGE_AVAILABLE_EDGE_MODES,
                    avail_edge_modes, ARRAY_SIZE(avail_edge_modes));
  if (res != android::OK) {
    return res;
  }

  /* android.flash. */

  res = info.update(ANDROID_FLASH_INFO_AVAILABLE,
                    &mFlashAvailable, 1);
  if (res != android::OK) {
    return res;
  }

  // info.chargeDuration, color.Temperature, maxEnergy marked FUTURE.

  /* android.hotPixel. */

  // No known V4L2 hot pixel correction. But it might be happening,
  // so we report FAST/HIGH_QUALITY.
  uint8_t avail_hot_pixel_modes[] = {
    ANDROID_HOT_PIXEL_MODE_FAST,
    ANDROID_HOT_PIXEL_MODE_HIGH_QUALITY};
  res = info.update(ANDROID_HOT_PIXEL_AVAILABLE_HOT_PIXEL_MODES,
                    avail_hot_pixel_modes, ARRAY_SIZE(avail_hot_pixel_modes));
  if (res != android::OK) {
    return res;
  }

  /* android.jpeg. */

  // For now, no thumbnails available (only [0,0], the "no thumbnail" size).
  // TODO(b/29580107): Could end up with a mismatch between request & result,
  // since V4L2 doesn't actually allow for thumbnail size control.
  int32_t thumbnail_sizes[] = {
    0, 0};
  res = info.update(ANDROID_JPEG_AVAILABLE_THUMBNAIL_SIZES,
                    thumbnail_sizes, ARRAY_SIZE(thumbnail_sizes));
  if (res != android::OK) {
    return res;
  }

  // V4L2 doesn't support querying this,
  // so instead use a constant (defined in V4L2Gralloc.h).
  int32_t max_jpeg_size = V4L2_MAX_JPEG_SIZE;
  res = info.update(ANDROID_JPEG_MAX_SIZE,
                    &max_jpeg_size, 1);
  if (res != android::OK) {
    return res;
  }

  /* android.lens. */

  /*   Misc. lens control. */

  res = info.update(ANDROID_LENS_INFO_AVAILABLE_APERTURES,
                    &mAperture, 1);
  if (res != android::OK) {
    return res;
  }

  res = info.update(ANDROID_LENS_INFO_AVAILABLE_FILTER_DENSITIES,
                    &mFilterDensity, 1);
  if (res != android::OK) {
    return res;
  }

  res = info.update(ANDROID_LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION,
                    mOpticalStabilizationModes.data(),
                    mOpticalStabilizationModes.size());
  if (res != android::OK) {
    return res;
  }

  // No known V4L2 shading map info.
  int32_t shading_map_size[] = {1, 1};
  res = info.update(ANDROID_LENS_INFO_SHADING_MAP_SIZE,
                    shading_map_size, ARRAY_SIZE(shading_map_size));
  if (res != android::OK) {
    return res;
  }

  // All V4L2 devices are considered to be external facing.
  uint8_t facing = ANDROID_LENS_FACING_EXTERNAL;
  res = info.update(ANDROID_LENS_FACING, &facing, 1);
  if (res != android::OK) {
    return res;
  }

  /*   Zoom/Focus. */

  // No way to actually get the focal length in V4L2, but it's a required key,
  // so we just fake it.
  res = info.update(ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS,
                    &mFocalLength, 1);
  if (res != android::OK) {
    return res;
  }

  // V4L2 focal units do not correspond to a particular physical unit.
  uint8_t focus_calibration =
      ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_UNCALIBRATED;
  res = info.update(ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION,
                    &focus_calibration, 1);
  if (res != android::OK) {
    return res;
  }

  // info.hyperfocalDistance not required for UNCALIBRATED.

  res = info.update(ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE,
                    &mFocusDistance, 1);
  if (res != android::OK) {
    return res;
  }

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
  res = info.update(ANDROID_NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES,
                    avail_noise_reduction_modes,
                    ARRAY_SIZE(avail_noise_reduction_modes));
  if (res != android::OK) {
    return res;
  }

  /* android.request. */

  int32_t max_num_output_streams[] = {
    mMaxRawOutputStreams, mMaxYuvOutputStreams, mMaxJpegOutputStreams};
  res = info.update(ANDROID_REQUEST_MAX_NUM_OUTPUT_STREAMS,
                    max_num_output_streams, ARRAY_SIZE(max_num_output_streams));
  if (res != android::OK) {
    return res;
  }

  res = info.update(ANDROID_REQUEST_MAX_NUM_INPUT_STREAMS,
                    &mMaxInputStreams, 1);
  if (res != android::OK) {
    return res;
  }

  // No way to know for V4L2, so fake with max allowable latency.
  // Doesn't mean much without per-frame controls.
  uint8_t pipeline_max_depth = 4;
  res = info.update(ANDROID_REQUEST_PIPELINE_MAX_DEPTH,
                    &pipeline_max_depth, 1);
  if (res != android::OK) {
    return res;
  }

  // Partial results not supported; partialResultCount defaults to 1.

  // Available capabilities & keys queried at very end of this method.

  /* android.scaler. */

  /*   Cropping. */

  res = info.update(ANDROID_SCALER_AVAILABLE_MAX_DIGITAL_ZOOM,
                    &mMaxZoom, 1);
  if (res != android::OK) {
    return res;
  }

  res = info.update(ANDROID_SCALER_CROPPING_TYPE, &mCropType, 1);
  if (res != android::OK) {
    return res;
  }

  /*   Streams. */

  // availableInputOutputFormatsMap only required for reprocessing capability.

  // Flatten mStreamConfigs.
  res = info.update(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
                    mStreamConfigs.data(),
                    mStreamConfigs.total_num_elements());
  if (res != android::OK) {
    return res;
  }

  // Flatten mMinFrameDurations.
  res = info.update(ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS,
                    mMinFrameDurations.data(),
                    mMinFrameDurations.total_num_elements());
  if (res != android::OK) {
    return res;
  }

  // Flatten mStallDurations.
  res = info.update(ANDROID_SCALER_AVAILABLE_STALL_DURATIONS,
                    mStallDurations.data(),
                    mStallDurations.total_num_elements());
  if (res != android::OK) {
    return res;
  }

  /* android.sensor. */

  /*   Sizes. */

  res = info.update(ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE,
                    mPixelArraySize.data(), mPixelArraySize.size());
  if (res != android::OK) {
    return res;
  }
  // No V4L2 way to differentiate active vs. inactive parts of the rectangle.
  res = info.update(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE,
                    mPixelArraySize.data(), mPixelArraySize.size());
  if (res != android::OK) {
    return res;
  }

  res = info.update(ANDROID_SENSOR_INFO_PHYSICAL_SIZE,
                    mPhysicalSize.data(), mPhysicalSize.size());
  if (res != android::OK) {
    return res;
  }

  /*   Misc sensor information. */

  res = info.update(ANDROID_SENSOR_INFO_MAX_FRAME_DURATION,
                    &mMaxFrameDuration, 1);
  if (res != android::OK) {
    return res;
  }

  // HAL uses BOOTTIME timestamps.
  // TODO(b/29457051): make sure timestamps are consistent throughout the HAL.
  uint8_t timestamp_source = ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE_UNKNOWN;
  res = info.update(ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE,
                    &timestamp_source, 1);
  if (res != android::OK) {
    return res;
  }

  // As in initDeviceInfo, no way to actually get orientation.
  res = info.update(ANDROID_SENSOR_ORIENTATION, &mOrientation, 1);
  if (res != android::OK) {
    return res;
  }

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
  res = info.update(ANDROID_SHADING_AVAILABLE_MODES,
                    avail_shading_modes, ARRAY_SIZE(avail_shading_modes));
  if (res != android::OK) {
    return res;
  }

  /* android.statistics */

  // Face detection not supported.
  uint8_t avail_face_detect_modes[] = {
    ANDROID_STATISTICS_FACE_DETECT_MODE_OFF};
  res = info.update(ANDROID_STATISTICS_INFO_AVAILABLE_FACE_DETECT_MODES,
                    avail_face_detect_modes,
                    ARRAY_SIZE(avail_face_detect_modes));
  if (res != android::OK) {
    return res;
  }

  int32_t max_face_count = 0;
  res = info.update(ANDROID_STATISTICS_INFO_MAX_FACE_COUNT,
                    &max_face_count, 1);
  if (res != android::OK) {
    return res;
  }

  // info.histogramBucketCount, info.maxHistogramCount,
  // info.maxSharpnessMapValue, info.sharpnessMapSizemarked FUTURE.

  // ON only needs to be supported for RAW capable devices.
  uint8_t avail_hot_pixel_map_modes[] = {
    ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE_OFF};
  res = info.update(ANDROID_STATISTICS_INFO_AVAILABLE_HOT_PIXEL_MAP_MODES,
                    avail_hot_pixel_map_modes,
                    ARRAY_SIZE(avail_hot_pixel_map_modes));
  if (res != android::OK) {
    return res;
  }

  // ON only needs to be supported for RAW capable devices.
  uint8_t avail_lens_shading_map_modes[] = {
    ANDROID_STATISTICS_LENS_SHADING_MAP_MODE_OFF};
  res = info.update(ANDROID_STATISTICS_INFO_AVAILABLE_LENS_SHADING_MAP_MODES,
                    avail_lens_shading_map_modes,
                    ARRAY_SIZE(avail_lens_shading_map_modes));
  if (res != android::OK) {
    return res;
  }

  /* android.tonemap. */

  // tonemapping only required for MANUAL_POST_PROCESSING capability.

  /* android.led. */

  // May or may not have LEDs available.
  if (!mLeds.empty()) {
    res = info.update(ANDROID_LED_AVAILABLE_LEDS, mLeds.data(), mLeds.size());
    if (res != android::OK) {
      return res;
    }
  }

  /* android.sync. */

  // "LIMITED devices are strongly encouraged to use a non-negative value.
  // If UNKNOWN is used here then app developers do not have a way to know
  // when sensor settings have been applied." - Unfortunately, V4L2 doesn't
  // really help here either. Could even be that adjusting settings mid-stream
  // blocks in V4L2, and should be avoided.
  int32_t max_latency = ANDROID_SYNC_MAX_LATENCY_UNKNOWN;
  res = info.update(ANDROID_SYNC_MAX_LATENCY,
                    &max_latency, 1);
  if (res != android::OK) {
    return res;
  }

  /* android.reprocess. */

  // REPROCESSING not supported by this HAL.

  /* android.depth. */

  // DEPTH not supported by this HAL.

  /* Capabilities and android.info. */

  uint8_t hw_level = ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_LIMITED;
  res = info.update(ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL,
                    &hw_level, 1);
  if (res != android::OK) {
    return res;
  }

  uint8_t capabilities[] = {
    ANDROID_REQUEST_AVAILABLE_CAPABILITIES_BACKWARD_COMPATIBLE};
  res = info.update(ANDROID_REQUEST_AVAILABLE_CAPABILITIES,
                    capabilities, ARRAY_SIZE(capabilities));
  if (res != android::OK) {
    return res;
  }

  // Scan a default request template for included request keys.
  if (!mTemplatesInitialized) {
    res = initTemplates();
    if (res) {
      return res;
    }
  }
  const camera_metadata_t* preview_request = nullptr;
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
  std::vector<int32_t> avail_request_keys = getMetadataKeys(preview_request);
  res = info.update(ANDROID_REQUEST_AVAILABLE_REQUEST_KEYS,
                    avail_request_keys.data(), avail_request_keys.size());
  if (res != android::OK) {
    return res;
  }

  // Result keys will be duplicated from the request, plus a few extras.
  // TODO(b/30035628): additonal available result keys.
  std::vector<int32_t> avail_result_keys(avail_request_keys);
  res = info.update(ANDROID_REQUEST_AVAILABLE_RESULT_KEYS,
                    avail_result_keys.data(), avail_result_keys.size());
  if (res != android::OK) {
    return res;
  }

  // Last thing, once all the available characteristics have been added.
  const camera_metadata_t* static_characteristics = info.getAndLock();
  std::vector<int32_t> avail_characteristics_keys =
      getMetadataKeys(static_characteristics);
  res = info.unlock(static_characteristics);
  if (res != android::OK) {
    return res;
  }
  avail_characteristics_keys.push_back(
      ANDROID_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS);
  res = info.update(ANDROID_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS,
                    avail_characteristics_keys.data(),
                    avail_characteristics_keys.size());
  if (res != android::OK) {
    return res;
  }

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

  // Initialize and check the gralloc module.
  const hw_module_t* module;
  res = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
  if (res) {
    HAL_LOGE("Couldn't get gralloc module.");
    return -ENODEV;
  }
  const gralloc_module_t* gralloc =
      reinterpret_cast<const gralloc_module_t*>(module);
  mGralloc = V4L2Gralloc(gralloc);
  if (!mGralloc.isValid()) {
    HAL_LOGE("Invalid gralloc module.");
    return -ENODEV;
  }

  // Templates should be set up if they haven't already been.
  if (!mTemplatesInitialized) {
    res = initTemplates();
    if (res) {
      return res;
    }
  }

  return 0;
}

int V4L2Camera::enqueueBuffer(const camera3_stream_buffer_t* camera_buffer) {
  HAL_LOG_ENTER();

  // Set up a v4l2 buffer struct.
  v4l2_buffer device_buffer;
  memset(&device_buffer, 0, sizeof(device_buffer));
  device_buffer.type = mOutStreamType;

  // Use QUERYBUF to ensure our buffer/device is in good shape.
  if (ioctlLocked(VIDIOC_QUERYBUF, &device_buffer) < 0) {
    HAL_LOGE("QUERYBUF (%d) fails: %s", 0, strerror(errno));
    return -ENODEV;
  }

  // Configure the device buffer based on the stream buffer.
  device_buffer.memory = V4L2_MEMORY_USERPTR;
  // TODO(b/29334616): when this is async, actually limit the number
  // of buffers used to the known max, and set this according to the
  // queue length.
  device_buffer.index = 0;
  // Lock the buffer for writing.
  int res = mGralloc.lock(camera_buffer, mOutStreamBytesPerLine,
                          &device_buffer);
  if (res) {
    return res;
  }
  if (ioctlLocked(VIDIOC_QBUF, &device_buffer) < 0) {
    HAL_LOGE("QBUF (%d) fails: %s", 0, strerror(errno));
    mGralloc.unlock(&device_buffer);
    return -ENODEV;
  }

  // Turn on the stream.
  // TODO(b/29334616): Lock around stream on/off access, only start stream
  // if not already on. (For now, since it's synchronous, it will always be
  // turned off before another call to this function).
  res = streamOn();
  if (res) {
    mGralloc.unlock(&device_buffer);
    return res;
  }

  // TODO(b/29334616): Enqueueing and dequeueing should be separate worker
  // threads, not in the same function.

  // Dequeue the buffer.
  v4l2_buffer result_buffer;
  res = dequeueBuffer(&result_buffer);
  if (res) {
    mGralloc.unlock(&result_buffer);
    return res;
  }
  // Now that we're done painting the buffer, we can unlock it.
  res = mGralloc.unlock(&result_buffer);
  if (res) {
    return res;
  }

  // All done, cleanup.
  // TODO(b/29334616): Lock around stream on/off access, only stop stream if
  // buffer queue is empty (synchronously, there's only ever 1 buffer in the
  // queue at a time, so this is safe).
  res = streamOff();
  if (res) {
    return res;
  }

  // Sanity check.
  if (result_buffer.m.userptr != device_buffer.m.userptr) {
    HAL_LOGE("Got the wrong buffer 0x%x back (expected 0x%x)",
             result_buffer.m.userptr, device_buffer.m.userptr);
    return -ENODEV;
  }

  return 0;
}

int V4L2Camera::dequeueBuffer(v4l2_buffer* buffer) {
  HAL_LOG_ENTER();

  memset(buffer, 0, sizeof(*buffer));
  buffer->type = mOutStreamType;
  buffer->memory = V4L2_MEMORY_USERPTR;
  if (ioctlLocked(VIDIOC_DQBUF, buffer) < 0) {
    HAL_LOGE("DQBUF fails: %s", strerror(errno));
    return -ENODEV;
  }
  return 0;
}

int V4L2Camera::getResultSettings(camera_metadata** metadata,
                                  uint64_t* timestamp) {
  HAL_LOG_ENTER();

  int res;
  android::CameraMetadata frame_metadata(*metadata);

  // TODO(b/30035628): fill in.
  // For now just spoof the timestamp to a non-0 value and send it back.
  int64_t frame_time = 1;
  res = frame_metadata.update(ANDROID_SENSOR_TIMESTAMP, &frame_time, 1);
  if (res != android::OK) {
    return res;
  }

  *metadata = frame_metadata.release();
  *timestamp = frame_time;

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

  android::CameraMetadata base_metadata;

  // Start with defaults for all templates.

  /* android.colorCorrection. */

  uint8_t aberration_mode = ANDROID_COLOR_CORRECTION_ABERRATION_MODE_FAST;
  res = base_metadata.update(ANDROID_COLOR_CORRECTION_ABERRATION_MODE,
                             &aberration_mode, 1);
  if (res != android::OK) {
    return res;
  }

  uint8_t color_correction_mode = ANDROID_COLOR_CORRECTION_MODE_FAST;
  res = base_metadata.update(ANDROID_COLOR_CORRECTION_MODE,
                             &color_correction_mode, 1);
  if (res != android::OK) {
    return res;
  }

  // transform and gains are for the unsupported MANUAL_POST_PROCESSING only.

  /* android.control. */

  /*   AE. */
  uint8_t ae_antibanding_mode = ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO;
  res = base_metadata.update(ANDROID_CONTROL_AE_ANTIBANDING_MODE,
                             &ae_antibanding_mode, 1);
  if (res != android::OK) {
    return res;
  }

  // Only matters if AE_MODE = OFF
  int32_t ae_exposure_compensation = 0;
  res = base_metadata.update(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION,
                             &ae_exposure_compensation, 1);
  if (res != android::OK) {
    return res;
  }

  uint8_t ae_lock = ANDROID_CONTROL_AE_LOCK_OFF;
  res = base_metadata.update(ANDROID_CONTROL_AE_LOCK, &ae_lock, 1);
  if (res != android::OK) {
    return res;
  }

  uint8_t ae_mode = ANDROID_CONTROL_AE_MODE_ON;
  res = base_metadata.update(ANDROID_CONTROL_AE_MODE, &ae_mode, 1);
  if (res != android::OK) {
    return res;
  }

  // AE regions not supported.

  // FPS set per-template.

  uint8_t ae_precapture_trigger = ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_IDLE;
  res = base_metadata.update(ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER,
                             &ae_precapture_trigger, 1);
  if (res != android::OK) {
    return res;
  }

  /*   AF. */

  // AF mode set per-template.

  // AF regions not supported.

  uint8_t af_trigger = ANDROID_CONTROL_AF_TRIGGER_IDLE;
  res = base_metadata.update(ANDROID_CONTROL_AF_TRIGGER, &af_trigger, 1);
  if (res != android::OK) {
    return res;
  }

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
  res = base_metadata.update(ANDROID_CONTROL_AWB_MODE, &default_awb_mode, 1);
  if (res != android::OK) {
    return res;
  }

  // AWB regions not supported.

  /*   Other controls. */

  uint8_t effect_mode = ANDROID_CONTROL_EFFECT_MODE_OFF;
  res = base_metadata.update(ANDROID_CONTROL_EFFECT_MODE, &effect_mode, 1);
  if (res != android::OK) {
    return res;
  }

  uint8_t control_mode = ANDROID_CONTROL_MODE_AUTO;
  res = base_metadata.update(ANDROID_CONTROL_MODE, &control_mode, 1);
  if (res != android::OK) {
    return res;
  }

  uint8_t scene_mode = ANDROID_CONTROL_SCENE_MODE_DISABLED;
  res = base_metadata.update(ANDROID_CONTROL_SCENE_MODE,
                             &scene_mode, 1);
  if (res != android::OK) {
    return res;
  }

  uint8_t video_stabilization = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_OFF;
  res = base_metadata.update(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE,
                             &video_stabilization, 1);
  if (res != android::OK) {
    return res;
  }

  // postRawSensitivityBoost: RAW not supported, leave null.

  /* android.demosaic. */

  // mode marked FUTURE.

  /* android.edge. */

  uint8_t edge_mode = ANDROID_EDGE_MODE_FAST;
  res = base_metadata.update(ANDROID_EDGE_MODE, &edge_mode, 1);
  if (res != android::OK) {
    return res;
  }

  // strength marked FUTURE.

  /* android.flash. */

  // firingPower, firingTime marked FUTURE.

  uint8_t flash_mode = ANDROID_FLASH_MODE_OFF;
  res = base_metadata.update(ANDROID_FLASH_MODE, &flash_mode, 1);
  if (res != android::OK) {
    return res;
  }

  /* android.hotPixel. */

  uint8_t hp_mode = ANDROID_HOT_PIXEL_MODE_FAST;
  res = base_metadata.update(ANDROID_HOT_PIXEL_MODE, &hp_mode, 1);
  if (res != android::OK) {
    return res;
  }

  /* android.jpeg. */

  double gps_coords[] = {/*latitude*/0, /*longitude*/0, /*altitude*/0};
  res = base_metadata.update(ANDROID_JPEG_GPS_COORDINATES, gps_coords, 3);
  if (res != android::OK) {
    return res;
  }

  uint8_t gps_processing_method[] = "none";
  res = base_metadata.update(ANDROID_JPEG_GPS_PROCESSING_METHOD,
                             gps_processing_method,
                             ARRAY_SIZE(gps_processing_method));
  if (res != android::OK) {
    return res;
  }

  int64_t gps_timestamp = 0;
  res = base_metadata.update(ANDROID_JPEG_GPS_TIMESTAMP, &gps_timestamp, 1);
  if (res != android::OK) {
    return res;
  }

  // JPEG orientation is relative to sensor orientation (mOrientation).
  int32_t jpeg_orientation = 0;
  res = base_metadata.update(ANDROID_JPEG_ORIENTATION, &jpeg_orientation, 1);
  if (res != android::OK) {
    return res;
  }

  // 1-100, larger is higher quality.
  uint8_t jpeg_quality = 80;
  res = base_metadata.update(ANDROID_JPEG_QUALITY, &jpeg_quality, 1);
  if (res != android::OK) {
    return res;
  }

  // TODO(b/29580107): If thumbnail quality actually matters/can be adjusted,
  // adjust this.
  uint8_t thumbnail_quality = 80;
  res = base_metadata.update(ANDROID_JPEG_THUMBNAIL_QUALITY,
                             &thumbnail_quality, 1);
  if (res != android::OK) {
    return res;
  }

  // TODO(b/29580107): Choose a size matching the resolution.
  int32_t thumbnail_size[] = {0, 0};
  res = base_metadata.update(ANDROID_JPEG_THUMBNAIL_SIZE, thumbnail_size, 2);
  if (res != android::OK) {
    return res;
  }

  /* android.lens. */

  // Fixed values.
  res = base_metadata.update(ANDROID_LENS_APERTURE, &mAperture, 1);
  if (res != android::OK) {
    return res;
  }
  res = base_metadata.update(ANDROID_LENS_FILTER_DENSITY, &mFilterDensity, 1);
  if (res != android::OK) {
    return res;
  }
  res = base_metadata.update(ANDROID_LENS_FOCAL_LENGTH, &mFocalLength, 1);
  if (res != android::OK) {
    return res;
  }
  res = base_metadata.update(ANDROID_LENS_FOCUS_DISTANCE, &mFocusDistance, 1);
  if (res != android::OK) {
    return res;
  }

  uint8_t optical_stabilization = ANDROID_LENS_OPTICAL_STABILIZATION_MODE_OFF;
  res = base_metadata.update(ANDROID_LENS_OPTICAL_STABILIZATION_MODE,
                    &optical_stabilization, 1);
  if (res != android::OK) {
    return res;
  }

  /* android.noiseReduction. */

  uint8_t noise_reduction_mode = ANDROID_NOISE_REDUCTION_MODE_FAST;
  res = base_metadata.update(ANDROID_NOISE_REDUCTION_MODE,
                             &noise_reduction_mode, 1);
  if (res != android::OK) {
    return res;
  }

  // strength marked FUTURE.

  /* android.request. */

  // Request Id unused by the HAL for now, and these are just
  // templates, so just fill it in with a dummy.
  int32_t id = 0;
  res = base_metadata.update(ANDROID_REQUEST_ID, &id, 1);
  if (res != android::OK) {
    return res;
  }

  // metadataMode marked FUTURE.

  /* android.scaler. */

  // No cropping by default; use the full active array.
  res = base_metadata.update(ANDROID_SCALER_CROP_REGION, mPixelArraySize.data(),
                             mPixelArraySize.size());
  if (res != android::OK) {
    return res;
  }

  /* android.sensor. */

  // exposureTime, sensitivity, testPattern[Data,Mode] not supported.

  // Ignored when AE is OFF.
  int64_t frame_duration = 33333333L; // 1/30 s.
  res = base_metadata.update(ANDROID_SENSOR_FRAME_DURATION, &frame_duration, 1);
  if (res != android::OK) {
    return res;
  }

  /* android.shading. */

  uint8_t shading_mode = ANDROID_SHADING_MODE_FAST;
  res = base_metadata.update(ANDROID_SHADING_MODE, &shading_mode, 1);
  if (res != android::OK) {
    return res;
  }

  /* android.statistics. */

  uint8_t face_detect_mode = ANDROID_STATISTICS_FACE_DETECT_MODE_OFF;
  res = base_metadata.update(ANDROID_STATISTICS_FACE_DETECT_MODE,
                             &face_detect_mode, 1);
  if (res != android::OK) {
    return res;
  }

  // histogramMode, sharpnessMapMode marked FUTURE.

  uint8_t hp_map_mode = ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE_OFF;
  res = base_metadata.update(ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE,
                             &hp_map_mode, 1);
  if (res != android::OK) {
    return res;
  }

  uint8_t lens_shading_map_mode = ANDROID_STATISTICS_LENS_SHADING_MAP_MODE_OFF;
  res = base_metadata.update(ANDROID_STATISTICS_LENS_SHADING_MAP_MODE,
                             &lens_shading_map_mode, 1);
  if (res != android::OK) {
    return res;
  }

  /* android.tonemap. */

  // Tonemap only required for MANUAL_POST_PROCESSING capability.

  /* android.led. */

  uint8_t transmit = ANDROID_LED_TRANSMIT_ON;
  res = base_metadata.update(ANDROID_LED_TRANSMIT, &transmit, 1);
  if (res != android::OK) {
    return res;
  }

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

  for (uint8_t template_id = 1; template_id < CAMERA3_TEMPLATE_COUNT;
       ++template_id) {
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

    // Copy our base metadata and add the new items.
    android::CameraMetadata template_metadata(base_metadata);
    res = template_metadata.update(ANDROID_CONTROL_CAPTURE_INTENT, &intent, 1);
    if (res != android::OK) {
      return res;
    }
    res = template_metadata.update(ANDROID_CONTROL_AE_TARGET_FPS_RANGE,
                                   fps_range.data(), fps_range.size());
    if (res != android::OK) {
      return res;
    }
    res = template_metadata.update(ANDROID_CONTROL_AF_MODE, &af_mode, 1);
    if (res != android::OK) {
      return res;
    }

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
  }

  mTemplatesInitialized = true;
  return 0;
}

bool V4L2Camera::isSupportedStreamSet(default_camera_hal::Stream** streams,
                                      int count, uint32_t mode) {
  HAL_LOG_ENTER();

  if (mode != CAMERA3_STREAM_CONFIGURATION_NORMAL_MODE) {
    HAL_LOGE("Unsupported stream configuration mode: %d", mode);
    return false;
  }

  // This should be checked by the caller, but put here as a sanity check.
  if (count < 1) {
    HAL_LOGE("Must request at least 1 stream");
    return false;
  }

  // Count the number of streams of each type.
  int32_t num_input = 0;
  int32_t num_raw = 0;
  int32_t num_yuv = 0;
  int32_t num_jpeg = 0;
  for (int i = 0; i < count; ++i) {
    default_camera_hal::Stream* stream = streams[i];

    if (stream->isInputType()) {
      ++num_input;
    }

    if (stream->isOutputType()) {
      switch (halToV4L2PixelFormat(stream->getFormat())) {
        case V4L2_PIX_FMT_YUV420:
          ++num_yuv;
          break;
        case V4L2_PIX_FMT_JPEG:
          ++num_jpeg;
          break;
        default:
          // Note: no supported raw formats at this time.
          HAL_LOGE("Unsupported format for stream %d: %d", i, stream->getFormat());
          return false;
      }
    }
  }

  if (num_input > mMaxInputStreams || num_raw > mMaxRawOutputStreams ||
      num_yuv > mMaxYuvOutputStreams || num_jpeg > mMaxJpegOutputStreams) {
    HAL_LOGE("Invalid stream configuration: %d input, %d RAW, %d YUV, %d JPEG "
             "(max supported: %d input, %d RAW, %d YUV, %d JPEG)",
             mMaxInputStreams, mMaxRawOutputStreams, mMaxYuvOutputStreams,
             mMaxJpegOutputStreams, num_input, num_raw, num_yuv, num_jpeg);
    return false;
  }

  // TODO(b/29939583): The above logic should be all that's necessary,
  // but V4L2 doesn't actually support more than 1 stream at a time. So for now,
  // if not all streams are the same format and size, error. Note that this
  // means the HAL is not spec-compliant; the requested streams are technically
  // valid and it is not technically allowed to error once it has reached this
  // point.
  int format = streams[0]->getFormat();
  uint32_t width = streams[0]->getWidth();
  uint32_t height = streams[0]->getHeight();
  for (int i = 1; i < count; ++i) {
    const default_camera_hal::Stream* stream = streams[i];
    if (stream->getFormat() != format || stream->getWidth() != width ||
        stream->getHeight() != height) {
      HAL_LOGE("V4L2 only supports 1 stream configuration at a time "
               "(stream 0 is format %d, width %u, height %u, "
               "stream %d is format %d, width %u, height %u).",
               format, width, height, i, stream->getFormat(),
               stream->getWidth(), stream->getHeight());
      return false;
    }
  }

  return true;
}

int V4L2Camera::setupStream(default_camera_hal::Stream* stream,
                            uint32_t* max_buffers) {
  HAL_LOG_ENTER();

  if (stream->getRotation() != CAMERA3_STREAM_ROTATION_0) {
    HAL_LOGE("Rotation %d not supported", stream->getRotation());
    return -EINVAL;
  }

  // Doesn't matter what was requested, we always use dataspace V0_JFIF.
  // Note: according to camera3.h, this isn't allowed, but etalvala@google.com
  // claims it's underdocumented; the implementation lets the HAL overwrite it.
  stream->setDataSpace(HAL_DATASPACE_V0_JFIF);

  int ret = setFormat(stream);
  if (ret) {
    return ret;
  }

  // Only do buffer setup if we don't know our maxBuffers.
  if (mOutStreamMaxBuffers < 1) {
    ret = setupBuffers();
    if (ret) {
      return ret;
    }
  }

  // Sanity check.
  if (mOutStreamMaxBuffers < 1) {
    HAL_LOGE("HAL failed to determine max number of buffers.");
    return -ENODEV;
  }
  *max_buffers = mOutStreamMaxBuffers;

  return 0;
}

int V4L2Camera::setFormat(const default_camera_hal::Stream* stream) {
  HAL_LOG_ENTER();

  // Should be checked earlier; sanity check.
  if (stream->isInputType()) {
    HAL_LOGE("Input streams not supported.");
    return -EINVAL;
  }

  // TODO(b/30000211): Check if appropriate to do multi-planar capture instead.
  uint32_t desired_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  uint32_t desired_format = halToV4L2PixelFormat(stream->getFormat());
  uint32_t desired_width = stream->getWidth();
  uint32_t desired_height = stream->getHeight();

  // Check to make sure we're not already in the correct format.
  if (desired_type == mOutStreamType &&
      desired_format == mOutStreamFormat &&
      desired_width == mOutStreamWidth &&
      desired_height == mOutStreamHeight) {
    return 0;
  }

  // Not in the correct format, set our format.
  v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = desired_type;
  format.fmt.pix.width = desired_width;
  format.fmt.pix.height = desired_height;
  format.fmt.pix.pixelformat = desired_format;
  // TODO(b/29334616): When async, this will need to check if the stream
  // is on, and if so, lock it off while setting format.
  if (ioctlLocked(VIDIOC_S_FMT, &format) < 0) {
    HAL_LOGE("S_FMT failed: %s", strerror(errno));
    return -ENODEV;
  }
  // Check that the driver actually set to the requested values.
  if (format.type != desired_type ||
      format.fmt.pix.pixelformat != desired_format ||
      format.fmt.pix.width != desired_width ||
      format.fmt.pix.height != desired_height) {
    HAL_LOGE("Device doesn't support desired stream configuration.");
    return -EINVAL;
  }

  // Keep track of the new format.
  mOutStreamType = format.type;
  mOutStreamFormat = format.fmt.pix.pixelformat;
  mOutStreamWidth = format.fmt.pix.width;
  mOutStreamHeight = format.fmt.pix.height;
  mOutStreamBytesPerLine = format.fmt.pix.bytesperline;
  HAL_LOGV("New format: type %u, pixel format %u, width %u, height %u, "
           "bytes/line %u", mOutStreamType, mOutStreamFormat, mOutStreamWidth,
           mOutStreamHeight, mOutStreamBytesPerLine);

  // Since our format changed, our maxBuffers may be incorrect.
  mOutStreamMaxBuffers = 0;


  return 0;
}

int V4L2Camera::setupBuffers() {
  HAL_LOG_ENTER();

  // "Request" a buffer (since we're using a userspace buffer, this just
  // tells V4L2 to switch into userspace buffer mode).
  v4l2_requestbuffers req_buffers;
  memset(&req_buffers, 0, sizeof(req_buffers));
  req_buffers.type = mOutStreamType;
  req_buffers.memory = V4L2_MEMORY_USERPTR;
  req_buffers.count = 1;
  if (ioctlLocked(VIDIOC_REQBUFS, &req_buffers) < 0) {
    HAL_LOGE("REQBUFS failed: %s", strerror(errno));
    return -ENODEV;
  }

  // V4L2 will set req_buffers.count to a number of buffers it can handle.
  mOutStreamMaxBuffers = req_buffers.count;
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

  // TODO(b/29939583): V4L2 can only support 1 stream at a time.
  // For now, just reporting minimum allowable for LIMITED devices.
  mMaxRawOutputStreams = 0;
  mMaxYuvOutputStreams = 2;
  mMaxJpegOutputStreams = 1;
  // Reprocessing not supported.
  mMaxInputStreams = 0;

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
  // We want to find the smallest maximum frame duration amongst formats.
  mMaxFrameDuration = std::numeric_limits<int64_t>::max();
  int64_t min_yuv_frame_duration = std::numeric_limits<int64_t>::max();
  for (auto format : formats) {
    int32_t hal_format = V4L2ToHalPixelFormat(format);
    if (hal_format < 0) {
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

int V4L2Camera::V4L2ToHalPixelFormat(uint32_t v4l2_format) {
  // Translate V4L2 format to HAL format.
  int hal_format = -1;
  switch (v4l2_format) {
    case V4L2_PIX_FMT_JPEG:
      hal_format = HAL_PIXEL_FORMAT_BLOB;
      break;
    case V4L2_PIX_FMT_YUV420:
      hal_format = HAL_PIXEL_FORMAT_YCbCr_420_888;
      break;
    default:
      // Unrecognized format.
      break;
  }
  return hal_format;
}

uint32_t V4L2Camera::halToV4L2PixelFormat(int hal_format) {
  // Translate HAL format to V4L2 format.
  uint32_t v4l2_format = 0;
  switch (hal_format) {
    case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:  // fall-through.
    case HAL_PIXEL_FORMAT_YCbCr_420_888:
      v4l2_format = V4L2_PIX_FMT_YUV420;
      break;
    case HAL_PIXEL_FORMAT_BLOB:
      v4l2_format = V4L2_PIX_FMT_JPEG;
      break;
    default:
      // Unrecognized format.
      break;
  }
  return v4l2_format;
}

} // namespace v4l2_camera_hal
