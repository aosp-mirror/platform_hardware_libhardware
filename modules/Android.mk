hardware_modules := gralloc hwcomposer \
	usbaudio audio_remote_submix camera usbcamera sensors input
include $(call all-named-subdir-makefiles,$(hardware_modules))
