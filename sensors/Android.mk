# Copyright 2008 The Android Open Source Project

ifeq ($(TARGET_PRODUCT),dream)
LOCAL_SRC_FILES += sensors/sensors_trout.c
else
LOCAL_SRC_FILES += sensors/sensors_stub.c
endif

