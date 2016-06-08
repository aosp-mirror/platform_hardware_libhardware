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
include $(CLEAR_VARS)

LOCAL_MODULE := camera.v4l2
LOCAL_MODULE_RELATIVE_PATH := hw

LOCAL_CFLAGS += -fno-short-enums

# Note: see V4L2 HALv1 implementation when adding YUV support,
#   some various unexpected variables had to be set.

LOCAL_SHARED_LIBRARIES := \
    libcamera_client \
    libcamera_metadata \
    libcutils \
    liblog \
    libsync \
    libutils \

LOCAL_STATIC_LIBRARIES :=

LOCAL_C_INCLUDES += \
    $(call include-path-for, camera)

LOCAL_SRC_FILES := \
  Camera.cpp \
  Stream.cpp \
  V4L2Camera.cpp \
  V4L2CameraHAL.cpp \

LOCAL_CFLAGS += -Wall -Wextra -fvisibility=hidden

include $(BUILD_SHARED_LIBRARY)
