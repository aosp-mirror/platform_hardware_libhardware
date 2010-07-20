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

#ifndef ANDROID_SENSORS_INTERFACE_H
#define ANDROID_SENSORS_INTERFACE_H

#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#include <hardware/hardware.h>
#include <cutils/native_handle.h>

__BEGIN_DECLS

/**
 * The id of this module
 */
#define SENSORS_HARDWARE_MODULE_ID "sensors"

/**
 * Name of the sensors device to open
 */
#define SENSORS_HARDWARE_CONTROL    "control"
#define SENSORS_HARDWARE_DATA       "data"

/**
 * Handles must be higher than SENSORS_HANDLE_BASE and must be unique.
 * A Handle identifies a given sensors. The handle is used to activate
 * and/or deactivate sensors.
 * In this version of the API there can only be 256 handles.
 */
#define SENSORS_HANDLE_BASE             0
#define SENSORS_HANDLE_BITS             8
#define SENSORS_HANDLE_COUNT            (1<<SENSORS_HANDLE_BITS)


/**
 * Sensor types
 */
#define SENSOR_TYPE_ACCELEROMETER       1
#define SENSOR_TYPE_MAGNETIC_FIELD      2
#define SENSOR_TYPE_ORIENTATION         3
#define SENSOR_TYPE_GYROSCOPE           4
#define SENSOR_TYPE_LIGHT               5
#define SENSOR_TYPE_PRESSURE            6
#define SENSOR_TYPE_TEMPERATURE         7
#define SENSOR_TYPE_PROXIMITY           8
#define SENSOR_TYPE_GRAVITY             9
#define SENSOR_TYPE_LINEAR_ACCELERATION 10
#define SENSOR_TYPE_ROTATION_VECTOR     11

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
#define GRAVITY_NEPTUNE         (11.0f)
#define GRAVITY_PLUTO           (0.6f)
#define GRAVITY_DEATH_STAR_I    (0.000000353036145f)
#define GRAVITY_THE_ISLAND      (4.815162342f)

/** Maximum magnetic field on Earth's surface */
#define MAGNETIC_FIELD_EARTH_MAX    (60.0f)

/** Minimum magnetic field on Earth's surface */
#define MAGNETIC_FIELD_EARTH_MIN    (30.0f)


/**
 * status of each sensor
 */

#define SENSOR_STATUS_UNRELIABLE        0
#define SENSOR_STATUS_ACCURACY_LOW      1
#define SENSOR_STATUS_ACCURACY_MEDIUM   2
#define SENSOR_STATUS_ACCURACY_HIGH     3

