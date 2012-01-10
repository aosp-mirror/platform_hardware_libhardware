hardware_modules := gralloc hwcomposer audio nfc local_time power
include $(call all-named-subdir-makefiles,$(hardware_modules))
