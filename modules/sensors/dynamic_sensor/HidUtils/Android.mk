# Copyright (C) 2017 The Android Open Source Project
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
COMMON_CFLAGS := -Wall -Werror -Wextra

hidparser_src := \
    HidGlobal.cpp \
    HidItem.cpp \
    HidLocal.cpp \
    HidParser.cpp \
    HidReport.cpp \
    HidTree.cpp

include $(CLEAR_VARS)
LOCAL_MODULE := libhidparser
LOCAL_MODULE_TAGS := optional
# indended to be used by hal components, thus propietary
LOCAL_PROPRIETARY_MODULE := true

LOCAL_CFLAGS += $(COMMON_CFLAGS) -DLOG_TAG=\"HidUtil\"
LOCAL_SRC_FILES := $(hidparser_src)
LOCAL_SHARED_LIBRARIES := libbase
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)
include $(BUILD_SHARED_LIBRARY)

#
# host side shared library (for host test, example, etc)
#
include $(CLEAR_VARS)
LOCAL_MODULE := libhidparser_host
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS += $(COMMON_CFLAGS)

LOCAL_SRC_FILES := $(hidparser_src)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)
include $(BUILD_HOST_SHARED_LIBRARY)

#
# Example of HidParser
#
include $(CLEAR_VARS)
LOCAL_MODULE := hidparser_example
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS += $(COMMON_CFLAGS)
LOCAL_SRC_FILES := \
    $(hidparser_src) \
    test/HidParserExample.cpp \
    test/TestHidDescriptor.cpp

LOCAL_C_INCLUDES += $(LOCAL_PATH)/test
include $(BUILD_HOST_EXECUTABLE)

#
# Another example of HidParser
#
include $(CLEAR_VARS)
LOCAL_MODULE := hidparser_example2
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS += $(COMMON_CFLAGS)
LOCAL_SRC_FILES := \
    $(hidparser_src) \
    test/HidParserExample2.cpp \
    test/TestHidDescriptor.cpp

LOCAL_C_INCLUDES += $(LOCAL_PATH)/test
include $(BUILD_HOST_EXECUTABLE)

#
# Test for TriState template
#
include $(CLEAR_VARS)
LOCAL_MODULE := tristate_test
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS += $(COMMON_CFLAGS)
LOCAL_SRC_FILES := test/TriStateTest.cpp

LOCAL_STATIC_LIBRARIES := \
     libgtest \
     libgtest_main

LOCAL_C_INCLUDES += $(LOCAL_PATH)/test
include $(BUILD_HOST_NATIVE_TEST)
