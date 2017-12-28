hardware_modules := \
    camera \
    gralloc \
    sensors
include $(call all-named-subdir-makefiles,$(hardware_modules))
