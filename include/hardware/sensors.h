/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_SENSORS_INTERFACE_H
#define ANDROID_SENSORS_INTERFACE_H

#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#include <hardware/hardware.h>
#include <cutils/native_handle.h>

__BEGIN_DECLS

/*****************************************************************************/

#define SENSORS_HEADER_VERSION          1
#define SENSORS_MODULE_API_VERSION_0_1  HARDWARE_MODULE_API_VERSION(0, 1)
#define SENSORS_DEVICE_API_VERSION_0_1  HARDWARE_DEVICE_API_VERSION_2(0, 1, SENSORS_HEADER_VERSION)
#define SENSORS_DEVICE_API_VERSION_1_0  HARDWARE_DEVICE_API_VERSION_2(1, 0, SENSORS_HEADER_VERSION)
#define SENSORS_DEVICE_API_VERSION_1_1  HARDWARE_DEVICE_API_VERSION_2(1, 1, SENSORS_HEADER_VERSION)
#define SENSORS_DEVICE_API_VERSION_1_2  HARDWARE_DEVICE_API_VERSION_2(1, 2, SENSORS_HEADER_VERSION)
#define SENSORS_DEVICE_API_VERSION_1_3  HARDWARE_DEVICE_API_VERSION_2(1, 3, SENSORS_HEADER_VERSION)
#define SENSORS_DEVICE_API_VERSION_1_4  HARDWARE_DEVICE_API_VERSION_2(1, 4, SENSORS_HEADER_VERSION)

/**
 * Please see the Sensors section of source.android.com for an
 * introduction to and detailed descriptions of Android sensor types:
 * http://source.android.com/devices/sensors/index.html
 */

/**
 * The id of this module
 */
#define SENSORS_HARDWARE_MODULE_ID "sensors"

/**
 * Name of the sensors device to open
 */
#define SENSORS_HARDWARE_POLL       "poll"

/**
 * Handles must be higher than SENSORS_HANDLE_BASE and must be unique.
 * A Handle identifies a given sensors. The handle is used to activate
 * and/or deactivate sensors.
 * In this version of the API there can only be 256 handles.
 */
#define SENSORS_HANDLE_BASE             0
#define SENSORS_HANDLE_BITS             8
#define SENSORS_HANDLE_COUNT            (1<<SENSORS_HANDLE_BITS)


/*
 * **** Deprecated *****
 * flags for (*batch)()
 * Availability: SENSORS_DEVICE_API_VERSION_1_0
 * see (*batch)() documentation for details.
 * Deprecated as of  SENSORS_DEVICE_API_VERSION_1_3.
 * WAKE_UP_* sensors replace WAKE_UPON_FIFO_FULL concept.
 */
enum {
    SENSORS_BATCH_DRY_RUN               = 0x00000001,
    SENSORS_BATCH_WAKE_UPON_FIFO_FULL   = 0x00000002
};

/*
 * what field for meta_data_event_t
 */
enum {
    /* a previous flush operation has completed */
    META_DATA_FLUSH_COMPLETE = 1,
    META_DATA_VERSION   /* always last, leave auto-assigned */
};

/*
 * The permission to use for body sensors (like heart rate monitors).
 * See sensor types for more details on what sensors should require this
 * permission.
 */
#define SENSOR_PERMISSION_BODY_SENSORS "android.permission.BODY_SENSORS"

/*
 * Availability: SENSORS_DEVICE_API_VERSION_1_4
 * Sensor HAL modes used in set_operation_mode method
 */
enum {
    /*
     * Operating modes for the HAL.
     */

    /*
     * Normal mode operation. This is the default state of operation.
     * The HAL shall initialize into this mode on device startup.
     */
    SENSOR_HAL_NORMAL_MODE        = 0,

    /*
     * Data Injection mode. In this mode, the device shall not source data from the
     * physical sensors as it would in normal mode. Instead sensor data is
     * injected by the sensor service.
     */
    SENSOR_HAL_DATA_INJECTION_MODE      = 0x1
};

#define SENSOR_FLAG_MASK(nbit, shift)   (((1<<(nbit))-1)<<(shift))
#define SENSOR_FLAG_MASK_1(shift)       SENSOR_FLAG_MASK(1, shift)

/*
 * Mask and shift for reporting mode sensor flags defined above.
 */
#define REPORTING_MODE_SHIFT            (1)
#define REPORTING_MODE_NBIT             (3)
#define REPORTING_MODE_MASK             SENSOR_FLAG_MASK(REPORTING_MODE_NBIT, REPORTING_MODE_SHIFT)
                                        // 0xE

/*
 * Mask and shift for data_injection mode sensor flags defined above.
 */
#define DATA_INJECTION_SHIFT            (4)
#define DATA_INJECTION_MASK             SENSOR_FLAG_MASK_1(DATA_INJECTION_SHIFT) //0x10

/*
 * Mask and shift for dynamic sensor flag.
 */
#define DYNAMIC_SENSOR_SHIFT            (5)
#define DYNAMIC_SENSOR_MASK             SENSOR_FLAG_MASK_1(DYNAMIC_SENSOR_SHIFT) //0x20

/*
 * Mask and shift for sensor additional information support.
 */
#define ADDITIONAL_INFO_SHIFT           (6)
#define ADDITIONAL_INFO_MASK            SENSOR_FLAG_MASK_1(ADDITIONAL_INFO_SHIFT) //0x40

/*
 * Availability: SENSORS_DEVICE_API_VERSION_1_3
 * Sensor flags used in sensor_t.flags.
 */
enum {
    /*
     * Whether this sensor wakes up the AP from suspend mode when data is available.  Whenever
     * sensor events are delivered from a wake_up sensor, the driver needs to hold a wake_lock till
     * the events are read by the SensorService i.e till sensors_poll_device_t.poll() is called the
     * next time. Once poll is called again it means events have been read by the SensorService, the
     * driver can safely release the wake_lock. SensorService will continue to hold a wake_lock till
     * the app actually reads the events.
     */
    SENSOR_FLAG_WAKE_UP = 1U << 0,
    /*
     * Reporting modes for various sensors. Each sensor will have exactly one of these modes set.
     * The least significant 2nd, 3rd and 4th bits are used to represent four possible reporting
     * modes.
     */
    SENSOR_FLAG_CONTINUOUS_MODE        = 0,    // 0000
    SENSOR_FLAG_ON_CHANGE_MODE         = 0x2,  // 0010
    SENSOR_FLAG_ONE_SHOT_MODE          = 0x4,  // 0100
    SENSOR_FLAG_SPECIAL_REPORTING_MODE = 0x6,  // 0110

    /*
     * Set this flag if the sensor supports data_injection mode and allows data to be injected
     * from the SensorService. When in data_injection ONLY sensors with this flag set are injected
     * sensor data and only sensors with this flag set are activated. Eg: Accelerometer and Step
     * Counter sensors can be set with this flag and SensorService will inject accelerometer data
     * and read the corresponding step counts.
     */
    SENSOR_FLAG_SUPPORTS_DATA_INJECTION = DATA_INJECTION_MASK, // 1 0000

