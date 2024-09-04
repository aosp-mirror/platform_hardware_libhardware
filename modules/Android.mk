hardware_modules := \
    camera \
    sensors
include $(call all-named-subdir-makefiles,$(hardware_modules))
