/*
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef _HARDWARE_SENSORS_H
#define _HARDWARE_SENSORS_H

#include <stdint.h>

#if __cplusplus
extern "C" {
#endif

/**
 * Sensor IDs must be a power of two and
 * must match values in SensorManager.java
 */

#define SENSORS_ORIENTATION     0x00000001
#define SENSORS_ACCELERATION    0x00000002
#define SENSORS_TEMPERATURE     0x00000004
#define SENSORS_MAGNETIC_FIELD  0x00000008
#define SENSORS_LIGHT           0x00000010
#define SENSORS_PROXIMITY       0x00000020
#define SENSORS_TRICORDER       0x00000040
#define SENSORS_ORIENTATION_RAW 0x00000080
#define SENSORS_MASK            0x000000FF

/**
 * Values returned by the accelerometer in various locations in the universe.
 * all values are in SI units (m/s^2)
 */

#define GRAVITY_SUN             (275.0f)
#define GRAVITY_MERCURY         (3.70f)
#define GRAVITY_VENUS           (8.87f)
#define GRAVITY_EARTH           (9.80665f)
#define GRAVITY_MOON            (1.6f)
#define GRAVITY_MARS            (3.71f)
#define GRAVITY_JUPITER         (23.12f)
#define GRAVITY_SATURN          (8.96f)
#define GRAVITY_URANUS          (8.69f)
#define GRAVITY_NEPTUN          (11.0f)
#define GRAVITY_PLUTO           (0.6f)
#define GRAVITY_DEATH_STAR_I    (0.000000353036145f)
#define GRAVITY_THE_ISLAND      (4.815162342f)

/** Maximum magnetic field on Earth's surface */
#define MAGNETIC_FIELD_EARTH_MAX    (60.0f)

/** Minimum magnetic field on Earth's surface */
#define MAGNETIC_FIELD_EARTH_MIN    (30.0f)

/**
 * Various luminance values during the day (lux)
 */

#define LIGHT_SUNLIGHT_MAX      (120000.0f)
#define LIGHT_SUNLIGHT          (110000.0f)
#define LIGHT_SHADE             (20000.0f)
#define LIGHT_OVERCAST          (10000.0f)
#define LIGHT_SUNRISE           (400.0f)
#define LIGHT_CLOUDY            (100.0f)

/*
 * Various luminance values during the night (lux)
 */

#define LIGHT_FULLMOON          (0.25f)
#define LIGHT_NO_MOON           (0.001f)

/**
 * status of each sensor
 */

#define SENSOR_STATUS_UNRELIABLE        0
#define SENSOR_STATUS_ACCURACY_LOW      1
#define SENSOR_STATUS_ACCURACY_MEDIUM   2
#define SENSOR_STATUS_ACCURACY_HIGH     3

/**
 * Definition of the axis
 *
 * This API is relative to the screen of the device in its default orientation,
 * that is, if the device can be used in portrait or landscape, this API
 * is only relative to the NATURAL orientation of the screen. In other words,
 * the axis are not swapped when the device's screen orientation changes.
 * Higher level services /may/ perform this transformation.
 *
 *    -x         +x
 *                ^
 *                |
 *    +-----------+-->  +y
 *    |           |
 *    |           |
 *    |           |
 *    |           |   / -z
 *    |           |  /
 *    |           | /
 *    +-----------+/
 *    | o   O   o /
 *    +----------/+     -y
 *              /
 *             /
 *           |/ +z
 *
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
            float yaw;
            float pitch;
            float roll;
        };
    };
    int8_t status;
    uint8_t reserved[3];
} sensors_vec_t;

/**
 * Union of the various types of sensor data
 * that can be returned.
 */
typedef struct {
    /* sensor identifier */
    int             sensor;

    union {
        /* x,y,z values of the given sensor */
        sensors_vec_t   vector;

        /* orientation values are in degres */
        sensors_vec_t   orientation;

        /* acceleration values are in meter per second per second (m/s^2) */
        sensors_vec_t   acceleration;

        /* magnetic vector values are in micro-Tesla (uT) */
        sensors_vec_t   magnetic;

        /* temperature is in degres C */
        float           temperature;
    };

    /* time is in nanosecond */
    int64_t         time;

    uint32_t        reserved;
} sensors_data_t;

/**
 * Initialize the module. This is the first entry point
 * called and typically initializes the hardware.
 *
 * @return bit map of available sensors defined by
 *         the constants SENSORS_XXXX.
 */
uint32_t sensors_control_init();

/**
 * Returns the fd which will be the parameter to
 * sensors_data_open. The caller takes ownership
 * of this fd.
 *
 * @return a fd if successful, < 0 on error
 */
int sensors_control_open();

/** Activate/deactiveate one or more of the sensors.
 *
 * @param sensors is a bitmask of the sensors to change.
 * @param mask is a bitmask for enabling/disabling sensors.
 *
 * @return bitmask of SENSORS_XXXX indicating which sensors are enabled
 */
uint32_t sensors_control_activate(uint32_t sensors, uint32_t mask);

/**
 * Set the delay between sensor events in ms
 *
 * @return 0 if successful, < 0 on error
 */
int sensors_control_delay(int32_t ms);

/**
 * Prepare to read sensor data.
 *
 * This routiune does NOT take ownership of the fd
 * and must not close it. Typcially this routine would
 * use a duplicate of the fd parameter.
 *
 * @param fd from sensors_control_open.
 *
 * @return 0 if successful, < 0 on error
 */
int sensors_data_open(int fd);

/**
 * Caller has completed using the sensor data.
 * The caller will not be blocked in sensors_data_poll
 * when this routine is called.
 *
 * @return 0 if successful, < 0 on error
 */
int sensors_data_close();

/**
 * Return sensor data for one of the enabled sensors.
 *
 * @return SENSOR_XXXX for the returned data, -1 on error
 * */
int sensors_data_poll(sensors_data_t* data, uint32_t sensors_of_interest);

/**
 * @return bit map of available sensors defined by
 *         the constants SENSORS_XXXX.
 */
uint32_t sensors_data_get_sensors();


#if __cplusplus
}  // extern "C"
#endif

#endif  // _HARDWARE_SENSORS_H
