# Copyright 2006 The Android Open Source Project

ifeq ($(TARGET_PRODUCT),sooner)
LOCAL_SRC_FILES += led/led_sardine.c
else
ifeq ($(TARGET_PRODUCT),dream)
LOCAL_SRC_FILES += led/led_trout.c
else
LOCAL_SRC_FILES += led/led_stub.c
endif
endif

