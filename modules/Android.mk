hardware_modules := gralloc hwcomposer audio
include $(call all-named-subdir-makefiles,$(hardware_modules))