    /*
     * Set this flag if the sensor is a dynamically connected sensor. See
     * dynamic_sensor_meta_event_t and SENSOR_TYPE_DYNAMIC_SENSOR_META for details.
     */
    SENSOR_FLAG_DYNAMIC_SENSOR = DYNAMIC_SENSOR_MASK,

    /*
     * Set this flag if sensor additional information is supported. See SENSOR_TYPE_ADDITIONAL_INFO
     * and additional_info_event_t for details.
     */
    SENSOR_FLAG_ADDITIONAL_INFO = ADDITIONAL_INFO_MASK
};


/*
 * Sensor type
 *
 * Each sensor has a type which defines what this sensor measures and how
 * measures are reported. See the Base sensors and Composite sensors lists
 * for complete descriptions:
 * http://source.android.com/devices/sensors/base_triggers.html
 * http://source.android.com/devices/sensors/composite_sensors.html
 *
 * Device manufacturers (OEMs) can define their own sensor types, for
 * their private use by applications or services provided by them. Such
 * sensor types are specific to an OEM and can't be exposed in the SDK.
 * These types must start at SENSOR_TYPE_DEVICE_PRIVATE_BASE.
 *
 * All sensors defined outside of the device private range must correspond to
 * a type defined in this file, and must satisfy the characteristics listed in
 * the description of the sensor type.
 *
 * Starting with version SENSORS_DEVICE_API_VERSION_1_2, each sensor also
 * has a stringType.
 *  - StringType of sensors inside of the device private range MUST be prefixed
 *    by the sensor provider's or OEM reverse domain name. In particular, they
 *    cannot use the "android.sensor" prefix.
 *  - StringType of sensors outside of the device private range MUST correspond
 *    to the one defined in this file (starting with "android.sensor").
 *    For example, accelerometers must have
 *      type=SENSOR_TYPE_ACCELEROMETER and
 *      stringType=SENSOR_STRING_TYPE_ACCELEROMETER
 *
 * When android introduces a new sensor type that can replace an OEM-defined
 * sensor type, the OEM must use the official sensor type and stringType on
 * versions of the HAL that support this new official sensor type.
 *
 * Example (made up): Suppose Google's Glass team wants to surface a sensor
 * detecting that Glass is on a head.
 *  - Such a sensor is not officially supported in android KitKat
 *  - Glass devices launching on KitKat can implement a sensor with
 *    type = 0x10001 and stringType = "com.google.glass.onheaddetector"
 *  - In L android release, if android decides to define
 *    SENSOR_TYPE_ON_HEAD_DETECTOR and STRING_SENSOR_TYPE_ON_HEAD_DETECTOR,
 *    those types should replace the Glass-team-specific types in all future
 *    launches.
 *  - When launching Glass on the L release, Google should now use the official
 *    type (SENSOR_TYPE_ON_HEAD_DETECTOR) and stringType.
 *  - This way, all applications can now use this sensor.
 */

/*
 * Base for device manufacturers private sensor types.
 * These sensor types can't be exposed in the SDK.
 */
#define SENSOR_TYPE_DEVICE_PRIVATE_BASE     0x10000

/*
 * SENSOR_TYPE_META_DATA
 * reporting-mode: n/a
 * wake-up sensor: n/a
 *
 * NO SENSOR OF THAT TYPE MUST BE RETURNED (*get_sensors_list)()
 *
 * SENSOR_TYPE_META_DATA is a special token used to populate the
 * sensors_meta_data_event structure. It doesn't correspond to a physical
 * sensor. sensors_meta_data_event are special, they exist only inside
 * the HAL and are generated spontaneously, as opposed to be related to
 * a physical sensor.
 *
 *   sensors_meta_data_event_t.version must be META_DATA_VERSION
 *   sensors_meta_data_event_t.sensor must be 0
 *   sensors_meta_data_event_t.type must be SENSOR_TYPE_META_DATA
 *   sensors_meta_data_event_t.reserved must be 0
 *   sensors_meta_data_event_t.timestamp must be 0
 *
 * The payload is a meta_data_event_t, where:
 * meta_data_event_t.what can take the following values:
 *
 * META_DATA_FLUSH_COMPLETE
 *   This event indicates that a previous (*flush)() call has completed for the sensor
 *   handle specified in meta_data_event_t.sensor.
 *   see (*flush)() for more details
 *
 * All other values for meta_data_event_t.what are reserved and
 * must not be used.
 *
 */
#define SENSOR_TYPE_META_DATA                        (0)

/*
  * Wake up sensors.
  * Each sensor may have either or both a wake-up and a non-wake variant.
  * When registered in batch mode, wake-up sensors will wake up the AP when
  * their FIFOs are full or when the batch timeout expires. A separate FIFO has
  * to be maintained for wake up sensors and non wake up sensors. The non wake-up
  * sensors need to overwrite their FIFOs when they are full till the AP wakes up
  * and the wake-up sensors will wake-up the AP when their FIFOs are full or when
  * the batch timeout expires without losing events. Wake-up and non wake-up variants
  * of each sensor can be activated at different rates independently of each other.
  *
  * Note: Proximity sensor and significant motion sensor which were defined in previous
  * releases are also wake-up sensors and should be treated as such. Wake-up one-shot
  * sensors like SIGNIFICANT_MOTION cannot be batched, hence the text about batch above
  * doesn't apply to them. See the definitions of SENSOR_TYPE_PROXIMITY and
  * SENSOR_TYPE_SIGNIFICANT_MOTION for more info.
  *
  * Set SENSOR_FLAG_WAKE_UP flag for all wake-up sensors.
  *
  * For example, A device can have two sensors both of SENSOR_TYPE_ACCELEROMETER and
  * one of them can be a wake_up sensor (with SENSOR_FLAG_WAKE_UP flag set) and the other
  * can be a regular non wake_up sensor. Both of these sensors must be activated/deactivated
  * independently of the other.
  */

/*
 * SENSOR_TYPE_ACCELEROMETER
 * reporting-mode: continuous
 *
 *  All values are in SI units (m/s^2) and measure the acceleration of the
 *  device minus the force of gravity.
 *
 *  Implement the non-wake-up version of this sensor and implement the wake-up
 *  version if the system possesses a wake up fifo.
 */
#define SENSOR_TYPE_ACCELEROMETER                    (1)
#define SENSOR_STRING_TYPE_ACCELEROMETER             "android.sensor.accelerometer"

/*
 * SENSOR_TYPE_GEOMAGNETIC_FIELD
 * reporting-mode: continuous
 *
 *  All values are in micro-Tesla (uT) and measure the geomagnetic
 *  field in the X, Y and Z axis.
 *
 *  Implement the non-wake-up version of this sensor and implement the wake-up
 *  version if the system possesses a wake up fifo.
 */
