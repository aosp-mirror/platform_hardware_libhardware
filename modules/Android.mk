hardware_modules := gralloc hwcomposer audio nfc nfc-nci local_time \
	power usbaudio audio_remote_submix camera usbcamera consumerir sensors vibrator \
	tv_input fingerprint input
include $(call all-named-subdir-makefiles,$(hardware_modules))
