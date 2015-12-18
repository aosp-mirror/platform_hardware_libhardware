LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES += hardware/libhardware/modules/input/evdev
LOCAL_C_INCLUDES += $(TOP)/external/gmock/include

LOCAL_SRC_FILES:= \
    BitUtils_test.cpp \
    InputDevice_test.cpp \
    InputHub_test.cpp \
    InputMocks.cpp \
    MouseInputMapper_test.cpp \
    SwitchInputMapper_test.cpp \
    TestHelpers.cpp

LOCAL_STATIC_LIBRARIES := libgmock

LOCAL_SHARED_LIBRARIES := \
    libinput_evdev \
    liblog \
    libutils

LOCAL_CLANG := true
LOCAL_CFLAGS += -Wall -Wextra -Wno-unused-parameter
LOCAL_CPPFLAGS += -std=c++14

# TestHelpers uses mktemp. As the path is given to TempFile, we can't do too much
# here (e.g., use mkdtemp first). At least races will lead to an early failure, as
# mkfifo fails on existing files.
LOCAL_CFLAGS += -Wno-deprecated-declarations

LOCAL_MODULE := libinput_evdevtests
LOCAL_MODULE_TAGS := tests

include $(BUILD_NATIVE_TEST)