#define SENSOR_TYPE_GEOMAGNETIC_FIELD                (2)
#define SENSOR_TYPE_MAGNETIC_FIELD  SENSOR_TYPE_GEOMAGNETIC_FIELD
#define SENSOR_STRING_TYPE_MAGNETIC_FIELD            "android.sensor.magnetic_field"

/*
 * SENSOR_TYPE_ORIENTATION
 * reporting-mode: continuous
 *
 * All values are angles in degrees.
 *
 * Orientation sensors return sensor events for all 3 axes at a constant
 * rate defined by setDelay().
 *
 * Implement the non-wake-up version of this sensor and implement the wake-up
 * version if the system possesses a wake up fifo.
 */
#define SENSOR_TYPE_ORIENTATION                      (3)
#define SENSOR_STRING_TYPE_ORIENTATION               "android.sensor.orientation"

/*
 * SENSOR_TYPE_GYROSCOPE
 * reporting-mode: continuous
 *
 *  All values are in radians/second and measure the rate of rotation
 *  around the X, Y and Z axis.
 *
 *  Implement the non-wake-up version of this sensor and implement the wake-up
 *  version if the system possesses a wake up fifo.
 */
#define SENSOR_TYPE_GYROSCOPE                        (4)
#define SENSOR_STRING_TYPE_GYROSCOPE                 "android.sensor.gyroscope"

/*
 * SENSOR_TYPE_LIGHT
 * reporting-mode: on-change
 *
 * The light sensor value is returned in SI lux units.
 *
 * Both wake-up and non wake-up versions are useful.
 */
#define SENSOR_TYPE_LIGHT                            (5)
#define SENSOR_STRING_TYPE_LIGHT                     "android.sensor.light"

/*
 * SENSOR_TYPE_PRESSURE
 * reporting-mode: continuous
 *
 * The pressure sensor return the athmospheric pressure in hectopascal (hPa)
 *
 * Implement the non-wake-up version of this sensor and implement the wake-up
 * version if the system possesses a wake up fifo.
 */
#define SENSOR_TYPE_PRESSURE                         (6)
#define SENSOR_STRING_TYPE_PRESSURE                  "android.sensor.pressure"

/* SENSOR_TYPE_TEMPERATURE is deprecated in the HAL */
#define SENSOR_TYPE_TEMPERATURE                      (7)
#define SENSOR_STRING_TYPE_TEMPERATURE               "android.sensor.temperature"

/*
 * SENSOR_TYPE_PROXIMITY
 * reporting-mode: on-change
 *
 * The proximity sensor which turns the screen off and back on during calls is the
 * wake-up proximity sensor. Implement wake-up proximity sensor before implementing
 * a non wake-up proximity sensor. For the wake-up proximity sensor set the flag
 * SENSOR_FLAG_WAKE_UP.
 * The value corresponds to the distance to the nearest object in centimeters.
 */
#define SENSOR_TYPE_PROXIMITY                        (8)
#define SENSOR_STRING_TYPE_PROXIMITY                 "android.sensor.proximity"

/*
 * SENSOR_TYPE_GRAVITY
 * reporting-mode: continuous
 *
 * A gravity output indicates the direction of and magnitude of gravity in
 * the devices's coordinates.
 *
 * Implement the non-wake-up version of this sensor and implement the wake-up
 * version if the system possesses a wake up fifo.
 */
#define SENSOR_TYPE_GRAVITY                          (9)
#define SENSOR_STRING_TYPE_GRAVITY                   "android.sensor.gravity"

/*
 * SENSOR_TYPE_LINEAR_ACCELERATION
 * reporting-mode: continuous
 *
 * Indicates the linear acceleration of the device in device coordinates,
 * not including gravity.
 *
 * Implement the non-wake-up version of this sensor and implement the wake-up
 * version if the system possesses a wake up fifo.
 */
#define SENSOR_TYPE_LINEAR_ACCELERATION             (10)
#define SENSOR_STRING_TYPE_LINEAR_ACCELERATION      "android.sensor.linear_acceleration"


/*
 * SENSOR_TYPE_ROTATION_VECTOR
 * reporting-mode: continuous
 *
 * The rotation vector symbolizes the orientation of the device relative to the
 * East-North-Up coordinates frame.
 *
 * Implement the non-wake-up version of this sensor and implement the wake-up
 * version if the system possesses a wake up fifo.
 */
#define SENSOR_TYPE_ROTATION_VECTOR                 (11)
#define SENSOR_STRING_TYPE_ROTATION_VECTOR          "android.sensor.rotation_vector"

/*
 * SENSOR_TYPE_RELATIVE_HUMIDITY
 * reporting-mode: on-change
 *
 * A relative humidity sensor measures relative ambient air humidity and
 * returns a value in percent.
 *
 * Both wake-up and non wake-up versions are useful.
 */
#define SENSOR_TYPE_RELATIVE_HUMIDITY               (12)
#define SENSOR_STRING_TYPE_RELATIVE_HUMIDITY        "android.sensor.relative_humidity"

/*
 * SENSOR_TYPE_AMBIENT_TEMPERATURE
 * reporting-mode: on-change
 *
 * The ambient (room) temperature in degree Celsius.
 *
 * Both wake-up and non wake-up versions are useful.
 */
#define SENSOR_TYPE_AMBIENT_TEMPERATURE             (13)
#define SENSOR_STRING_TYPE_AMBIENT_TEMPERATURE      "android.sensor.ambient_temperature"

/*
 * SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED
 * reporting-mode: continuous
 *
 *  Similar to SENSOR_TYPE_MAGNETIC_FIELD, but the hard iron calibration is
 *  reported separately instead of being included in the measurement.
 *
 *  Implement the non-wake-up version of this sensor and implement the wake-up
 *  version if the system possesses a wake up fifo.
 */
#define SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED     (14)
#define SENSOR_STRING_TYPE_MAGNETIC_FIELD_UNCALIBRATED "android.sensor.magnetic_field_uncalibrated"

/*
 * SENSOR_TYPE_GAME_ROTATION_VECTOR
 * reporting-mode: continuous
 *
 *  Similar to SENSOR_TYPE_ROTATION_VECTOR, but not using the geomagnetic
 *  field.
 *
 *  Implement the non-wake-up version of this sensor and implement the wake-up
 *  version if the system possesses a wake up fifo.
 */
#define SENSOR_TYPE_GAME_ROTATION_VECTOR            (15)
#define SENSOR_STRING_TYPE_GAME_ROTATION_VECTOR     "android.sensor.game_rotation_vector"

/*
 * SENSOR_TYPE_GYROSCOPE_UNCALIBRATED
 * reporting-mode: continuous
 *
 *  All values are in radians/second and measure the rate of rotation
 *  around the X, Y and Z axis.
 *
 *  Implement the non-wake-up version of this sensor and implement the wake-up
 *  version if the system possesses a wake up fifo.
 */
