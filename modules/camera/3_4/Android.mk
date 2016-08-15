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
  libcamera_client \
  libcamera_metadata \
  libcutils \
  libhardware \
  liblog \
  libnativehelper \
  libsync \
  libutils \

v4l2_static_libs :=

v4l2_cflags := -fno-short-enums -Wall -Wextra -fvisibility=hidden

v4l2_c_includes := $(call include-path-for, camera)

v4l2_src_files := \
  camera.cpp \
  metadata/enum_converter.cpp \
  metadata/metadata.cpp \
  metadata/v4l2_enum_control.cpp \
  stream.cpp \
  stream_format.cpp \
  v4l2_camera.cpp \
  v4l2_camera_hal.cpp \
  v4l2_gralloc.cpp \
  v4l2_metadata.cpp \
  v4l2_wrapper.cpp \

v4l2_test_files := \
  metadata/control_test.cpp \
  metadata/enum_converter_test.cpp \
  metadata/fixed_property_test.cpp \
  metadata/ignored_control_test.cpp \
  metadata/map_converter_test.cpp \
  metadata/menu_control_options_test.cpp \
  metadata/metadata_test.cpp \
  metadata/optioned_control_test.cpp \
  metadata/ranged_converter_test.cpp \
  metadata/slider_control_options_test.cpp \
  metadata/v4l2_enum_control_test.cpp \

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
  libBionicGtestMain \
  libgmock \
  $(v4l2_static_libs) \

LOCAL_C_INCLUDES += $(v4l2_c_includes)
LOCAL_SRC_FILES := \
  $(v4l2_src_files) \
  $(v4l2_test_files) \

include $(BUILD_NATIVE_TEST)

endif # USE_CAMERA_V4L2_HAL
