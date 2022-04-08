#
# Copyright 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

LOCAL_PATH := $(call my-dir)

# Prevent the HAL from building on devices not specifically
# requesting to use it.
ifeq ($(USE_CAMERA_V4L2_HAL), true)

v4l2_shared_libs := \
  libbase \
  libchrome \
  libcamera_client \
  libcamera_metadata \
  libcutils \
  libexif \
  libhardware \
  liblog \
  libsync \
  libutils \

v4l2_static_libs := \
  libyuv_static \
  libjpeg_static_ndk \

v4l2_cflags := -fno-short-enums -Wall -Wextra -Werror -fvisibility=hidden -DHAVE_JPEG

v4l2_c_includes := $(call include-path-for, camera) \
  external/libyuv/files/include \

v4l2_src_files := \
  arc/cached_frame.cpp \
  arc/exif_utils.cpp \
  arc/frame_buffer.cpp \
  arc/image_processor.cpp \
  arc/jpeg_compressor.cpp \
  camera.cpp \
  capture_request.cpp \
  format_metadata_factory.cpp \
  metadata/boottime_state_delegate.cpp \
  metadata/enum_converter.cpp \
  metadata/metadata.cpp \
  metadata/metadata_reader.cpp \
  request_tracker.cpp \
  static_properties.cpp \
  stream_format.cpp \
  v4l2_camera.cpp \
  v4l2_camera_hal.cpp \
  v4l2_metadata_factory.cpp \
  v4l2_wrapper.cpp \

v4l2_test_files := \
  format_metadata_factory_test.cpp \
  metadata/control_test.cpp \
  metadata/default_option_delegate_test.cpp \
  metadata/enum_converter_test.cpp \
  metadata/ignored_control_delegate_test.cpp \
  metadata/map_converter_test.cpp \
  metadata/menu_control_options_test.cpp \
  metadata/metadata_reader_test.cpp \
  metadata/metadata_test.cpp \
  metadata/no_effect_control_delegate_test.cpp \
  metadata/partial_metadata_factory_test.cpp \
  metadata/property_test.cpp \
  metadata/ranged_converter_test.cpp \
  metadata/slider_control_options_test.cpp \
  metadata/state_test.cpp \
  metadata/tagged_control_delegate_test.cpp \
  metadata/tagged_control_options_test.cpp \
  metadata/v4l2_control_delegate_test.cpp \
  request_tracker_test.cpp \
  static_properties_test.cpp \

# V4L2 Camera HAL.
# ==============================================================================
include $(CLEAR_VARS)
LOCAL_MODULE := camera.v4l2
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_CFLAGS += $(v4l2_cflags)
LOCAL_SHARED_LIBRARIES := $(v4l2_shared_libs)
LOCAL_STATIC_LIBRARIES := \
  libgtest_prod \
  $(v4l2_static_libs) \

LOCAL_C_INCLUDES += $(v4l2_c_includes)
LOCAL_SRC_FILES := $(v4l2_src_files)
include $(BUILD_SHARED_LIBRARY)

# Unit tests for V4L2 Camera HAL.
# ==============================================================================
include $(CLEAR_VARS)
LOCAL_MODULE := camera.v4l2_test
LOCAL_CFLAGS += $(v4l2_cflags)
LOCAL_SHARED_LIBRARIES := $(v4l2_shared_libs)
LOCAL_STATIC_LIBRARIES := \
  libgmock \
  $(v4l2_static_libs) \

LOCAL_C_INCLUDES += $(v4l2_c_includes)
LOCAL_SRC_FILES := \
  $(v4l2_src_files) \
  $(v4l2_test_files) \

include $(BUILD_NATIVE_TEST)

endif # USE_CAMERA_V4L2_HAL