/**
 * Definition of the axis
 * ----------------------
 *
 * This API is relative to the screen of the device in its default orientation,
 * that is, if the device can be used in portrait or landscape, this API
 * is only relative to the NATURAL orientation of the screen. In other words,
 * the axis are not swapped when the device's screen orientation changes.
 * Higher level services /may/ perform this transformation.
 *
 *   x<0         x>0
 *                ^
 *                |
 *    +-----------+-->  y>0
 *    |           |
 *    |           |
 *    |           |
 *    |           |   / z<0
 *    |           |  /
 *    |           | /
 *    O-----------+/
 *    |[]  [ ]  []/
 *    +----------/+     y<0
 *              /
 *             /
 *           |/ z>0 (toward the sky)
 *
 *    O: Origin (x=0,y=0,z=0)
 *
 *
 * Orientation
 * ----------- 
 * 
 * All values are angles in degrees.
 * 
 * azimuth: angle between the magnetic north direction and the Y axis, around 
 *  the Z axis (0<=azimuth<360).
 *      0=North, 90=East, 180=South, 270=West
 * 
 * pitch: Rotation around X axis (-180<=pitch<=180), with positive values when
 *  the z-axis moves toward the y-axis.
 *
 * roll: Rotation around Y axis (-90<=roll<=90), with positive values when
 *  the x-axis moves towards the z-axis.
 *
 * Note: For historical reasons the roll angle is positive in the clockwise
 *  direction (mathematically speaking, it should be positive in the
 *  counter-clockwise direction):
 *
 *                Z
 *                ^
 *  (+roll)  .--> |
 *          /     |
 *         |      |  roll: rotation around Y axis
 *     X <-------(.)
 *                 Y
 *       note that +Y == -roll
 *
 *
 *
 * Note: This definition is different from yaw, pitch and roll used in aviation
 *  where the X axis is along the long side of the plane (tail to nose).
 *  
 *  
 * Acceleration
 * ------------
 *
 *  All values are in SI units (m/s^2) and measure the acceleration of the
 *  device minus the force of gravity.
 *  
 *  x: Acceleration minus Gx on the x-axis 
 *  y: Acceleration minus Gy on the y-axis 
 *  z: Acceleration minus Gz on the z-axis
 *  
 *  Examples:
 *    When the device lies flat on a table and is pushed on its left side
 *    toward the right, the x acceleration value is positive.
 *    
 *    When the device lies flat on a table, the acceleration value is +9.81,
 *    which correspond to the acceleration of the device (0 m/s^2) minus the
 *    force of gravity (-9.81 m/s^2).
 *    
 *    When the device lies flat on a table and is pushed toward the sky, the
 *    acceleration value is greater than +9.81, which correspond to the
 *    acceleration of the device (+A m/s^2) minus the force of 
 *    gravity (-9.81 m/s^2).
 *    
 *    
 * Magnetic Field
 * --------------
 * 
 *  All values are in micro-Tesla (uT) and measure the ambient magnetic
 *  field in the X, Y and Z axis.
 *
 * Gyroscope
 * ---------
 *  All values are in radians/second and measure the rate of rotation
 *  around the X, Y and Z axis.  The coordinate system is the same as is
 *  used for the acceleration sensor.  Rotation is positive in the counter-clockwise
 *  direction.  That is, an observer looking from some positive location on the x, y.
 *  or z axis at a device positioned on the origin would report positive rotation
 *  if the device appeared to be rotating counter clockwise.  Note that this is the
 *  standard mathematical definition of positive rotation and does not agree with the
 *  definition of roll given earlier.
 *
 * Proximity
 * ---------
 *
 * The distance value is measured in centimeters.  Note that some proximity
 * sensors only support a binary "close" or "far" measurement.  In this case,
 * the sensor should report its maxRange value in the "far" state and a value
 * less than maxRange in the "near" state.
 *
 * Light
 * -----
 *
 * The light sensor value is returned in SI lux units.
 *
 * Gravity
 * -------
 * A gravity output indicates the direction of and magnitude of gravity in the devices's
 * coordinates.  On Earth, the magnitude is 9.8.  Units are m/s^2.  The coordinate system
 * is the same as is used for the acceleration sensor.
 *
 * Linear Acceleration
 * -------------------
 * Indicates the linear acceleration of the device in device coordinates, not including gravity.
 * This output is essentially Acceleration - Gravity.  Units are m/s^2.  The coordinate system is
 * the same as is used for the acceleration sensor.
 *
 * Rotation Vector
 * ---------------
 * A rotation vector represents the orientation of the device as a combination
 * of an angle and an axis, in which the device has rotated through an angle
 * theta around an axis <x, y, z>. The three elements of the rotation vector
 * are <x*sin(theta/2), y*sin(theta/2), z*sin(theta/2)>, such that the magnitude
 * of the rotation vector is equal to sin(theta/2), and the direction of the
 * rotation vector is equal to the direction of the axis of rotation. The three
 * elements of the rotation vector are equal to the last three components of a
 * unit quaternion <cos(theta/2), x*sin(theta/2), y*sin(theta/2), z*sin(theta/2)>.
 * Elements of the rotation vector are unitless.  The x, y, and z axis are defined
 * in the same was as for the acceleration sensor.
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
 * Union of the various types of sensor data
 * that can be returned.
 */
