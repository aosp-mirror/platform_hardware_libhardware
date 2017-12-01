LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := static-hal-check
LOCAL_SRC_FILES := struct-size.cpp struct-offset.cpp struct-last.cpp
LOCAL_SHARED_LIBRARIES := libhardware
LOCAL_CFLAGS := -O0 -Wall -Werror

LOCAL_C_INCLUDES += \
    system/media/camera/include

include $(BUILD_STATIC_LIBRARY)
