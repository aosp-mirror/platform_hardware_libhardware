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
 * Availability: SENSORS_DEVICE_API_VERSION_1_3
 * Sensor flags used in sensor_t.flags.
 */
enum {
    /*
     * Whether this sensor wakes up the AP from suspend mode when data is available.
     */
    SENSOR_FLAG_WAKE_UP = 1U << 0
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
 *  - When launching glass on the L release, Google should now use the official
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
 * trigger-mode: n/a
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
 * SENSOR_TYPE_ACCELEROMETER
 * trigger-mode: continuous
 * wake-up sensor: no
 *
 *  All values are in SI units (m/s^2) and measure the acceleration of the
 *  device minus the force of gravity.
 *
 */
#define SENSOR_TYPE_ACCELEROMETER                    (1)
#define SENSOR_STRING_TYPE_ACCELEROMETER             "android.sensor.accelerometer"

/*
 * SENSOR_TYPE_GEOMAGNETIC_FIELD
 * trigger-mode: continuous
 * wake-up sensor: no
 *
 *  All values are in micro-Tesla (uT) and measure the geomagnetic
 *  field in the X, Y and Z axis.
 *
 */
#define SENSOR_TYPE_GEOMAGNETIC_FIELD                (2)
#define SENSOR_TYPE_MAGNETIC_FIELD  SENSOR_TYPE_GEOMAGNETIC_FIELD
#define SENSOR_STRING_TYPE_MAGNETIC_FIELD            "android.sensor.magnetic_field"

/*
 * SENSOR_TYPE_ORIENTATION
 * trigger-mode: continuous
 * wake-up sensor: no
 *
 * All values are angles in degrees.
 *
 * Orientation sensors return sensor events for all 3 axes at a constant
 * rate defined by setDelay().
 */
#define SENSOR_TYPE_ORIENTATION                      (3)
#define SENSOR_STRING_TYPE_ORIENTATION               "android.sensor.orientation"

/*
 * SENSOR_TYPE_GYROSCOPE
 * trigger-mode: continuous
 * wake-up sensor: no
 *
 *  All values are in radians/second and measure the rate of rotation
 *  around the X, Y and Z axis.
 */
#define SENSOR_TYPE_GYROSCOPE                        (4)
#define SENSOR_STRING_TYPE_GYROSCOPE                 "android.sensor.gyroscope"

/*
 * SENSOR_TYPE_LIGHT
 * trigger-mode: on-change
 * wake-up sensor: no
 *
 * The light sensor value is returned in SI lux units.
 */
#define SENSOR_TYPE_LIGHT                            (5)
#define SENSOR_STRING_TYPE_LIGHT                     "android.sensor.light"

/*
 * SENSOR_TYPE_PRESSURE
 * trigger-mode: continuous
 * wake-up sensor: no
 *
 * The pressure sensor return the athmospheric pressure in hectopascal (hPa)
 */
#define SENSOR_TYPE_PRESSURE                         (6)
#define SENSOR_STRING_TYPE_PRESSURE                  "android.sensor.pressure"

/* SENSOR_TYPE_TEMPERATURE is deprecated in the HAL */
#define SENSOR_TYPE_TEMPERATURE                      (7)
#define SENSOR_STRING_TYPE_TEMPERATURE               "android.sensor.temperature"

/*
 * SENSOR_TYPE_PROXIMITY
 * trigger-mode: on-change
 * wake-up sensor: yes (set SENSOR_FLAG_WAKE_UP flag)
 *
 * The value corresponds to the distance to the nearest object in centimeters.
 */
#define SENSOR_TYPE_PROXIMITY                        (8)
#define SENSOR_STRING_TYPE_PROXIMITY                 "android.sensor.proximity"

/*
 * SENSOR_TYPE_GRAVITY
 * trigger-mode: continuous
 * wake-up sensor: no
 *
 * A gravity output indicates the direction of and magnitude of gravity in
 * the devices's coordinates.
 */
#define SENSOR_TYPE_GRAVITY                          (9)
#define SENSOR_STRING_TYPE_GRAVITY                   "android.sensor.gravity"

/*
 * SENSOR_TYPE_LINEAR_ACCELERATION
 * trigger-mode: continuous
 * wake-up sensor: no
 *
 * Indicates the linear acceleration of the device in device coordinates,
 * not including gravity.
 */
#define SENSOR_TYPE_LINEAR_ACCELERATION             (10)
#define SENSOR_STRING_TYPE_LINEAR_ACCELERATION      "android.sensor.linear_acceleration"


/*
 * SENSOR_TYPE_ROTATION_VECTOR
 * trigger-mode: continuous
 * wake-up sensor: no
 *
 * The rotation vector symbolizes the orientation of the device relative to the
 * East-North-Up coordinates frame.
 */
#define SENSOR_TYPE_ROTATION_VECTOR                 (11)
#define SENSOR_STRING_TYPE_ROTATION_VECTOR          "android.sensor.rotation_vector"

/*
 * SENSOR_TYPE_RELATIVE_HUMIDITY
 * trigger-mode: on-change
 * wake-up sensor: no
 *
 * A relative humidity sensor measures relative ambient air humidity and
 * returns a value in percent.
 */
#define SENSOR_TYPE_RELATIVE_HUMIDITY               (12)
#define SENSOR_STRING_TYPE_RELATIVE_HUMIDITY        "android.sensor.relative_humidity"

/*
 * SENSOR_TYPE_AMBIENT_TEMPERATURE
 * trigger-mode: on-change
 * wake-up sensor: no
 *
 * The ambient (room) temperature in degree Celsius.
 */
#define SENSOR_TYPE_AMBIENT_TEMPERATURE             (13)
#define SENSOR_STRING_TYPE_AMBIENT_TEMPERATURE      "android.sensor.ambient_temperature"

/*
 * SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED
 * trigger-mode: continuous
 * wake-up sensor: no
 *
 *  Similar to SENSOR_TYPE_MAGNETIC_FIELD, but the hard iron calibration is
 *  reported separately instead of being included in the measurement.
 */
#define SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED     (14)
#define SENSOR_STRING_TYPE_MAGNETIC_FIELD_UNCALIBRATED "android.sensor.magnetic_field_uncalibrated"

/*
 * SENSOR_TYPE_GAME_ROTATION_VECTOR
 * trigger-mode: continuous
 * wake-up sensor: no
 *
 *  Similar to SENSOR_TYPE_ROTATION_VECTOR, but not using the geomagnetic
 *  field.
 */
#define SENSOR_TYPE_GAME_ROTATION_VECTOR            (15)
#define SENSOR_STRING_TYPE_GAME_ROTATION_VECTOR     "android.sensor.game_rotation_vector"

/*
 * SENSOR_TYPE_GYROSCOPE_UNCALIBRATED
 * trigger-mode: continuous
 * wake-up sensor: no
 *
 *  All values are in radians/second and measure the rate of rotation
 *  around the X, Y and Z axis.
 */
#define SENSOR_TYPE_GYROSCOPE_UNCALIBRATED          (16)
#define SENSOR_STRING_TYPE_GYROSCOPE_UNCALIBRATED   "android.sensor.gyroscope_uncalibrated"

/*
 * SENSOR_TYPE_SIGNIFICANT_MOTION
 * trigger-mode: one-shot
 * wake-up sensor: yes (set SENSOR_FLAG_WAKE_UP flag)
 *
 * A sensor of this type triggers an event each time significant motion
 * is detected and automatically disables itself.
 * The only allowed value to return is 1.0.
 */

#define SENSOR_TYPE_SIGNIFICANT_MOTION              (17)
#define SENSOR_STRING_TYPE_SIGNIFICANT_MOTION       "android.sensor.significant_motion"

/*
 * SENSOR_TYPE_STEP_DETECTOR
 * trigger-mode: special
 * wake-up sensor: no
 *
 * A sensor of this type triggers an event each time a step is taken
 * by the user. The only allowed value to return is 1.0 and an event
 * is generated for each step.
 */

#define SENSOR_TYPE_STEP_DETECTOR                   (18)
#define SENSOR_STRING_TYPE_STEP_DETECTOR            "android.sensor.step_detector"


/*
 * SENSOR_TYPE_STEP_COUNTER
 * trigger-mode: on-change
 * wake-up sensor: no
 *
 * A sensor of this type returns the number of steps taken by the user since
 * the last reboot while activated. The value is returned as a uint64_t and is
 * reset to zero only on a system / android reboot.
 */

#define SENSOR_TYPE_STEP_COUNTER                    (19)
#define SENSOR_STRING_TYPE_STEP_COUNTER             "android.sensor.step_counter"

/*
 * SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR
 * trigger-mode: continuous
 * wake-up sensor: no
 *
 *  Similar to SENSOR_TYPE_ROTATION_VECTOR, but using a magnetometer instead
 *  of using a gyroscope.
 */
#define SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR     (20)
#define SENSOR_STRING_TYPE_GEOMAGNETIC_ROTATION_VECTOR "android.sensor.geomagnetic_rotation_vector"

/*
 * SENSOR_TYPE_HEART_RATE
 * trigger-mode: on-change
 * wake-up sensor: no
 *
 *  A sensor of this type returns the current heart rate.
 *  The events contain the current heart rate in beats per minute (BPM) and the
 *  status of the sensor during the measurement. See heart_rate_event_t for more
 *  details.
 *
 *  Because this sensor is on-change, events must be generated when and only
 *  when heart_rate.bpm or heart_rate.status have changed since the last
 *  event. The event should be generated no faster than every period_ns passed
 *  to setDelay() or to batch(). See the definition of the on-change trigger
 *  mode for more information.
 *
 *  sensor_t.requiredPermission must be set to SENSOR_PERMISSION_BODY_SENSORS.
 */
#define SENSOR_TYPE_HEART_RATE                      (21)
#define SENSOR_STRING_TYPE_HEART_RATE               "android.sensor.heart_rate"

/*
 * SENSOR_TYPE_NON_WAKE_UP_PROXIMITY_SENSOR
 * Same as proximity_sensor but does not wake up the AP from suspend mode.
 * wake-up sensor: no
 */
#define SENSOR_TYPE_NON_WAKE_UP_PROXIMITY_SENSOR           (22)
#define SENSOR_STRING_TYPE_NON_WAKE_UP_PROXIMITY_SENSOR    "android.sensor.non_wake_up_proximity_sensor"

/*
 * The sensors below are wake_up variants of the base sensor types defined
 * above. When registered in batch mode, these sensors will wake up the AP when
 * their FIFOs are full or when the batch timeout expires. A separate FIFO has
 * to be maintained for wake up sensors and non wake up sensors. The non wake-up
 * sensors need to overwrite their FIFOs when they are full till the AP wakes up
 * and the wake-up sensors will wake-up the AP when their FIFOs are full or when
 * the batch timeout expires without losing events.
 * Note: Sensors of type SENSOR_TYPE_PROXIMITY are also wake up sensors and
 * should be treated as such. Wake-up one-shot sensors like SIGNIFICANT_MOTION
 * cannot be batched, hence the text about batch above doesn't apply to them.
 *
 * Define these sensors only if:
 * 1) batching is supported.
 * 2) wake-up and non wake-up variants of each sensor can be activated at
 *    different rates.
 *
 * wake-up sensor: yes
 * Set SENSOR_FLAG_WAKE_UP flag for all these sensors.
 */
#define SENSOR_TYPE_WAKE_UP_ACCELEROMETER                      (23)
#define SENSOR_STRING_TYPE_WAKE_UP_ACCELEROMETER               "android.sensor.wake_up_accelerometer"

#define SENSOR_TYPE_WAKE_UP_MAGNETIC_FIELD                     (24)
#define SENSOR_STRING_TYPE_WAKE_UP_MAGNETIC_FIELD              "android.sensor.wake_up_magnetic_field"

#define SENSOR_TYPE_WAKE_UP_ORIENTATION                        (25)
#define SENSOR_STRING_TYPE_WAKE_UP_ORIENTATION                 "android.sensor.wake_up_orientation"

#define SENSOR_TYPE_WAKE_UP_GYROSCOPE                          (26)
#define SENSOR_STRING_TYPE_WAKE_UP_GYROSCOPE                   "android.sensor.wake_up_gyroscope"

#define SENSOR_TYPE_WAKE_UP_LIGHT                              (27)
#define SENSOR_STRING_TYPE_WAKE_UP_LIGHT                       "android.sensor.wake_up_light"

#define SENSOR_TYPE_WAKE_UP_PRESSURE                           (28)
#define SENSOR_STRING_TYPE_WAKE_UP_PRESSURE                    "android.sensor.wake_up_pressure"

#define SENSOR_TYPE_WAKE_UP_GRAVITY                            (29)
#define SENSOR_STRING_TYPE_WAKE_UP_GRAVITY                     "android.sensor.wake_up_gravity"

#define SENSOR_TYPE_WAKE_UP_LINEAR_ACCELERATION                (30)
#define SENSOR_STRING_TYPE_WAKE_UP_LINEAR_ACCELERATION         "android.sensor.wake_up_linear_acceleration"

#define SENSOR_TYPE_WAKE_UP_ROTATION_VECTOR                    (31)
#define SENSOR_STRING_TYPE_WAKE_UP_ROTATION_VECTOR             "android.sensor.wake_up_rotation_vector"

#define SENSOR_TYPE_WAKE_UP_RELATIVE_HUMIDITY                  (32)
#define SENSOR_STRING_TYPE_WAKE_UP_RELATIVE_HUMIDITY           "android.sensor.wake_up_relative_humidity"

#define SENSOR_TYPE_WAKE_UP_AMBIENT_TEMPERATURE                (33)
#define SENSOR_STRING_TYPE_WAKE_UP_AMBIENT_TEMPERATURE         "android.sensor.wake_up_ambient_temperature"

#define SENSOR_TYPE_WAKE_UP_MAGNETIC_FIELD_UNCALIBRATED        (34)
#define SENSOR_STRING_TYPE_WAKE_UP_MAGNETIC_FIELD_UNCALIBRATED "android.sensor.wake_up_magnetic_field_uncalibrated"

#define SENSOR_TYPE_WAKE_UP_GAME_ROTATION_VECTOR               (35)
#define SENSOR_STRING_TYPE_WAKE_UP_GAME_ROTATION_VECTOR        "android.sensor.wake_up_game_rotation_vector"

#define SENSOR_TYPE_WAKE_UP_GYROSCOPE_UNCALIBRATED             (36)
#define SENSOR_STRING_TYPE_WAKE_UP_GYROSCOPE_UNCALIBRATED      "android.sensor.wake_up_gyroscope_uncalibrated"

#define SENSOR_TYPE_WAKE_UP_STEP_DETECTOR                      (37)
#define SENSOR_STRING_TYPE_WAKE_UP_STEP_DETECTOR               "android.sensor.wake_up_step_detector"

#define SENSOR_TYPE_WAKE_UP_STEP_COUNTER                       (38)
#define SENSOR_STRING_TYPE_WAKE_UP_STEP_COUNTER                "android.sensor.wake_up_step_counter"

#define SENSOR_TYPE_WAKE_UP_GEOMAGNETIC_ROTATION_VECTOR        (39)
#define SENSOR_STRING_TYPE_WAKE_UP_GEOMAGNETIC_ROTATION_VECTOR "android.sensor.wake_up_geomagnetic_rotation_vector"

#define SENSOR_TYPE_WAKE_UP_HEART_RATE                         (40)
#define SENSOR_STRING_TYPE_WAKE_UP_HEART_RATE                  "android.sensor.wake_up_heart_rate"

/*
 * SENSOR_TYPE_WAKE_UP_TILT_DETECTOR
 * trigger-mode: special (setDelay has no impact)
 * wake-up sensor: yes (set SENSOR_FLAG_WAKE_UP flag)
 *
 * A sensor of this type generates an event each time a tilt event is detected. A tilt event
 * should be generated if the direction of the 2-seconds window average gravity changed by at least
 * 35 degrees since the activation of the sensor.
 *     initial_estimated_gravity = average of accelerometer measurements over the first
 *                                 1 second after activation.
 *     current_estimated_gravity = average of accelerometer measurements over the last 2 seconds.
 *     trigger when angle (initial_estimated_gravity, current_estimated_gravity) > 35 degrees
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
 */
#define SENSOR_TYPE_WAKE_UP_TILT_DETECTOR                      (41)
#define SENSOR_STRING_TYPE_WAKE_UP_TILT_DETECTOR               "android.sensor.wake_up_tilt_detector"

/*
 * SENSOR_TYPE_WAKE_GESTURE
 * trigger-mode: one-shot
 * wake-up sensor: yes (set SENSOR_FLAG_WAKE_UP flag)
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
 */
#define SENSOR_TYPE_WAKE_GESTURE                               (42)
#define SENSOR_STRING_TYPE_WAKE_GESTURE                        "android.sensor.wake_gesture"

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

typedef struct meta_data_event {
    int32_t what;
    int32_t sensor;
} meta_data_event_t;

/**
 * Heart rate event data
 */
typedef struct {
  // Heart rate in beats per minute.
  // Set to 0 when status is SENSOR_STATUS_UNRELIABLE or ..._NO_CONTACT
  float bpm;
  // Status of the sensor for this reading. Set to one SENSOR_STATUS_...
  int8_t status;
} heart_rate_event_t;

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


struct sensor_t;

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

