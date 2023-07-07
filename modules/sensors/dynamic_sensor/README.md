# Dynamic Sensors

[TOC]

## Links

*   [Sensor HAL dynamic sensor support](https://source.android.com/devices/sensors/sensors-hal2#dynamic-sensors)
*   [Sensors Multi-HAL](https://source.android.com/devices/sensors/sensors-multihal)

## Adding dynamic sensor support to a device

A few files need to be modified to add dynamic sensor support to a device. The
dynamic sensor HAL must be enabled in the device product makefile and in the
sensor sub-HAL configuration file, support for raw HID devices must be configured
in the Linux kernel (`CONFIG_HIDRAW=y`), and SELinux policy files must be updated
to provide the necessary permissions. Example changes are provided below.

```shell
acme-co$ git -C device/acme/rocket-phone diff
diff --git a/sensor_hal/hals.conf b/sensor_hal/hals.conf
index a1f4b8b..d112546 100644
--- a/sensor_hal/hals.conf
+++ b/sensor_hal/hals.conf
@@ -1 +1,2 @@
+sensors.dynamic_sensor_hal.so
 sensors.rocket-phone.so
diff --git a/rocket-phone.mk b/rocket-phone.mk
index 3fc8538..b1bd8a1 100644
--- a/rocket-phone.mk
+++ b/rocket-phone.mk
@@ -73,6 +73,9 @@
 PRODUCT_PACKAGES += sensors.rocket-phone
 PRODUCT_PACKAGES += thruster_stats

+# Add the dynamic sensor HAL.
+PRODUCT_PACKAGES += sensors.dynamic_sensor_hal
+
 # Only install test tools in debug build or eng build.
 ifneq ($(filter userdebug eng,$(TARGET_BUILD_VARIANT)),)
   PRODUCT_PACKAGES += thruster_test
diff --git a/conf/ueventd.rc b/conf/ueventd.rc
index 88ee00b..2f03009 100644
--- a/conf/ueventd.rc
+++ b/conf/ueventd.rc
@@ -209,3 +209,7 @@

 # Thrusters
 /dev/thruster*                         0600   system     system
+
+# Raw HID devices
+/dev/hidraw*                           0660   system     system
+
diff --git a/sepolicy/sensor_hal.te b/sepolicy/sensor_hal.te
index 0797253..22a4208 100644
--- a/sepolicy/sensor_hal.te
+++ b/sepolicy/sensor_hal.te
@@ -52,6 +52,9 @@
 # Allow sensor HAL to read thruster state.
 allow hal_sensors_default thruster_state:file r_file_perms;

+# Allow access for dynamic sensor properties.
+get_prop(hal_sensors_default, vendor_dynamic_sensor_prop)
+
+# Allow access to raw HID devices for dynamic sensors.
+allow hal_sensors_default device:dir r_dir_perms;
+allow hal_sensors_default hidraw_device:chr_file rw_file_perms;
+
 #
 # Thruster sensor enforcements.
 #
diff --git a/sepolicy/device.te b/sepolicy/device.te
index bc3c947..bad0be0 100644
--- a/sepolicy/device.te
+++ b/sepolicy/device.te
@@ -55,3 +55,7 @@

 # Thruster
 type thruster_device, dev_type;
+
+# Raw HID device
+type hidraw_device, dev_type;
+
diff --git a/sepolicy/property.te b/sepolicy/property.te
index 4b671a4..bb0894f 100644
--- a/sepolicy/property.te
+++ b/sepolicy/property.te
@@ -49,3 +49,7 @@

 # Thruster
 vendor_internal_prop(vendor_thruster_debug_prop)
+
+# Dynamic sensor
+vendor_internal_prop(vendor_dynamic_sensor_prop)
+
diff --git a/sepolicy/file_contexts b/sepolicy/file_contexts
index bc03a78..ff401dc 100644
--- a/sepolicy/file_contexts
+++ b/sepolicy/file_contexts
@@ -441,3 +441,7 @@
 /dev/thruster-fuel                  u:object_r:thruster_device:s0
 /dev/thruster-output                u:object_r:thruster_device:s0
 /dev/thruster-telemetry             u:object_r:thruster_device:s0
+
+# Raw HID device
+/dev/hidraw[0-9]*                  u:object_r:hidraw_device:s0
+
diff --git a/sepolicy/property_contexts b/sepolicy/property_contexts
index 5d2f018..18a6059 100644
--- a/sepolicy/property_contexts
+++ b/sepolicy/property_contexts
@@ -104,3 +104,7 @@

 # Thruster
 vendor.thruster.debug                           u:object_r:vendor_thruster_debug_prop:s0
+
+# Dynamic sensor
+vendor.dynamic_sensor.                          u:object_r:vendor_dynamic_sensor_prop:s0
+
acme-co$
```

Once the file modifications are made, rebuild and flash. The dynamic sensor HAL
should be initialized and appear in the sensor service.

```shell
acme-co$ build_and_flash_android
...
acme-co$ adb logcat -d | grep DynamicSensorHal
12-15 18:18:45.735   791   791 D DynamicSensorHal: DynamicSensorsSubHal::getSensorsList_2_1 invoked.
12-15 18:18:47.474   791   791 D DynamicSensorHal: DynamicSensorsSubHal::initialize invoked.
acme-co$ adb shell dumpsys sensorservice | grep Dynamic
0000000000) Dynamic Sensor Manager    | Google          | ver: 1 | type: android.sensor.dynamic_sensor_meta(32) | perm: n/a | flags: 0x00000007
Dynamic Sensor Manager (handle=0x00000000, connections=1)
         Dynamic Sensor Manager 0x00000000 | status: active | pending flush events 0
```

When a dynamic sensor is paired with the device (e.g., Bluetooth rocket buds),
it will appear in the sensor service.

```shell
acme-co$ adb logcat -d | grep "DynamicSensorHal\|hidraw\|Rocket"
12-15 18:19:55.268   157   157 I hid-generic 0003: 1234:5678.0001: hidraw0: BLUETOOTH HID v0.00 Device [RocketBuds] on
12-15 18:19:55.235   791   809 E DynamicSensorHal: return 1 sensors
12-15 18:19:56.239  1629  1787 I SensorService: Dynamic sensor handle 0x1 connected, type 37, name RocketBuds
acme-co$ adb shell dumpsys sensorservice | grep head_tracker
0x00000001) RocketBuds                | BLUETOOTH 1234:5678   | ver: 1 | type: android.sensor.head_tracker(37) | perm: n/a | flags: 0x00000020
acme-co$
```