#define SENSOR_TYPE_GYROSCOPE_UNCALIBRATED          (16)
#define SENSOR_STRING_TYPE_GYROSCOPE_UNCALIBRATED   "android.sensor.gyroscope_uncalibrated"

/*
 * SENSOR_TYPE_SIGNIFICANT_MOTION
 * reporting-mode: one-shot
 *
 * A sensor of this type triggers an event each time significant motion
 * is detected and automatically disables itself.
 * For Significant Motion sensor to be useful, it must be defined as a
 * wake-up sensor. (set SENSOR_FLAG_WAKE_UP). Implement the wake-up significant motion
 * sensor. A non wake-up version is not useful.
 * The only allowed value to return is 1.0.
 */

#define SENSOR_TYPE_SIGNIFICANT_MOTION              (17)
#define SENSOR_STRING_TYPE_SIGNIFICANT_MOTION       "android.sensor.significant_motion"

/*
 * SENSOR_TYPE_STEP_DETECTOR
 * reporting-mode: special
 *
 * A sensor of this type triggers an event each time a step is taken
 * by the user. The only allowed value to return is 1.0 and an event
 * is generated for each step.
 *
 * Both wake-up and non wake-up versions are useful.
 */

#define SENSOR_TYPE_STEP_DETECTOR                   (18)
#define SENSOR_STRING_TYPE_STEP_DETECTOR            "android.sensor.step_detector"


/*
 * SENSOR_TYPE_STEP_COUNTER
 * reporting-mode: on-change
 *
 * A sensor of this type returns the number of steps taken by the user since
 * the last reboot while activated. The value is returned as a uint64_t and is
 * reset to zero only on a system / android reboot.
 *
 * Implement the non-wake-up version of this sensor and implement the wake-up
 * version if the system possesses a wake up fifo.
 */

#define SENSOR_TYPE_STEP_COUNTER                    (19)
#define SENSOR_STRING_TYPE_STEP_COUNTER             "android.sensor.step_counter"

/*
 * SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR
 * reporting-mode: continuous
 *
 *  Similar to SENSOR_TYPE_ROTATION_VECTOR, but using a magnetometer instead
 *  of using a gyroscope.
 *
 * Implement the non-wake-up version of this sensor and implement the wake-up
 * version if the system possesses a wake up fifo.
 */
#define SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR     (20)
#define SENSOR_STRING_TYPE_GEOMAGNETIC_ROTATION_VECTOR "android.sensor.geomagnetic_rotation_vector"

/*
 * SENSOR_TYPE_HEART_RATE
 * reporting-mode: on-change
 *
 *  A sensor of this type returns the current heart rate.
 *  The events contain the current heart rate in beats per minute (BPM) and the
 *  status of the sensor during the measurement. See heart_rate_event_t for more
 *  details.
 *
 *  Because this sensor is on-change, events must be generated when and only
 *  when heart_rate.bpm or heart_rate.status have changed since the last
 *  event. In particular, upon the first activation, unless the device is known
 *  to not be on the body, the status field of the first event must be set to
 *  SENSOR_STATUS_UNRELIABLE. The event should be generated no faster than every
 *  period_ns passed to setDelay() or to batch().
 *  See the definition of the on-change reporting mode for more information.
 *
 *  sensor_t.requiredPermission must be set to SENSOR_PERMISSION_BODY_SENSORS.
 *
 *  Both wake-up and non wake-up versions are useful.
 */
#define SENSOR_TYPE_HEART_RATE                      (21)
#define SENSOR_STRING_TYPE_HEART_RATE               "android.sensor.heart_rate"

/*
 * SENSOR_TYPE_WAKE_UP_TILT_DETECTOR
 * reporting-mode: special (setDelay has no impact)
 *
 * A sensor of this type generates an event each time a tilt event is detected. A tilt event
 * should be generated if the direction of the 2-seconds window average gravity changed by at least
 * 35 degrees since the activation or the last trigger of the sensor.
 *     reference_estimated_gravity = average of accelerometer measurements over the first
 *                                 1 second after activation or the estimated gravity at the last
 *                                 trigger.
 *     current_estimated_gravity = average of accelerometer measurements over the last 2 seconds.
 *     trigger when angle (reference_estimated_gravity, current_estimated_gravity) > 35 degrees
 *
 * Large accelerations without a change in phone orientation should not trigger a tilt event.
 * For example, a sharp turn or strong acceleration while driving a car should not trigger a tilt
 * event, even though the angle of the average acceleration might vary by more than 35 degrees.
 *
 * Typically, this sensor is implemented with the help of only an accelerometer. Other sensors can
 * be used as well if they do not increase the power consumption significantly. This is a low power
 * sensor that should allow the AP to go into suspend mode. Do not emulate this sensor in the HAL.
 * Like other wake up sensors, the driver is expected to a hold a wake_lock with a timeout of 200 ms
 * while reporting this event. The only allowed return value is 1.0.
 *
 * Implement only the wake-up version of this sensor.
 */
#define SENSOR_TYPE_TILT_DETECTOR                      (22)
#define SENSOR_STRING_TYPE_TILT_DETECTOR               "android.sensor.tilt_detector"

/*
 * SENSOR_TYPE_WAKE_GESTURE
 * reporting-mode: one-shot
 *
 * A sensor enabling waking up the device based on a device specific motion.
 *
 * When this sensor triggers, the device behaves as if the power button was
 * pressed, turning the screen on. This behavior (turning on the screen when
 * this sensor triggers) might be deactivated by the user in the device
 * settings. Changes in settings do not impact the behavior of the sensor:
 * only whether the framework turns the screen on when it triggers.
 *
 * The actual gesture to be detected is not specified, and can be chosen by
 * the manufacturer of the device.
 * This sensor must be low power, as it is likely to be activated 24/7.
 * The only allowed value to return is 1.0.
 *
 * Implement only the wake-up version of this sensor.
 */
#define SENSOR_TYPE_WAKE_GESTURE                               (23)
#define SENSOR_STRING_TYPE_WAKE_GESTURE                        "android.sensor.wake_gesture"

/*
 * SENSOR_TYPE_GLANCE_GESTURE
 * reporting-mode: one-shot
 *
 * A sensor enabling briefly turning the screen on to enable the user to
 * glance content on screen based on a specific motion.  The device should
 * turn the screen off after a few moments.
 *
 * When this sensor triggers, the device turns the screen on momentarily
 * to allow the user to glance notifications or other content while the
 * device remains locked in a non-interactive state (dozing). This behavior
 * (briefly turning on the screen when this sensor triggers) might be deactivated
 * by the user in the device settings. Changes in settings do not impact the
 * behavior of the sensor: only whether the framework briefly turns the screen on
 * when it triggers.
 *
 * The actual gesture to be detected is not specified, and can be chosen by
 * the manufacturer of the device.
 * This sensor must be low power, as it is likely to be activated 24/7.
 * The only allowed value to return is 1.0.
 *
 * Implement only the wake-up version of this sensor.
 */
