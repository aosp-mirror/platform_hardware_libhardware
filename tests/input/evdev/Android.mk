LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES += hardware/libhardware/modules/input/evdev

LOCAL_SRC_FILES:= \
    InputDevice_test.cpp \
    InputHub_test.cpp \
    TestHelpers.cpp

LOCAL_SHARED_LIBRARIES := \
    libinput_evdev \
    liblog \
    libutils

LOCAL_CLANG := true
LOCAL_CFLAGS += -Wall -Wextra -Wno-unused-parameter
LOCAL_CPPFLAGS += -std=c++14

LOCAL_MODULE := libinput_evdevtests
LOCAL_MODULE_TAGS := tests

include $(BUILD_NATIVE_TEST)
