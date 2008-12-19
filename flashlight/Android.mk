# Copyright 2007 The Android Open Source Project

LOCAL_SRC_FILES += flashlight/flashlight.c
ifeq ($(QEMU_HARDWARE),yes)
  LOCAL_CFLAGS += -DQEMU_FLASHLIGHT
endif