#define SENSOR_TYPE_GLANCE_GESTURE                             (24)
#define SENSOR_STRING_TYPE_GLANCE_GESTURE                      "android.sensor.glance_gesture"

/**
 * SENSOR_TYPE_PICK_UP_GESTURE
 * reporting-mode: one-shot
 *
 * A sensor of this type triggers when the device is picked up regardless of wherever is was
 * before (desk, pocket, bag). The only allowed return value is 1.0.
 * This sensor de-activates itself immediately after it triggers.
 *
 * Implement only the wake-up version of this sensor.
 */
#define SENSOR_TYPE_PICK_UP_GESTURE                            (25)
#define SENSOR_STRING_TYPE_PICK_UP_GESTURE                     "android.sensor.pick_up_gesture"

/*
 * SENSOR_TYPE_WRIST_TILT_GESTURE
 * trigger-mode: special
 * wake-up sensor: yes
 *
 * A sensor of this type triggers an event each time a tilt of the wrist-worn
 * device is detected.
 *
 * This sensor must be low power, as it is likely to be activated 24/7.
 * The only allowed value to return is 1.0.
 *
 * Implement only the wake-up version of this sensor.
 */
#define SENSOR_TYPE_WRIST_TILT_GESTURE                         (26)
#define SENSOR_STRING_TYPE_WRIST_TILT_GESTURE                  "android.sensor.wrist_tilt_gesture"

/*
 * SENSOR_TYPE_DEVICE_ORIENTATION
 * reporting-mode: on-change
 *
 * The current orientation of the device. The value should be reported in the
 * first element of the 'data' member variable in sensors_event_t. The only
 * values that can be reported are (please refer to Android Sensor Coordinate
 * System to understand the X and Y axis direction with respect to default
 * orientation):
 *  - 0: device is in default orientation (Y axis is vertical and points up)
 *  - 1: device is rotated 90 degrees counter-clockwise from default
 *       orientation (X axis is vertical and points up)
 *  - 2: device is rotated 180 degrees from default orientation (Y axis is
 *       vertical and points down)
 *  - 3: device is rotated 90 degrees clockwise from default orientation (X axis
 *       is vertical and points down)
 *
 * Moving the device to an orientation where the Z axis is vertical (either up
 * or down) should not cause a new event to be reported.
 *
 * To improve the user experience of this sensor, it is recommended to implement
 * some physical (i.e., rotation angle) and temporal (i.e., delay) hysteresis.
 * In other words, minor or transient rotations should not cause a new event to
 * be reported.
 *
 * This sensor should only be implemented with the help of an accelerometer.
 * This is a low power sensor that should reduce the number of interrupts of the
 * AP. Do not emulate this sensor in the HAL.
 *
 * Both wake-up and non wake-up versions are useful.
 */
#define SENSOR_TYPE_DEVICE_ORIENTATION                 (27)
#define SENSOR_STRING_TYPE_DEVICE_ORIENTATION          "android.sensor.device_orientation"

/*
 * SENSOR_TYPE_POSE_6DOF
 * trigger-mode: continuous
 *
 * A sensor of this type returns the pose of the device.
 * Pose of the device is defined as the orientation of the device from a
 * Earth Centered Earth Fixed frame and the translation from an arbitrary
 * point at subscription.
 *
 * This sensor can be high power. It can use any and all of the following
 *           . Accelerometer
 *           . Gyroscope
 *           . Camera
 *           . Depth Camera
 *
 */
#define SENSOR_TYPE_POSE_6DOF                         (28)
#define SENSOR_STRING_TYPE_POSE_6DOF                  "android.sensor.pose_6dof"

/*
 * SENSOR_TYPE_STATIONARY_DETECT
 * trigger mode: one shot
 *
 * A sensor of this type returns an event if the device is still/stationary for
 * a while. The period of time to monitor for statinarity should be greater than
 * 5 seconds, and less than 10 seconds.
 *
 * Stationarity here refers to absolute stationarity. eg: device on desk.
 *
 * The only allowed value to return is 1.0.
 */
#define SENSOR_TYPE_STATIONARY_DETECT                   (29)
#define SENSOR_STRING_TYPE_STATIONARY_DETECT            "android.sensor.stationary_detect"

/*
 * SENSOR_TYPE_MOTION_DETECT
 * trigger mode: one shot
 *
 * A sensor of this type returns an event if the device is not still for
 * a while. The period of time to monitor for statinarity should be greater than
 * 5 seconds, and less than 10 seconds.
 *
 * Motion here refers to any mechanism in which the device is causes to be
 * moved in its inertial frame. eg: Pickin up the device and walking with it
 * to a nearby room may trigger motion wherewas keeping the device on a table
 * on a smooth train moving at constant velocity may not trigger motion.
 *
 * The only allowed value to return is 1.0.
 */
#define SENSOR_TYPE_MOTION_DETECT                       (30)
#define SENSOR_STRING_TYPE_MOTION_DETECT                "android.sensor.motion_detect"

/*
 * SENSOR_TYPE_HEART_BEAT
 * trigger mode: continuous
 *
 * A sensor of this type returns an event everytime a hear beat peak is
 * detected.
 *
 * Peak here ideally corresponds to the positive peak in the QRS complex of
 * and ECG signal.
 *
 * The sensor is not expected to be optimized for latency. As a guide, a
 * latency of up to 10 seconds is acceptable. However the timestamp attached
 * to the event should be accurate and should correspond to the time the peak
 * occured.
 *
 * The sensor event contains a parameter for the confidence in the detection
 * of the peak where 0.0 represent no information at all, and 1.0 represents
 * certainty.
 */
#define SENSOR_TYPE_HEART_BEAT                          (31)
#define SENSOR_STRING_TYPE_HEART_BEAT                   "android.sensor.heart_beat"