    /* this value depends on the trigger mode:
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

    /* This value is defined only for continuous mode sensors. It is the delay between two
     * sensor events corresponding to the lowest frequency that this sensor supports. When
     * lower frequencies are requested through batch()/setDelay() the events will be generated
     * at this frequency instead. It can be used by the framework or applications to estimate
     * when the batch FIFO may be full.
     * NOTE: period_ns is in nanoseconds where as maxDelay/minDelay are in microseconds.
     *     continuous: maximum sampling period allowed in microseconds.
     *     on-change, one-shot, special : -1
     * Availability: SENSORS_DEVICE_API_VERSION_1_3
     */
    #ifdef __LP64__
       int64_t maxDelay;
    #else
       int32_t maxDelay;
    #endif

    /* Flags for sensor. See SENSOR_FLAG_* above. */
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
            int handle, int enabled);
    int (*setDelay)(struct sensors_poll_device_t *dev,
            int handle, int64_t ns);
    int (*poll)(struct sensors_poll_device_t *dev,
            sensors_event_t* data, int count);
};

/*
 * struct sensors_poll_device_1 is used with SENSORS_DEVICE_API_VERSION_1_0
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
             * handle is the handle of the sensor to change.
             * enabled set to 1 to enable, or 0 to disable the sensor.
             *
             * Return 0 on success, negative errno code otherwise.
             */
            int (*activate)(struct sensors_poll_device_t *dev,
                    int handle, int enabled);

            /**
             * Set the events's period in nanoseconds for a given sensor. If
             * period_ns > max_delay it will be truncated to max_delay and if
             * period_ns < min_delay it will be replaced by min_delay.
             */
            int (*setDelay)(struct sensors_poll_device_t *dev,
                    int handle, int64_t period_ns);

            /**
             * Returns an array of sensor data.
             */
            int (*poll)(struct sensors_poll_device_t *dev,
                    sensors_event_t* data, int count);
        };
    };


    /*
     * Enables batch mode for the given sensor and sets the delay between events.
     * See the Batching sensor results page for details:
     * http://source.android.com/devices/sensors/batching.html
     */
    int (*batch)(struct sensors_poll_device_1* dev,
            int handle, int flags, int64_t period_ns, int64_t timeout);

    /*
     * Flush adds a META_DATA_FLUSH_COMPLETE event (sensors_event_meta_data_t)
     * to the end of the "batch mode" FIFO for the specified sensor and flushes
     * the FIFO.
     */
    int (*flush)(struct sensors_poll_device_1* dev, int handle);

    void (*reserved_procs[8])(void);

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
