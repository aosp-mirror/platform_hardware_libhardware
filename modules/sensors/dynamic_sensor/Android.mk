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

dynamic_sensor_src := \
    BaseDynamicSensorDaemon.cpp \
    BaseSensorObject.cpp \
    ConnectionDetector.cpp \
    DummyDynamicAccelDaemon.cpp \
    DynamicSensorManager.cpp \
    HidRawDevice.cpp \
    HidRawSensor.cpp \
    HidRawSensorDaemon.cpp \
    HidRawSensorDevice.cpp \
    RingBuffer.cpp

dynamic_sensor_shared_lib := \
    libbase \
    libcutils \
    libhidparser \
    liblog \
    libutils
#
# There are two ways to utilize the dynamic sensor module:
#   1. Use as an extension in an existing hal: declare dependency on libdynamic_sensor_ext shared
#      library in existing sensor hal.
#   2. Use as a standalone sensor HAL and configure multihal to combine it with sensor hal that
#      hosts other sensors: add dependency on sensors.dynamic_sensor_hal in device level makefile and
#      write multihal configuration file accordingly.
#
# Please take only one of these two options to avoid conflict over hardware resource.
#
#
# Option 1: sensor extension module
#
include $(CLEAR_VARS)
LOCAL_MODULE := libdynamic_sensor_ext
LOCAL_MODULE_TAGS := optional
# intended to be integrated into hal, thus proprietary
LOCAL_PROPRIETARY_MODULE := true

LOCAL_CFLAGS += $(COMMON_CFLAGS) -DLOG_TAG=\"DynamicSensorExt\"

LOCAL_SRC_FILES := $(dynamic_sensor_src)

LOCAL_SHARED_LIBRARIES := $(dynamic_sensor_shared_lib)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)

include $(BUILD_SHARED_LIBRARY)

#
# Option 2: independent sensor hal
#
include $(CLEAR_VARS)
LOCAL_MODULE := sensors.dynamic_sensor_hal
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_TAGS := optional
# hal module, thus proprietary
LOCAL_PROPRIETARY_MODULE := true

LOCAL_CFLAGS += $(COMMON_CFLAGS) -DLOG_TAG=\"DynamicSensorHal\"

LOCAL_SRC_FILES := $(dynamic_sensor_src) sensors.cpp

LOCAL_SHARED_LIBRARIES := $(dynamic_sensor_shared_lib)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)

include $(BUILD_SHARED_LIBRARY)

#
# Host test for HidRawSensor. Test with dummy test HID descriptors.
#
include $(CLEAR_VARS)
LOCAL_MODULE := hidrawsensor_host_test
LOCAL_MODULE_TAGS := optional
# host test is targeting linux host only
LOCAL_MODULE_HOST_OS := linux

LOCAL_CFLAGS += $(COMMON_CFLAGS)

LOCAL_SRC_FILES := \
    HidRawSensor.cpp \
    BaseSensorObject.cpp \
    HidUtils/test/TestHidDescriptor.cpp \
    test/HidRawSensorTest.cpp

LOCAL_SHARED_LIBRARIES := libhidparser_host

LOCAL_C_INCLUDES += $(LOCAL_PATH)/test $(LOCAL_PATH)/HidUtils/test
include $(BUILD_HOST_EXECUTABLE)

#
# Host test for HidRawDevice and HidRawSensor. Test with hidraw
# device node.
#
include $(CLEAR_VARS)
LOCAL_MODULE := hidrawdevice_host_test
LOCAL_MODULE_TAGS := optional
# host test is targeting linux host only
LOCAL_MODULE_HOST_OS := linux

LOCAL_CFLAGS += $(COMMON_CFLAGS)

LOCAL_SRC_FILES := \
    HidRawDevice.cpp \
    HidRawSensor.cpp \
    BaseSensorObject.cpp \
    test/HidRawDeviceTest.cpp

LOCAL_SHARED_LIBRARIES := libhidparser_host

LOCAL_C_INCLUDES += $(LOCAL_PATH)/test $(LOCAL_PATH)/HidUtils/test
include $(BUILD_HOST_EXECUTABLE)

include $(LOCAL_PATH)/HidUtils/Android.mk