/**
 * SENSOR_TYPE_DYNAMIC_SENSOR_META
 * trigger-mode: special
 *
 * A sensor event of this type is received when a dynamic sensor is added to or removed from the
 * system. At most one sensor of this type can be present in one sensor HAL implementation and
 * presence of a sensor of this type in sensor HAL implementation indicates that this sensor HAL
 * supports dynamic sensor feature. Operations, such as batch, activate and setDelay, to this
 * special purpose sensor should be treated as no-op and return successful.
 *
 * A dynamic sensor connection indicates connection of a physical device or instantiation of a
 * virtual sensor backed by algorithm; and a dynamic sensor disconnection indicates the the
 * opposite. A sensor event of SENSOR_TYPE_DYNAMIC_SENSOR_META type should be delivered regardless
 * of the activation status of the sensor in the event of dynamic sensor connection and
 * disconnection. In the sensor event, besides the common data entries, "dynamic_sensor_meta", which
 * includes fields for connection status, handle of the sensor involved, pointer to sensor_t
 * structure and a uuid field, should be populated.
 *
 * At a dynamic sensor connection event, fields of sensor_t structure referenced by a pointer in
 * dynamic_sensor_meta should be filled as if it was regular sensors. Sensor HAL is responsible for
 * recovery of memory if the corresponding data is dynamicially allocated. However, the the pointer
 * must be valid until the first activate call to the sensor reported in this connection event. At a
 * dynamic sensor disconnection, the sensor_t pointer should be NULL.
 *
 * The sensor handle assigned to dynamic sensors should never be the same as that of any regular
 * static sensors, and should be unique until next boot. In another word, if a handle h is used for
 * a dynamic sensor A, that same number cannot be used for the same dynamic sensor A or another
 * dynamic sensor B even after disconnection of A until reboot.
 *
 * The UUID field will be used for identifying the sensor in addition to name, vendor and version
 * and type. For physical sensors of the same model, all sensors will have the same values in
 * sensor_t, but the UUID should be unique and persistent for each individual unit. An all zero UUID
 * indicates it is not possible to differentiate individual sensor unit.
 *
 */
#define SENSOR_TYPE_DYNAMIC_SENSOR_META                         (32)
#define SENSOR_STRING_TYPE_DYNAMIC_SENSOR_META                  "android.sensor.dynamic_sensor_meta"

/**
 * SENSOR_TYPE_ADDITIONAL_INFO
 * reporting-mode: N/A
 *
 * This sensor type is for delivering additional sensor information aside from sensor event data.
 * Additional information may include sensor front-end group delay, internal calibration parameters,
 * noise level metrics, device internal temperature, etc.
 *
 * This type will never bind to a sensor. In other words, no sensor in the sensor list should be of
 * the type SENSOR_TYPE_ADDITIONAL_INFO. If a sensor HAL supports sensor additional information
 * feature, it reports sensor_event_t with "sensor" field set to handle of the reporting sensor and
 * "type" field set to SENSOR_TYPE_ADDITIONAL_INFO. Delivery of additional information events is
 * triggered under two conditions: an enable activate() call or a flush() call to the corresponding
 * sensor.
 *
 * A single additional information report consists of multiple frames. Sequences of these frames are
 * ordered using timestamps, which means the timestamps of sequential frames have to be at least 1
 * nanosecond apart from each other. Each frame is a sensor_event_t delivered through the HAL
 * interface, with related data stored in the "additional_info" field, which is of type
 * additional_info_event_t.  The "type" field of additional_info_event_t denotes the nature of the
 * payload data (see additional_info_type_t).  The "serial" field is used to keep the sequence of
 * payload data that spans multiple frames. The first frame of the entire report is always of type
 * AINFO_BEGIN, and the last frame is always AINFO_END.
 *
 * All additional information frames have to be delivered after flush complete event if flush() was
 * triggering the report.
 */
#define SENSOR_TYPE_ADDITIONAL_INFO                       (33)
#define SENSOR_STRING_TYPE_ADDITIONAL_INFO                "android.sensor.additional_info"

/**
 * Values returned by the accelerometer in various locations in the universe.
 * all values are in SI units (m/s^2)
 */
#define GRAVITY_SUN             (275.0f)
#define GRAVITY_EARTH           (9.80665f)

/** Maximum magnetic field on Earth's surface */
#define MAGNETIC_FIELD_EARTH_MAX    (60.0f)

/** Minimum magnetic field on Earth's surface */
#define MAGNETIC_FIELD_EARTH_MIN    (30.0f)

/**
 * Possible values of the status field of sensor events.
 */
#define SENSOR_STATUS_NO_CONTACT        -1
#define SENSOR_STATUS_UNRELIABLE        0
#define SENSOR_STATUS_ACCURACY_LOW      1
#define SENSOR_STATUS_ACCURACY_MEDIUM   2
#define SENSOR_STATUS_ACCURACY_HIGH     3


struct sensor_t;

/**
 * sensor event data
 */
typedef struct {
    union {
        float v[3];
        struct {
            float x;
            float y;
            float z;
        };
        struct {
            float azimuth;
            float pitch;
            float roll;
        };
    };
    int8_t status;
    uint8_t reserved[3];
} sensors_vec_t;

/**
 * uncalibrated gyroscope and magnetometer event data
 */
typedef struct {
  union {
    float uncalib[3];
    struct {
      float x_uncalib;
      float y_uncalib;
      float z_uncalib;
    };
  };
  union {
    float bias[3];
    struct {
      float x_bias;
      float y_bias;
      float z_bias;
    };
  };
} uncalibrated_event_t;

/**
 * Meta data event data
 */
typedef struct meta_data_event {
    int32_t what;
    int32_t sensor;
} meta_data_event_t;

/**
 * Dynamic sensor meta event. See the description of SENSOR_TYPE_DYNAMIC_SENSOR_META type for
 * details.
 */
typedef struct dynamic_sensor_meta_event {
    int32_t  connected;
    int32_t  handle;
    const struct sensor_t * sensor; // should be NULL if connected == false
    uint8_t uuid[16];               // UUID of a dynamic sensor (using RFC 4122 byte order)
                                    // For UUID 12345678-90AB-CDEF-1122-334455667788 the uuid field
                                    // should be initialized as:
                                    // {0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF, 0x11, ...}
} dynamic_sensor_meta_event_t;

/**
 * Heart rate event data
 */
typedef struct {
  // Heart rate in beats per minute.
  // Set to 0 when status is SENSOR_STATUS_UNRELIABLE or ..._NO_CONTACT
  float bpm;
  // Status of the sensor for this reading. Set to one SENSOR_STATUS_...
  // Note that this value should only be set for sensors that explicitly define
  // the meaning of this field. This field is not piped through the framework
  // for other sensors.
  int8_t status;
} heart_rate_event_t;

typedef struct {
    int32_t type;                           // type of payload data, see additional_info_type_t
    int32_t serial;                         // sequence number of this frame for this type
    union {
        // for each frame, a single data type, either int32_t or float, should be used.
        int32_t data_int32[14];
        float   data_float[14];
    };
} additional_info_event_t;