typedef struct {
    /* sensor identifier */
    int             sensor;

    union {
        /* x,y,z values of the given sensor */
        sensors_vec_t   vector;

        /* orientation values are in degrees */
        sensors_vec_t   orientation;

        /* acceleration values are in meter per second per second (m/s^2) */
        sensors_vec_t   acceleration;

        /* magnetic vector values are in micro-Tesla (uT) */
        sensors_vec_t   magnetic;

        /* temperature is in degrees centigrade (Celsius) */
        float           temperature;

        /* distance in centimeters */
        float           distance;

        /* light in SI lux units */
        float           light;
    };

    /* time is in nanosecond */
    int64_t         time;

    uint32_t        reserved;
} sensors_data_t;


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
    /* name of this sensors */
    const char*     name;
    /* vendor of the hardware part */
    const char*     vendor;
    /* version of the hardware part + driver. The value of this field is
     * left to the implementation and doesn't have to be monotonicaly
     * increasing.
     */    
    int             version;
    /* handle that identifies this sensors. This handle is used to activate
     * and deactivate this sensor. The value of the handle must be 8 bits
     * in this version of the API. 
     */
    int             handle;
    /* this sensor's type. */
    int             type;
    /* maximaum range of this sensor's value in SI units */
    float           maxRange;
    /* smallest difference between two values reported by this sensor */
    float           resolution;
    /* rough estimate of this sensor's power consumption in mA */
    float           power;
    /* reserved fields, must be zero */
    void*           reserved[9];
};


/**
 * Every device data structure must begin with hw_device_t
 * followed by module specific public methods and attributes.
 */
struct sensors_control_device_t {
    struct hw_device_t common;
    
    /**
     * Returns a native_handle_t, which will be the parameter to
     * sensors_data_device_t::open_data(). 
     * The caller takes ownership of this handle. This is intended to be
     * passed cross processes.
     *
     * @return a native_handle_t if successful, NULL on error
     */
    native_handle_t* (*open_data_source)(struct sensors_control_device_t *dev);

    /**
     * Releases any resources that were created by open_data_source.
     * This call is optional and can be NULL if not implemented
     * by the sensor HAL.
     *
     * @return 0 if successful, < 0 on error
     */
    int (*close_data_source)(struct sensors_control_device_t *dev);

    /** Activate/deactivate one sensor.
     *
     * @param handle is the handle of the sensor to change.
     * @param enabled set to 1 to enable, or 0 to disable the sensor.
     *
     * @return 0 on success, negative errno code otherwise
     */
    int (*activate)(struct sensors_control_device_t *dev, 
            int handle, int enabled);
    
    /**
     * Set the delay between sensor events in ms
     *
     * @return 0 if successful, < 0 on error
     */
    int (*set_delay)(struct sensors_control_device_t *dev, int32_t ms);

    /**
     * Causes sensors_data_device_t.poll() to return -EWOULDBLOCK immediately.
     */
    int (*wake)(struct sensors_control_device_t *dev);
};

struct sensors_data_device_t {
    struct hw_device_t common;

    /**
     * Prepare to read sensor data.
     *
     * This routine does NOT take ownership of the handle
     * and must not close it. Typically this routine would
     * use a duplicate of the nh parameter.
     *
     * @param nh from sensors_control_open.
     *
     * @return 0 if successful, < 0 on error
     */
    int (*data_open)(struct sensors_data_device_t *dev, native_handle_t* nh);
    
    /**
     * Caller has completed using the sensor data.
     * The caller will not be blocked in sensors_data_poll
     * when this routine is called.
     *
     * @return 0 if successful, < 0 on error
     */
    int (*data_close)(struct sensors_data_device_t *dev);
    
    /**
     * Return sensor data for one of the enabled sensors.
     *
     * @return sensor handle for the returned data, 0x7FFFFFFF when 
     * sensors_control_device_t.wake() is called and -errno on error
     *  
     */
    int (*poll)(struct sensors_data_device_t *dev, 
            sensors_data_t* data);
};


/** convenience API for opening and closing a device */

static inline int sensors_control_open(const struct hw_module_t* module, 
        struct sensors_control_device_t** device) {
    return module->methods->open(module, 
            SENSORS_HARDWARE_CONTROL, (struct hw_device_t**)device);
}

static inline int sensors_control_close(struct sensors_control_device_t* device) {
    return device->common.close(&device->common);
}

static inline int sensors_data_open(const struct hw_module_t* module, 
        struct sensors_data_device_t** device) {
    return module->methods->open(module, 
            SENSORS_HARDWARE_DATA, (struct hw_device_t**)device);
}

static inline int sensors_data_close(struct sensors_data_device_t* device) {
    return device->common.close(&device->common);
}


__END_DECLS

#endif  // ANDROID_SENSORS_INTERFACE_H
