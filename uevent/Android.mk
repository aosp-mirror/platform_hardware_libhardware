# Copyright 2008 The Android Open Source Project

ifeq ($(TARGET_SIMULATOR),true)
LOCAL_SRC_FILES += uevent/uevent_stub.c
else
LOCAL_SRC_FILES += uevent/uevent.c
endif
