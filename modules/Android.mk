hardware_modules := gralloc hwcomposer
include $(call all-named-subdir-makefiles,$(hardware_modules))
