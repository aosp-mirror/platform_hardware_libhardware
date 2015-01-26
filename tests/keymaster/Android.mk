# Build the keymaster unit tests

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    keymaster_test.cpp

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libutils \
    libcrypto \
    libhardware \

LOCAL_MODULE := keymaster_test

LOCAL_MODULE_TAGS := tests

include $(BUILD_NATIVE_TEST)
