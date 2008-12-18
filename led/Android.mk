# Copyright 2006 The Android Open Source Project

ifeq ($(TARGET_DEVICE),sooner)
LOCAL_SRC_FILES += led/led_sardine.c
LOCAL_CFLAGS    += -DCONFIG_LED_SARDINE
endif
ifeq ($(TARGET_DEVICE),dream)
LOCAL_SRC_FILES += led/led_trout.c
LOCAL_CFLAGS    += -DCONFIG_LED_TROUT
endif
ifeq ($(QEMU_HARDWARE),true)
LOCAL_CFLAGS    += -DCONFIG_LED_QEMU
endif
LOCAL_SRC_FILES += led/led_stub.c

