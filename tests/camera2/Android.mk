LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	camera2.cpp \
	camera2_utils.cpp

LOCAL_SHARED_LIBRARIES := \
	libutils \
	libstlport \
	libhardware \
	libcamera_metadata \
	libgui \
	libsync \
	libui

LOCAL_STATIC_LIBRARIES := \
	libgtest \
	libgtest_main

LOCAL_C_INCLUDES += \
	bionic \
	bionic/libstdc++/include \
	external/gtest/include \
	external/stlport/stlport \
	system/media/camera/include \

LOCAL_MODULE:= camera2_test
LOCAL_MODULE_TAGS := tests

include $(BUILD_EXECUTABLE)
