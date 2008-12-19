# Copyright 2006 The Android Open Source Project

LOCAL_SRC_FILES += power/power.c

ifeq ($(QEMU_HARDWARE),true)
  LOCAL_SRC_FILES += power/power_qemu.c
  LOCAL_CFLAGS    += -DQEMU_POWER=1
endif
