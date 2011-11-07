hardware_modules := gralloc hwcomposer audio nfc
include $(call all-named-subdir-makefiles,$(hardware_modules))