typedef enum additional_info_type {
    //
    AINFO_BEGIN = 0x0,                      // Marks the beginning of additional information frames
    AINFO_END   = 0x1,                      // Marks the end of additional information frames
    // Basic information
    AINFO_UNTRACKED_DELAY =  0x10000,       // Estimation of the delay that is not tracked by sensor
                                            // timestamps. This includes delay introduced by
                                            // sensor front-end filtering, data transport, etc.
                                            // float[2]: delay in seconds
                                            //           standard deviation of estimated value
                                            //
    AINFO_INTERNAL_TEMPERATURE,             // float: Celsius temperature.
                                            //
    AINFO_VEC3_CALIBRATION,                 // First three rows of a homogeneous matrix, which
                                            // represents calibration to a three-element vector
                                            // raw sensor reading.
                                            // float[12]: 3x4 matrix in row major order
                                            //
    AINFO_SENSOR_PLACEMENT,                 // Location and orientation of sensor element in the
                                            // device frame: origin is the geometric center of the
                                            // mobile device screen surface; the axis definition
                                            // corresponds to Android sensor definitions.
                                            // float[12]: 3x4 matrix in row major order
                                            //
    AINFO_SAMPLING,                         // float[2]: raw sample period in seconds,
                                            //           standard deviation of sampling period

    // Sampling channel modeling information
    AINFO_CHANNEL_NOISE = 0x20000,          // int32_t: noise type
                                            // float[n]: parameters
                                            //
    AINFO_CHANNEL_SAMPLER,                  // float[3]: sample period
                                            //           standard deviation of sample period,
                                            //           quantization unit
                                            //
    AINFO_CHANNEL_FILTER,                   // Represents a filter:
                                            //      \sum_j a_j y[n-j] == \sum_i b_i x[n-i]
                                            //
                                            // int32_t[3]: number of feedforward coefficients, M,
                                            //             number of feedback coefficients, N, for
                                            //               FIR filter, N=1.
                                            //             bit mask that represents which element to
                                            //               which the filter is applied, bit 0 == 1
                                            //               means this filter applies to vector
                                            //               element 0.
                                            // float[M+N]: filter coefficients (b0, b1, ..., BM-1),
                                            //             then (a0, a1, ..., aN-1), a0 is always 1.
                                            //             Multiple frames may be needed for higher
                                            //             number of taps.
                                            //
    AINFO_CHANNEL_LINEAR_TRANSFORM,         // int32_t[2]: size in (row, column) ... 1st frame
                                            // float[n]: matrix element values in row major order.
                                            //
    AINFO_CHANNEL_NONLINEAR_MAP,            // int32_t[2]: extrapolate method
                                            //             interpolate method
                                            // float[n]: mapping key points in pairs, (in, out)...
                                            //           (may be used to model saturation)
                                            //
    AINFO_CHANNEL_RESAMPLER,                // int32_t:  resample method (0-th order, 1st order...)
                                            // float[1]: resample ratio (upsampling if < 1.0;
                                            //           downsampling if > 1.0).
                                            //

    // Custom information
    AINFO_CUSTOM_START =    0x10000000,     //
    // Debugging
    AINFO_DEBUGGING_START = 0x40000000,     //
} additional_info_type_t;

/**
 * Union of the various types of sensor data
 * that can be returned.
 */
typedef struct sensors_event_t {
    /* must be sizeof(struct sensors_event_t) */
    int32_t version;

    /* sensor identifier */
    int32_t sensor;

    /* sensor type */
    int32_t type;

    /* reserved */
    int32_t reserved0;

    /* time is in nanosecond */
    int64_t timestamp;

    union {
        union {
            float           data[16];

            /* acceleration values are in meter per second per second (m/s^2) */
            sensors_vec_t   acceleration;

            /* magnetic vector values are in micro-Tesla (uT) */
            sensors_vec_t   magnetic;

            /* orientation values are in degrees */
            sensors_vec_t   orientation;

            /* gyroscope values are in rad/s */
            sensors_vec_t   gyro;

            /* temperature is in degrees centigrade (Celsius) */
            float           temperature;

            /* distance in centimeters */
            float           distance;

            /* light in SI lux units */
            float           light;

            /* pressure in hectopascal (hPa) */
            float           pressure;

            /* relative humidity in percent */
            float           relative_humidity;

            /* uncalibrated gyroscope values are in rad/s */
            uncalibrated_event_t uncalibrated_gyro;

            /* uncalibrated magnetometer values are in micro-Teslas */
            uncalibrated_event_t uncalibrated_magnetic;

            /* heart rate data containing value in bpm and status */
            heart_rate_event_t heart_rate;

            /* this is a special event. see SENSOR_TYPE_META_DATA above.
             * sensors_meta_data_event_t events are all reported with a type of
             * SENSOR_TYPE_META_DATA. The handle is ignored and must be zero.
             */
            meta_data_event_t meta_data;

            /* dynamic sensor meta event. See SENSOR_TYPE_DYNAMIC_SENSOR_META type for details */
            dynamic_sensor_meta_event_t dynamic_sensor_meta;

            /*
             * special additional sensor information frame, see
             * SENSOR_TYPE_ADDITIONAL_INFO for details.
             */
            additional_info_event_t additional_info;
        };

        union {
            uint64_t        data[8];

            /* step-counter */
            uint64_t        step_counter;
        } u64;
    };

    /* Reserved flags for internal use. Set to zero. */
    uint32_t flags;

    uint32_t reserved1[3];
} sensors_event_t;


/* see SENSOR_TYPE_META_DATA */
typedef sensors_event_t sensors_meta_data_event_t;


/**
 * Every hardware module must have a data structure named HAL_MODULE_INFO_SYM
 * and the fields of this data structure must begin with hw_module_t
 * followed by module specific information.
 */
struct sensors_module_t {
    struct hw_module_t common;

    /**
     * Enumerate all available sensors. The list is returned in "list".
     * @return number of sensors in the list
     */
    int (*get_sensors_list)(struct sensors_module_t* module,
            struct sensor_t const** list);

    /**
     *  Place the module in a specific mode. The following modes are defined
     *
     *  0 - Normal operation. Default state of the module.
     *  1 - Loopback mode. Data is injected for the supported
     *      sensors by the sensor service in this mode.
     * @return 0 on success
     *         -EINVAL if requested mode is not supported
     *         -EPERM if operation is not allowed
     */
    int (*set_operation_mode)(unsigned int mode);
};

struct sensor_t {

    /* Name of this sensor.
     * All sensors of the same "type" must have a different "name".
     */
    const char*     name;

    /* vendor of the hardware part */
    const char*     vendor;

    /* version of the hardware part + driver. The value of this field
     * must increase when the driver is updated in a way that changes the
     * output of this sensor. This is important for fused sensors when the
     * fusion algorithm is updated.
     */
    int             version;

    /* handle that identifies this sensors. This handle is used to reference
     * this sensor throughout the HAL API.
     */
    int             handle;

    /* this sensor's type. */
    int             type;

    /* maximum range of this sensor's value in SI units */
    float           maxRange;

    /* smallest difference between two values reported by this sensor */
    float           resolution;

    /* rough estimate of this sensor's power consumption in mA */
    float           power;

    /* this value depends on the reporting mode:
     *
     *   continuous: minimum sample period allowed in microseconds
     *   on-change : 0
     *   one-shot  :-1
     *   special   : 0, unless otherwise noted
     */
    int32_t         minDelay;

