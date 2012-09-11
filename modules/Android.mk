hardware_modules := gralloc hwcomposer audio nfc local_time power usbaudio audio_remote_submix
include $(call all-named-subdir-makefiles,$(hardware_modules))
