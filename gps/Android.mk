# the Dream and Surf use the Qualcom GPS stuff
#

ifneq ($(BOARD_GPS_LIBRARIES),)
  LOCAL_CFLAGS           += -DHAVE_GPS_HARDWARE
  LOCAL_SHARED_LIBRARIES += $(BOARD_GPS_LIBRARIES)
endif

# always compile the emulator-specific hardware for now
#
USE_QEMU_GPS_HARDWARE := true

ifeq ($(USE_QEMU_GPS_HARDWARE),true)
    LOCAL_CFLAGS    += -DHAVE_QEMU_GPS_HARDWARE
    LOCAL_SRC_FILES += gps/gps_qemu.c
endif

LOCAL_SRC_FILES += gps/gps.cpp

