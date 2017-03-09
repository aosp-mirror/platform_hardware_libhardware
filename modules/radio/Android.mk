# Copyright (C) 2015 The Android Open Source Project
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

LOCAL_PATH := $(call my-dir)

# Stub radio HAL module, used for tests
include $(CLEAR_VARS)

LOCAL_MULTILIB := $(AUDIOSERVER_MULTILIB)

LOCAL_MODULE := radio.fm.default
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_PROPRIETARY_MODULE := true
LOCAL_SRC_FILES := radio_hw.c
LOCAL_SHARED_LIBRARIES := liblog libcutils libradio_metadata
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

# Stub radio tool that can be run in native.
include $(CLEAR_VARS)

LOCAL_MULTILIB := $(AUDIOSERVER_MULTILIB)

LOCAL_MODULE := radio_hal_tool
LOCAL_SRC_FILES := radio_hal_tool.c
LOCAL_CFLAGS := -Wall -Wno-unused-parameter -Werror
LOCAL_SHARED_LIBRARIES := libcutils libhardware liblog libradio_metadata

include $(BUILD_EXECUTABLE)
