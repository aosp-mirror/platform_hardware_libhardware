hardware_modules := gralloc hwcomposer audio local_time nfc
include $(call all-named-subdir-makefiles,$(hardware_modules))
