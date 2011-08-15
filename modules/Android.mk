hardware_modules := gralloc hwcomposer audio local_time
include $(call all-named-subdir-makefiles,$(hardware_modules))
