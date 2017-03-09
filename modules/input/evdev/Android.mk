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

# Evdev module implementation
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    BitUtils.cpp \
    InputHub.cpp \
    InputDevice.cpp \
    InputDeviceManager.cpp \
    InputHost.cpp \
    InputMapper.cpp \
    MouseInputMapper.cpp \
    SwitchInputMapper.cpp

LOCAL_SHARED_LIBRARIES := \
    libhardware_legacy \
    liblog \
    libutils

LOCAL_CLANG := true
LOCAL_CPPFLAGS += -std=c++14 -Wno-unused-parameter

LOCAL_MODULE := libinput_evdev
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

# HAL module
include $(CLEAR_VARS)

LOCAL_MODULE := input.evdev.default
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_PROPRIETARY_MODULE := true

LOCAL_SRC_FILES := \
    EvdevModule.cpp

LOCAL_SHARED_LIBRARIES := \
    libinput_evdev \
    liblog

LOCAL_CLANG := true
LOCAL_CPPFLAGS += -std=c++14 -Wno-unused-parameter

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