    /* number of events reserved for this sensor in the batch mode FIFO.
     * If there is a dedicated FIFO for this sensor, then this is the
     * size of this FIFO. If the FIFO is shared with other sensors,
     * this is the size reserved for that sensor and it can be zero.
     */
    uint32_t        fifoReservedEventCount;

    /* maximum number of events of this sensor that could be batched.
     * This is especially relevant when the FIFO is shared between
     * several sensors; this value is then set to the size of that FIFO.
     */
    uint32_t        fifoMaxEventCount;

    /* type of this sensor as a string. Set to corresponding
     * SENSOR_STRING_TYPE_*.
     * When defining an OEM specific sensor or sensor manufacturer specific
     * sensor, use your reserve domain name as a prefix.
     * ex: com.google.glass.onheaddetector
     * For sensors of known type, the android framework might overwrite this
     * string automatically.
     */
    const char*    stringType;

    /* permission required to see this sensor, register to it and receive data.
     * Set to "" if no permission is required. Some sensor types like the
     * heart rate monitor have a mandatory require_permission.
     * For sensors that always require a specific permission, like the heart
     * rate monitor, the android framework might overwrite this string
     * automatically.
     */
    const char*    requiredPermission;

    /* This value is defined only for continuous mode and on-change sensors. It is the delay between
     * two sensor events corresponding to the lowest frequency that this sensor supports. When lower
     * frequencies are requested through batch()/setDelay() the events will be generated at this
     * frequency instead. It can be used by the framework or applications to estimate when the batch
     * FIFO may be full.
     *
     * NOTE: 1) period_ns is in nanoseconds where as maxDelay/minDelay are in microseconds.
     *              continuous, on-change: maximum sampling period allowed in microseconds.
     *              one-shot, special : 0
     *   2) maxDelay should always fit within a 32 bit signed integer. It is declared as 64 bit
     *      on 64 bit architectures only for binary compatibility reasons.
     * Availability: SENSORS_DEVICE_API_VERSION_1_3
     */
    #ifdef __LP64__
       int64_t maxDelay;
    #else
       int32_t maxDelay;
    #endif

    /* Flags for sensor. See SENSOR_FLAG_* above. Only the least significant 32 bits are used here.
     * It is declared as 64 bit on 64 bit architectures only for binary compatibility reasons.
     * Availability: SENSORS_DEVICE_API_VERSION_1_3
     */
    #ifdef __LP64__
       uint64_t flags;
    #else
       uint32_t flags;
    #endif

    /* reserved fields, must be zero */
    void*           reserved[2];
};


/*
 * sensors_poll_device_t is used with SENSORS_DEVICE_API_VERSION_0_1
 * and is present for backward binary and source compatibility.
 * See the Sensors HAL interface section for complete descriptions of the
 * following functions:
 * http://source.android.com/devices/sensors/index.html#hal
 */
struct sensors_poll_device_t {
    struct hw_device_t common;
    int (*activate)(struct sensors_poll_device_t *dev,
            int sensor_handle, int enabled);
    int (*setDelay)(struct sensors_poll_device_t *dev,
            int sensor_handle, int64_t sampling_period_ns);
    int (*poll)(struct sensors_poll_device_t *dev,
            sensors_event_t* data, int count);
};

/*
 * struct sensors_poll_device_1 is used in HAL versions >= SENSORS_DEVICE_API_VERSION_1_0
 */
typedef struct sensors_poll_device_1 {
    union {
        /* sensors_poll_device_1 is compatible with sensors_poll_device_t,
         * and can be down-cast to it
         */
        struct sensors_poll_device_t v0;

        struct {
            struct hw_device_t common;

            /* Activate/de-activate one sensor. Return 0 on success, negative
             *
             * sensor_handle is the handle of the sensor to change.
             * enabled set to 1 to enable, or 0 to disable the sensor.
             *
             * Return 0 on success, negative errno code otherwise.
             */
            int (*activate)(struct sensors_poll_device_t *dev,
                    int sensor_handle, int enabled);

            /**
             * Set the events's period in nanoseconds for a given sensor.
             * If sampling_period_ns > max_delay it will be truncated to
             * max_delay and if sampling_period_ns < min_delay it will be
             * replaced by min_delay.
             */
            int (*setDelay)(struct sensors_poll_device_t *dev,
                    int sensor_handle, int64_t sampling_period_ns);

            /**
             * Returns an array of sensor data.
             */
            int (*poll)(struct sensors_poll_device_t *dev,
                    sensors_event_t* data, int count);
        };
    };


    /*
     * Sets a sensor’s parameters, including sampling frequency and maximum
     * report latency. This function can be called while the sensor is
     * activated, in which case it must not cause any sensor measurements to
     * be lost: transitioning from one sampling rate to the other cannot cause
     * lost events, nor can transitioning from a high maximum report latency to
     * a low maximum report latency.
     * See the Batching sensor results page for details:
     * http://source.android.com/devices/sensors/batching.html
     */
    int (*batch)(struct sensors_poll_device_1* dev,
            int sensor_handle, int flags, int64_t sampling_period_ns,
            int64_t max_report_latency_ns);

    /*
     * Flush adds a META_DATA_FLUSH_COMPLETE event (sensors_event_meta_data_t)
     * to the end of the "batch mode" FIFO for the specified sensor and flushes
     * the FIFO.
     * If the FIFO is empty or if the sensor doesn't support batching (FIFO size zero),
     * it should return SUCCESS along with a trivial META_DATA_FLUSH_COMPLETE event added to the
     * event stream. This applies to all sensors other than one-shot sensors.
     * If the sensor is a one-shot sensor, flush must return -EINVAL and not generate
     * any flush complete metadata.
     * If the sensor is not active at the time flush() is called, flush() should return
     * -EINVAL.
     */
    int (*flush)(struct sensors_poll_device_1* dev, int sensor_handle);

    /*
     * Inject a single sensor sample to be to this device.
     * data points to the sensor event to be injected
     * @return 0 on success
     *         -EPERM if operation is not allowed
     *         -EINVAL if sensor event cannot be injected
     */
    int (*inject_sensor_data)(struct sensors_poll_device_1 *dev, const sensors_event_t *data);

    void (*reserved_procs[7])(void);

} sensors_poll_device_1_t;


/** convenience API for opening and closing a device */

static inline int sensors_open(const struct hw_module_t* module,
        struct sensors_poll_device_t** device) {
    return module->methods->open(module,
            SENSORS_HARDWARE_POLL, (struct hw_device_t**)device);
}

static inline int sensors_close(struct sensors_poll_device_t* device) {
    return device->common.close(&device->common);
}

static inline int sensors_open_1(const struct hw_module_t* module,
        sensors_poll_device_1_t** device) {
    return module->methods->open(module,
            SENSORS_HARDWARE_POLL, (struct hw_device_t**)device);
}

static inline int sensors_close_1(sensors_poll_device_1_t* device) {
    return device->common.close(&device->common);
}

__END_DECLS

#endif  // ANDROID_SENSORS_INTERFACE_H
