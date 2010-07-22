# Copyright 2006 The Android Open Source Project

# Setting LOCAL_PATH will mess up all-subdir-makefiles, so do it beforehand.

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SHARED_LIBRARIES := libcutils

LOCAL_INCLUDES += $(LOCAL_PATH)

ifneq ($(TARGET_SIMULATOR),true)
  LOCAL_CFLAGS  += -DQEMU_HARDWARE
  QEMU_HARDWARE := true
endif

ifneq ($(TARGET_SIMULATOR),true)
LOCAL_SHARED_LIBRARIES += libdl
endif

LOCAL_SRC_FILES += hardware.c

# need "-lrt" on Linux simulator to pick up clock_gettime
ifeq ($(TARGET_SIMULATOR),true)
	ifeq ($(HOST_OS),linux)
		LOCAL_LDLIBS += -lrt -lpthread -ldl
	endif
endif

LOCAL_MODULE:= libhardware

include $(BUILD_SHARED_LIBRARY)

include $(addsuffix /Android.mk, $(addprefix $(LOCAL_PATH)/, \
			modules/gralloc \
			tests \
		))
		