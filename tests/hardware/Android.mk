LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := struct-size
LOCAL_SRC_FILES := struct-size.cpp
LOCAL_SHARED_LIBRARIES := libhardware
LOCAL_CFLAGS := -std=gnu++11 -O0

LOCAL_C_INCLUDES += \
    system/media/camera/include

include $(BUILD_STATIC_LIBRARY)
