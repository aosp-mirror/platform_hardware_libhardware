#
#  Copyright (C) 2015 The Android Open Source Project
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

LOCAL_PATH:= $(call my-dir)

# Build native tests.
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    vehicle_tests.cpp \

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libhardware \

LOCAL_CFLAGS += -Wall -Wextra

LOCAL_MODULE:= vehicle_tests
LOCAL_MODULE_TAGS := tests

include $(BUILD_NATIVE_TEST)

# Build HAL command line utility.
include $(CLEAR_VARS)

LOCAL_SRC_FILES := vehicle-hal-tool.c
LOCAL_MODULE := vehicle-hal-tool
LOCAL_CFLAGS := -Wall -Wno-unused-parameter -Werror
LOCAL_MODULE_TAGS := tests

LOCAL_SHARED_LIBRARIES := libcutils libhardware liblog

include $(BUILD_EXECUTABLE)
