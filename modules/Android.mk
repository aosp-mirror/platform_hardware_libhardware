hardware_modules := gralloc hwcomposer \
	usbaudio audio_remote_submix camera usbcamera sensors \
	input vehicle thermal vr
include $(call all-named-subdir-makefiles,$(hardware_modules))
