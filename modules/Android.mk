hardware_modules := gralloc hwcomposer audio nfc local_time power usbaudio
include $(call all-named-subdir-makefiles,$(hardware_modules))
