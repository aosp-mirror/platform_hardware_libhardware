/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ANDROID_VEHICLE_INTERFACE_H
#define ANDROID_VEHICLE_INTERFACE_H

#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <math.h>
#include <errno.h>

#include <hardware/hardware.h>
#include <cutils/native_handle.h>

__BEGIN_DECLS

/*****************************************************************************/

#define VEHICLE_HEADER_VERSION          1
#define VEHICLE_MODULE_API_VERSION_1_0  HARDWARE_MODULE_API_VERSION(1, 0)
#define VEHICLE_DEVICE_API_VERSION_1_0  HARDWARE_DEVICE_API_VERSION_2(1, 0, VEHICLE_HEADER_VERSION)

/**
 * Vehicle HAL to provide interfaces to various Car related sensors. The HAL is
 * designed in a property, value maping where each property has a value which
 * can be "get", "set" and "(un)subscribed" to. Subscribing will require the
 * user of this HAL to provide parameters such as sampling rate.
 */


/*
 * The id of this module
 */
#define VEHICLE_HARDWARE_MODULE_ID  "vehicle"

/**
 *  Name of the vehicle device to open
 */
#define VEHICLE_HARDWARE_DEVICE     "vehicle_hw_device"

/**
 * Each vehicle property is defined with various annotations to specify the type of information.
 * Annotations will be used by scripts to run some type check or generate some boiler-plate codes.
 * Also the annotations are the specification for each property, and each HAL implementation should
 * follow what is specified as annotations.
 * Here is the list of annotations with explanation on what it does:
 * @value_type: Type of data for this property. One of the value from vehicle_value_type should be
 *              set here.
 * @change_mode: How this property changes. Value set is from vehicle_prop_change_mode. Some
 *               properties can allow either on change or continuous mode and it is up to HAL
 *               implementation to choose which mode to use.
 * @access: Define how this property can be accessed. read only, write only or R/W from
 *          vehicle_prop_access
 * @data_member: Name of member from vehicle_value union to access this data.
 * @data_enum: enum type that should be used for the data.
 * @unit: Unit of data. Should be from vehicle_unit_type.
 * @config_flags: Usage of config_flags in vehicle_prop_config
 * @config_array: Usage of config_array in vehicle_prop_config. When this is specified,
 *                @config_flags will not be used.
 * @config_string: Explains the usage of config_string in vehicle_prop_config. Property with
 *                 this annotation is expected to have additional information in config_string
 *                 for that property to work.
 * @zone_type type of zoned used. defined for zoned property
 * @range_start, @range_end : define range of specific property values.
 * @allow_out_of_range_value : This property allows out of range value to deliver additional
 *                             information. Check VEHICLE_*_OUT_OF_RANGE_* for applicable values.
 */
//===== Vehicle Information ====

/**
 * Invalid property value used for argument where invalid property gives different result.
 */
#define VEHICLE_PROPERTY_INVALID (0x0)

/**
 * VIN of vehicle
 * @value_type VEHICLE_VALUE_TYPE_STRING
 * @change_mode VEHICLE_PROP_CHANGE_MODE_STATIC
 * @access VEHICLE_PROP_ACCESS_READ
 * @data_member info_vin
 */
#define VEHICLE_PROPERTY_INFO_VIN                                   (0x00000100)

/**
 * Maker name of vehicle
 * @value_type VEHICLE_VALUE_TYPE_STRING
 * @change_mode VEHICLE_PROP_CHANGE_MODE_STATIC
 * @access VEHICLE_PROP_ACCESS_READ
 * @data_member info_make
 */
#define VEHICLE_PROPERTY_INFO_MAKE                                  (0x00000101)

/**
 * Model of vehicle
 * @value_type VEHICLE_VALUE_TYPE_STRING
 * @change_mode VEHICLE_PROP_CHANGE_MODE_STATIC
 * @access VEHICLE_PROP_ACCESS_READ
 * @data_member info_model
 */
#define VEHICLE_PROPERTY_INFO_MODEL                                 (0x00000102)

/**
 * Model year of vehicle.
 * @value_type VEHICLE_VALUE_TYPE_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_STATIC
 * @access VEHICLE_PROP_ACCESS_READ
 * @data_member info_model_year
 * @unit VEHICLE_UNIT_TYPE_YEAR
 */
#define VEHICLE_PROPERTY_INFO_MODEL_YEAR                            (0x00000103)

/**
 * Fuel capacity of the vehicle
 * @value_type VEHICLE_VALUE_TYPE_FLOAT
 * @change_mode VEHICLE_PROP_CHANGE_MODE_STATIC
 * @access VEHICLE_PROP_ACCESS_READ
 * @data_member info_fuel_capacity
 * @unit VEHICLE_UNIT_TYPE_VEHICLE_UNIT_TYPE_MILLILITER
 */
#define VEHICLE_PROPERTY_INFO_FUEL_CAPACITY                         (0x00000104)


//==== Vehicle Performance Sensors ====

/**
 * Current odometer value of the vehicle
 * @value_type VEHICLE_VALUE_TYPE_FLOAT
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE|VEHICLE_PROP_CHANGE_MODE_CONTINUOUS
 * @access VEHICLE_PROP_ACCESS_READ
 * @data_member odometer
 * @unit VEHICLE_UNIT_TYPE_KILOMETER
 */
#define VEHICLE_PROPERTY_PERF_ODOMETER                              (0x00000204)

/**
 * Speed of the vehicle
 * @value_type VEHICLE_VALUE_TYPE_FLOAT
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE|VEHICLE_PROP_CHANGE_MODE_CONTINUOUS
 * @access VEHICLE_PROP_ACCESS_READ
 * @data_member vehicle_speed
 * @unit VEHICLE_UNIT_TYPE_METER_PER_SEC
 */
#define VEHICLE_PROPERTY_PERF_VEHICLE_SPEED                         (0x00000207)


//==== Engine Sensors ====

/**
 * Temperature of engine coolant
 * @value_type VEHICLE_VALUE_TYPE_FLOAT
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE|VEHICLE_PROP_CHANGE_MODE_CONTINUOUS
 * @access VEHICLE_PROP_ACCESS_READ
 * @data_member engine_coolant_temperature
 * @unit VEHICLE_UNIT_TYPE_CELCIUS
 */
#define VEHICLE_PROPERTY_ENGINE_COOLANT_TEMP                        (0x00000301)

/**
 * Temperature of engine oil
 * @value_type VEHICLE_VALUE_TYPE_FLOAT
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE|VEHICLE_PROP_CHANGE_MODE_CONTINUOUS
 * @access VEHICLE_PROP_ACCESS_READ
 * @data_member engine_oil_temperature
 * @unit VEHICLE_UNIT_TYPE_CELCIUS
 */
#define VEHICLE_PROPERTY_ENGINE_OIL_TEMP                            (0x00000304)
/**
 * Engine rpm
 * @value_type VEHICLE_VALUE_TYPE_FLOAT
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE|VEHICLE_PROP_CHANGE_MODE_CONTINUOUS
 * @access VEHICLE_PROP_ACCESS_READ
 * @data_member engine_rpm
 * @unit VEHICLE_UNIT_TYPE_RPM
 */
#define VEHICLE_PROPERTY_ENGINE_RPM                                 (0x00000305)

//==== Event Sensors ====

/**
 * Currently selected gear
 * @value_type VEHICLE_VALUE_TYPE_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ
 * @data_member gear_selection
 * @data_enum vehicle_gear
 */
#define VEHICLE_PROPERTY_GEAR_SELECTION                             (0x00000400)

/**
 * Current gear. In non-manual case, selected gear does not necessarily match the current gear
 * @value_type VEHICLE_VALUE_TYPE_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ
 * @data_member gear_current_gear
 * @data_enum vehicle_gear
 */
#define VEHICLE_PROPERTY_CURRENT_GEAR                               (0x00000401)

/**
 * Parking brake state.
 * @value_type VEHICLE_VALUE_TYPE_BOOLEAN
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ
 * @data_member parking_brake
 * @data_enum vehicle_boolean
 */
#define VEHICLE_PROPERTY_PARKING_BRAKE_ON                           (0x00000402)

/**
 * Driving status policy.
 * @value_type VEHICLE_VALUE_TYPE_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ
 * @data_member driving_status
 * @data_enum vehicle_driving_status
 */
#define VEHICLE_PROPERTY_DRIVING_STATUS                             (0x00000404)

/**
 * Warning for fuel low level.
 * @value_type VEHICLE_VALUE_TYPE_BOOLEAN
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ
 * @data_member is_fuel_level_low
 * @data_enum vehicle_boolean
 */
#define VEHICLE_PROPERTY_FUEL_LEVEL_LOW                             (0x00000405)

/**
 * Night mode or not.
 * @value_type VEHICLE_VALUE_TYPE_BOOLEAN
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ
 * @data_member night_mode
 * @data_enum vehicle_boolean
 */
#define VEHICLE_PROPERTY_NIGHT_MODE                                 (0x00000407)



//==== HVAC Properties ====

/**
 * Fan speed setting
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @data_member hvac.fan_speed
 * @zone_type VEHICLE_ZONE_TYPE_ZONE
 * @allow_out_of_range_value : OFF
 */
#define VEHICLE_PROPERTY_HVAC_FAN_SPEED                             (0x00000500)

/**
 * Fan direction setting
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @data_member hvac.fan_direction
 * @zone_type VEHICLE_ZONE_TYPE_ZONE
 * @data_enum vehicle_hvac_fan_direction
 * @allow_out_of_range_value : OFF
 */
#define VEHICLE_PROPERTY_HVAC_FAN_DIRECTION                         (0x00000501)

/*
 * Bit flags for fan direction
 */
enum vehicle_hvac_fan_direction {
    VEHICLE_HVAC_FAN_DIRECTION_FACE                 = 0x1,
    VEHICLE_HVAC_FAN_DIRECTION_FLOOR                = 0x2,
    VEHICLE_HVAC_FAN_DIRECTION_FACE_AND_FLOOR       = 0x3,
    VEHICLE_HVAC_FAN_DIRECTION_DEFROST              = 0x4,
    VEHICLE_HVAC_FAN_DIRECTION_DEFROST_AND_FLOOR    = 0x5
};

/**
 * HVAC current temperature.
 * @value_type VEHICLE_VALUE_TYPE_ZONED_FLOAT
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_ZONE
 * @data_member hvac.temperature_current
 */
#define VEHICLE_PROPERTY_HVAC_TEMPERATURE_CURRENT                   (0x00000502)

/**
 * HVAC, target temperature set.
 * @value_type VEHICLE_VALUE_TYPE_ZONED_FLOAT
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_ZONE
 * @data_member hvac.temperature_set
 * @allow_out_of_range_value : MIN / MAX / OFF
 */
#define VEHICLE_PROPERTY_HVAC_TEMPERATURE_SET                       (0x00000503)

/**
 * On/off defrost
 * @value_type VEHICLE_VALUE_TYPE_ZONED_BOOLEAN
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_WINDOW
 * @data_member hvac.defrost_on
 */
#define VEHICLE_PROPERTY_HVAC_DEFROSTER                             (0x00000504)

/**
 * On/off AC
 * @value_type VEHICLE_VALUE_TYPE_ZONED_BOOLEAN
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @config_flags Supported zones
 * @zone_type VEHICLE_ZONE_TYPE_ZONE
 * @data_member hvac.ac_on
 */
#define VEHICLE_PROPERTY_HVAC_AC_ON                                 (0x00000505)

/**
 * On/off max AC
 * @value_type VEHICLE_VALUE_TYPE_ZONED_BOOLEAN
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_ZONE
 * @data_member hvac.max_ac_on
 */
#define VEHICLE_PROPERTY_HVAC_MAX_AC_ON                             (0x00000506)

/**
 * On/off max defrost
 * @value_type VEHICLE_VALUE_TYPE_ZONED_BOOLEAN
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_ZONE
 * @data_member hvac.max_defrost_on
 */
#define VEHICLE_PROPERTY_HVAC_MAX_DEFROST_ON                        (0x00000507)

/**
 * On/off re-circulation
 * @value_type VEHICLE_VALUE_TYPE_ZONED_BOOLEAN
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_ZONE
 * @data_member hvac.max_recirc_on
 */
#define VEHICLE_PROPERTY_HVAC_RECIRC_ON                             (0x00000508)

/**
 * On/off dual. This will be defined per each row.
 * @value_type VEHICLE_VALUE_TYPE_ZONED_BOOLEAN
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_ZONE
 * @data_member hvac.dual_on
 */
#define VEHICLE_PROPERTY_HVAC_DUAL_ON                               (0x00000509)

/**
 * On/off automatic mode
 * @value_type VEHICLE_VALUE_TYPE_ZONED_BOOLEAN
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_ZONE
 * @data_member hvac.auto_on
 */
#define VEHICLE_PROPERTY_HVAC_AUTO_ON                               (0x0000050A)

/**
 * Seat temperature
 *
 * Negative values indicate cooling.
 * 0 indicates off.
 * Positive values indicate heating.
 *
 * Some vehicles may have multiple levels of heating and cooling. The min/max
 * range defines the allowable range and number of steps in each direction.
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_HVAC_SEAT_TEMPERATURE                      (0x0000050B)

/**
 * Side Mirror Heat
 *
 * Increase values denote higher heating levels for side mirrors.
 * 0 indicates heating is turned off.
 *
 * @value_type VEHICLE_VALUE_TYPE_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_HVAC_SIDE_MIRROR_HEAT                      (0x0000050C)

/**
 * Steering Wheel Temperature
 *
 * Sets the temperature for the steering wheel
 * Positive value indicates heating.
 * Negative value indicates cooling.
 * 0 indicates tempreature control is off.
 *
 * @value_type VEHICLE_VALUE_TYPE_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_HVAC_STEERING_WHEEL_TEMP                   (0x0000050D)

/**
 * Temperature units
 *
 * Indicates whether the temperature is in Celsius, Fahrenheit, or a different unit.
 * This parameter affects all HVAC temperatures in the system.
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ
 * @data_enum vehicle_unit_type
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_HVAC_TEMPERATURE_UNITS                     (0x0000050E)

/**
 * Actual fan speed
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ
 * @data_member hvac.fan_speed
 * @zone_type VEHICLE_ZONE_TYPE_ZONE
 * @allow_out_of_range_value : OFF
 */
#define VEHICLE_PROPERTY_HVAC_ACTUAL_FAN_SPEED_RPM                  (0x0000050F)


/**
 * Represents power state for HVAC. Some HVAC properties will require matching power to be turned on
 * to get out of OFF state. For non-zoned HVAC properties, VEHICLE_ALL_ZONE corresponds to
 * global power state.
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_BOOLEAN
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @config_string list of HVAC properties whose power is controlled by this property. Format is
 *                hexa-decimal number (0x...) separated by comma like "0x500,0x503". All zones
 *                defined in these affected properties should be available in the property.
 * @zone_type VEHICLE_ZONE_TYPE_ZONE
 * @data_member hvac.power_on
 */
#define VEHICLE_PROPERTY_HVAC_POWER_ON                              (0x00000510)

/**
 * Fan Positions Available
 *
 * This is a bit mask of fan positions available for the zone.  Each entry in
 * vehicle_hvac_fan_direction is selected by bit position.  For instance, if
 * only the FAN_DIRECTION_FACE (0x1) and FAN_DIRECTION_DEFROST (0x4) are available,
 * then this value shall be set to 0x12.
 *
 * 0x12 = (1 << 1) | (1 << 4)
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_STATIC
 * @access VEHICLE_PROP_ACCESS_READ
 * @data_member int32_value
 * @zone_type VEHICLE_ZONE_TYPE_ZONE
 * @allow_out_of_range_value : OFF
 */
#define VEHICLE_PROPERTY_HVAC_FAN_DIRECTION_AVAILABLE               (0x00000511)

/**
 * Outside temperature
 * @value_type VEHICLE_VALUE_TYPE_FLOAT
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE|VEHICLE_PROP_CHANGE_MODE_CONTINUOUS
 * @access VEHICLE_PROP_ACCESS_READ
 * @data_member outside_temperature
 * @unit VEHICLE_UNIT_TYPE_CELCIUS
 */
#define VEHICLE_PROPERTY_ENV_OUTSIDE_TEMPERATURE                    (0x00000703)


/**
 * Cabin temperature
 * @value_type VEHICLE_VALUE_TYPE_FLOAT
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE|VEHICLE_PROP_CHANGE_MODE_CONTINUOUS
 * @access VEHICLE_PROP_ACCESS_READ
 * @data_member cabin_temperature
 * @unit VEHICLE_UNIT_TYPE_CELCIUS
 */
#define VEHICLE_PROPERTY_ENV_CABIN_TEMPERATURE                      (0x00000704)


/*
 * Radio features.
 */
/**
 * Radio presets stored on the Car radio module. The data type used is int32
 * array with the following fields:
 * <ul>
 *    <li> int32_array[0]: Preset number </li>
 *    <li> int32_array[1]: Band type (see #RADIO_BAND_FM in
 *    system/core/include/system/radio.h).
 *    <li> int32_array[2]: Channel number </li>
 *    <li> int32_array[3]: Sub channel number </li>
 * </ul>
 *
 * NOTE: When getting a current preset config ONLY set preset number (i.e.
 * int32_array[0]). For setting a preset other fields are required.
 *
 * @value_type VEHICLE_VALUE_TYPE_INT32_VEC4
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @config_flags Number of presets supported
 * @data_member int32_array
 */
#define VEHICLE_PROPERTY_RADIO_PRESET                               (0x00000801)

/**
 * Constants relevant to radio.
 */
enum vehicle_radio_consts {
    /** Minimum value for the radio preset */
    VEHICLE_RADIO_PRESET_MIN_VALUE = 1,
};

/**
 * Property to control power state of application processor.
 *
 * It is assumed that AP's power state is controller by separate power controller.
 *
 * For configuration information, vehicle_prop_config.config_flags can have bit flag combining
 * values in vehicle_ap_power_state_config_type.
 *
 * For get / notification, data type looks like this:
 *   int32_array[0] : vehicle_ap_power_state_type
 *   int32_array[1] : additional parameter relevant for each state. should be 0 if not used.
 * For set, data type looks like this:
 *   int32_array[0] : vehicle_ap_power_state_set_type
 *   int32_array[1] : additional parameter relevant for each request. should be 0 if not used.
 *
 * @value_type VEHICLE_VALUE_TYPE_INT32_VEC2
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @config_flags Additional info on power state. Should use vehicle_ap_power_state_config_flag.
 * @data_member int32_array
 */
#define VEHICLE_PROPERTY_AP_POWER_STATE                             (0x00000A00)

enum vehicle_ap_power_state_config_flag {
    /**
     * AP can enter deep sleep state. If not set, AP will always shutdown from
     * VEHICLE_AP_POWER_STATE_SHUTDOWN_PREPARE power state.
     */
    VEHICLE_AP_POWER_STATE_CONFIG_ENABLE_DEEP_SLEEP_FLAG = 0x1,

    /**
     * The power controller can power on AP from off state after timeout specified in
     * VEHICLE_AP_POWER_SET_SHUTDOWN_READY message.
     */
    VEHICLE_AP_POWER_STATE_CONFIG_SUPPORT_TIMER_POWER_ON_FLAG = 0x2,
};

enum vehicle_ap_power_state {
    /** vehicle HAL will never publish this state to AP */
    VEHICLE_AP_POWER_STATE_OFF = 0,
    /** vehicle HAL will never publish this state to AP */
    VEHICLE_AP_POWER_STATE_DEEP_SLEEP = 1,
    /** AP is on but display should be off. */
    VEHICLE_AP_POWER_STATE_ON_DISP_OFF = 2,
    /** AP is on with display on. This state allows full user interaction. */
    VEHICLE_AP_POWER_STATE_ON_FULL = 3,
    /**
     * The power controller has requested AP to shutdown. AP can either enter sleep state or start
     * full shutdown. AP can also request postponing shutdown by sending
     * VEHICLE_AP_POWER_SET_SHUTDOWN_POSTPONE message. The power controller should change power
     * state to this state to shutdown system.
     *
     * int32_array[1] : one of enum_vehicle_ap_power_state_shutdown_param_type
     */
    VEHICLE_AP_POWER_STATE_SHUTDOWN_PREPARE = 4,
};

enum vehicle_ap_power_state_shutdown_param {
    /** AP should shutdown immediately. Postponing is not allowed. */
    VEHICLE_AP_POWER_SHUTDOWN_PARAM_SHUTDOWN_IMMEDIATELY = 1,
    /** AP can enter deep sleep instead of shutting down completely. */
    VEHICLE_AP_POWER_SHUTDOWN_PARAM_CAN_SLEEP = 2,
    /** AP can only shutdown with postponing allowed. */
    VEHICLE_AP_POWER_SHUTDOWN_PARAM_SHUTDOWN_ONLY = 3,
};

enum vehicle_ap_power_set_state {
    /**
     * AP has finished boot up, and can start shutdown if requested by power controller.
     */
    VEHICLE_AP_POWER_SET_BOOT_COMPLETE = 0x1,
    /**
     * AP is entering deep sleep state. How this state is implemented may vary depending on
     * each H/W, but AP's power should be kept in this state.
     */
    VEHICLE_AP_POWER_SET_DEEP_SLEEP_ENTRY = 0x2,
    /**
     * AP is exiting from deep sleep state, and is in VEHICLE_AP_POWER_STATE_SHUTDOWN_PREPARE state.
     * The power controller may change state to other ON states based on the current state.
     */
    VEHICLE_AP_POWER_SET_DEEP_SLEEP_EXIT = 0x3,
    /**
     * int32_array[1]: Time to postpone shutdown in ms. Maximum value can be 5000 ms.
     *                 If AP needs more time, it will send another POSTPONE message before
     *                 the previous one expires.
     */
    VEHICLE_AP_POWER_SET_SHUTDOWN_POSTPONE = 0x4,
    /**
     * AP is starting shutting down. When system completes shutdown, everything will stop in AP
     * as kernel will stop all other contexts. It is responsibility of vehicle HAL or lower level
     * to synchronize that state with external power controller. As an example, some kind of ping
     * with timeout in power controller can be a solution.
     *
     * int32_array[1]: Time to turn on AP in secs. Power controller may turn on AP after specified
     *                 time so that AP can run tasks like update. If it is set to 0, there is no
     *                 wake up, and power controller may not necessarily support wake-up.
     *                 If power controller turns on AP due to timer, it should start with
     *                 VEHICLE_AP_POWER_STATE_ON_DISP_OFF state, and after receiving
     *                 VEHICLE_AP_POWER_SET_BOOT_COMPLETE, it shall do state transition to
     *                 VEHICLE_AP_POWER_STATE_SHUTDOWN_PREPARE.
     */
    VEHICLE_AP_POWER_SET_SHUTDOWN_START = 0x5,
    /**
     * User has requested to turn off headunit's display, which is detected in android side.
     * The power controller may change the power state to VEHICLE_AP_POWER_STATE_ON_DISP_OFF.
     */
    VEHICLE_AP_POWER_SET_DISPLAY_OFF = 0x6,
    /**
     * User has requested to turn on headunit's display, most probably from power key input which
     * is attached to headunit. The power controller may change the power state to
     * VEHICLE_AP_POWER_STATE_ON_FULL.
     */
    VEHICLE_AP_POWER_SET_DISPLAY_ON = 0x7,
};

/**
 * Property to represent brightness of the display. Some cars have single control for
 * the brightness of all displays and this property is to share change in that control.
 *
 * If this is writable, android side can set this value when user changes display brightness
 * from Settings. If this is read only, user may still change display brightness from Settings,
 * but that will not be reflected to other displays.
 *
 * @value_type VEHICLE_VALUE_TYPE_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ|VEHICLE_PROP_ACCESS_READ_WRITE
 * @data_member int32
 */
#define VEHICLE_PROPERTY_DISPLAY_BRIGHTNESS                         (0x00000A01)


/**
 * Index in int32_array for VEHICLE_PROPERTY_AP_POWER_STATE property.
 */
enum vehicle_ap_power_state_index {
    VEHICLE_AP_POWER_STATE_INDEX_STATE = 0,
    VEHICLE_AP_POWER_STATE_INDEX_ADDITIONAL = 1,
};

/**
* Property to report bootup reason for the current power on. This is a static property that will
* not change for the whole duration until power off. For example, even if user presses power on
* button after automatic power on with door unlock, bootup reason should stay with
* VEHICLE_AP_POWER_BOOTUP_REASON_USER_UNLOCK.
*
* int32_value should be vehicle_ap_power_bootup_reason.
*
* @value_type VEHICLE_VALUE_TYPE_INT32
* @change_mode VEHICLE_PROP_CHANGE_MODE_STATIC
* @access VEHICLE_PROP_ACCESS_READ
* @data_member int32_value
*/
#define VEHICLE_PROPERTY_AP_POWER_BOOTUP_REASON                     (0x00000A02)

/**
 * Enum to represent bootup reason.
 */
enum vehicle_ap_power_bootup_reason {
    /**
     * Power on due to user's pressing of power key or rotating of ignition switch.
     */
    VEHICLE_AP_POWER_BOOTUP_REASON_USER_POWER_ON = 0,
    /**
     * Automatic power on triggered by door unlock or any other kind of automatic user detection.
     */
    VEHICLE_AP_POWER_BOOTUP_REASON_USER_UNLOCK   = 1,
    /**
     * Automatic power on triggered by timer. This only happens when AP has asked wake-up after
     * certain time through time specified in VEHICLE_AP_POWER_SET_SHUTDOWN_START.
     */
    VEHICLE_AP_POWER_BOOTUP_REASON_TIMER         = 2,
};


/**
 * Property to feed H/W input events to android
 *
 * int32_array[0] : action defined by vehicle_hw_key_input_action
 * int32_array[1] : key code, should use standard android key code
 * int32_array[2] : target display defined in vehicle_display. Events not tied
 *                  to specific display should be sent to DISPLAY_MAIN.
 * int32_array[3] : reserved for now. should be zero
 * @value_type VEHICLE_VALUE_TYPE_INT32_VEC4
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ
 * @config_flags
 * @data_member int32_array
 */
#define VEHICLE_PROPERTY_HW_KEY_INPUT                               (0x00000A10)

enum vehicle_hw_key_input_action {
    /** Key down */
    VEHICLE_HW_KEY_INPUT_ACTION_DOWN = 0,
    /** Key up */
    VEHICLE_HW_KEY_INPUT_ACTION_UP = 1,
};

enum vehicle_display {
    /** center console */
    VEHICLE_DISPLAY_MAIN               = 0,
    VEHICLE_DISPLAY_INSTRUMENT_CLUSTER = 1,
};

/**
 * Property to define instrument cluster information.
 * For CLUSTER_TYPE_EXTERNAL_DISPLAY:
 *  READ:
 *   int32_array[0] : The current screen mode index. Screen mode is defined
 *                     as a configuration in car service and represents which
 *                     area of screen is renderable.
 *   int32_array[1] : Android can render to instrument cluster (=1) or not(=0). When this is 0,
 *                    instrument cluster may be rendering some information in the area
 *                    allocated for android and android side rendering is invisible. *
 *   int32_array[2..3] : should be zero
 *  WRITE from android:
 *   int32_array[0] : Preferred mode for android side. Depending on the app rendering to instrument
 *                    cluster, preferred mode can change. Instrument cluster still needs to send
 *                    event with new mode to trigger actual mode change.
 *   int32_array[1] : The current app context relevant for instrument cluster. Use the same flag
 *                    with vehicle_audio_context_flag but this context represents active apps, not
 *                    active audio. Instrument cluster side may change mode depending on the
 *                    currently active contexts.
 *   int32_array[2..3] : should be zero
 *  When system boots up, Android side will write {0, 0, 0, 0} when it is ready to render to
 *  instrument cluster. Before this message, rendering from android should not be visible in the
 *  cluster.
 * @value_type VEHICLE_VALUE_TYPE_INT32_VEC4
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @config_array 0:vehicle_instument_cluster_type 1:hw type
 * @data_member int32_array
 */
#define VEHICLE_PROPERTY_INSTRUMENT_CLUSTER_INFO                    (0x00000A20)

/**
 * Represents instrument cluster type available in system
 */
enum vehicle_instument_cluster_type {
    /** Android has no access to instument cluster */
    VEHICLE_INSTRUMENT_CLUSTER_TYPE_NONE = 0,
    /**
     * Instrument cluster can communicate through vehicle hal with additional
     * properties to exchange meta-data
     */
    VEHICLE_INSTRUMENT_CLUSTER_TYPE_HAL_INTERFACE = 1,
    /**
     * Instrument cluster is external display where android can render contents
     */
    VEHICLE_INSTRUMENT_CLUSTER_TYPE_EXTERNAL_DISPLAY = 2,
};

/**
 * Current date and time, encoded as Unix time.
 * This value denotes the number of seconds that have elapsed since 1/1/1970.
 *
 * @value_type VEHICLE_VALUE_TYPE_INT64
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_SET
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @data_member int64_value
 * @unit VEHICLE_UNIT_TYPE_SECS
 */
#define VEHICLE_PROPERTY_UNIX_TIME                                  (0x00000A30)

/**
 * Current time only.
 * Some vehicles may not keep track of date.  This property only affects the current time, in
 * seconds during the day.  Thus, the max value for this parameter is 86,400 (24 * 60 * 60)
 *
 * @value_type VEHICLE_VALUE_TYPE_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_SET
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @data_member int32_value
 * @unit VEHICLE_UNIT_TYPE_SECS
 */
#define VEHICLE_PROPERTY_CURRENT_TIME_IN_SECONDS                    (0x00000A31)


//==== Car Cabin Properties ====
/**
 * Most Car Cabin properties have both a MOVE and POSITION parameter associated with them.
 *
 * The MOVE parameter will start moving the device in the indicated direction.  The magnitude
 * indicates the relative speed.  For instance, setting the WINDOW_MOVE parameter to +1 will roll
 * the window up.  Setting it to +2 (if available) will roll it up faster.
 *
 * The POSITION parameter will move the device to the desired position.  For instance, if the
 * WINDOW_POS has a range of 0-100, then setting this parameter to 50 will open the window halfway.
 * Depending upon the initial position, the window may move up or down to the 50% value.
 *
 * OEMs may choose to implement one or both of the MOVE/POSITION parameters depending upon the
 * capability of the hardware.
 */

// Doors
/**
 * Door position
 *
 * This is an integer in case a door may be set to a particular position.  Max
 * value indicates fully open, min value (0) indicates fully closed.
 *
 * Some vehicles (minivans) can open the door electronically.  Hence, the ability
 * to write this property.
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ|VEHICLE_PROP_ACCESS_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_DOOR
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_DOOR_POS                                   (0x00000B00)

/**
 * Door move
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE|VEHICLE_PROP_ACCESS_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_DOOR
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_DOOR_MOVE                                  (0x00000B01)


/**
 * Door lock
 *
 * 'true' indicates door is locked
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_BOOLEAN
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_DOOR
 * @data_member boolean_value
 */
#define VEHICLE_PROPERTY_DOOR_LOCK                                  (0x00000B02)

// Mirrors
/**
 * Mirror Z Position
 *
 * Positive value indicates tilt upwards, negative value is downwards
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_MIRROR
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_MIRROR_Z_POS                               (0x00000B40)

/**
 * Mirror Z Move
 *
 * Positive value indicates tilt upwards, negative value is downwards
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE|VEHICLE_PROP_ACCESS_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_MIRROR
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_MIRROR_Z_MOVE                              (0x00000B41)

/**
 * Mirror Y Position
 *
 * Positive value indicate tilt right, negative value is left
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_MIRROR
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_MIRROR_Y_POS                               (0x00000B42)

/**
 * Mirror Y Move
 *
 * Positive value indicate tilt right, negative value is left
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE|VEHICLE_PROP_ACCESS_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_MIRROR
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_MIRROR_Y_MOVE                              (0x00000B43)

/**
 * Mirror Lock
 *
 * True indicates mirror positions are locked and not changeable
 *
 * @value_type VEHICLE_VALUE_TYPE_BOOLEAN
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @data_member boolean_value
 */
#define VEHICLE_PROPERTY_MIRROR_LOCK                                (0x00000B44)

/**
 * Mirror Fold
 *
 * True indicates mirrors are folded
 *
 * @value_type VEHICLE_VALUE_TYPE_BOOLEAN
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @data_member boolean_value
 */
#define VEHICLE_PROPERTY_MIRROR_FOLD                                (0x00000B45)

// Seats
/**
 * Seat memory select
 *
 * This parameter selects the memory preset to use to select the seat position.
 * The minValue is always 0, and the maxValue determines the number of seat
 * positions available.
 *
 * For instance, if the driver's seat has 3 memory presets, the maxValue will be 3.
 * When the user wants to select a preset, the desired preset number (1, 2, or 3)
 * is set.
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_SEAT_MEMORY_SELECT                         (0x00000B80)

/**
 * Seat memory set
 *
 * This setting allows the user to save the current seat position settings into
 * the selected preset slot.  The maxValue for each seat position shall match
 * the maxValue for VEHICLE_PROPERTY_SEAT_MEMORY_SELECT.
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_SEAT_MEMORY_SET                            (0x00000B81)

/**
 * Seatbelt buckled
 *
 * True indicates belt is buckled.
 *
 * Write access indicates automatic seat buckling capabilities.  There are no known cars at this
 * time, but you never know...
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_BOOLEAN
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ|VEHICLE_PROP_ACCESS_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member boolean_value
 */
#define VEHICLE_PROPERTY_SEAT_BELT_BUCKLED                          (0x00000B82)

/**
 * Seatbelt height position
 *
 * Adjusts the shoulder belt anchor point.
 * Max value indicates highest position
 * Min value indicates lowest position
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_SEAT_BELT_HEIGHT_POS                       (0x00000B83)

/**
 * Seatbelt height move
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE|VEHICLE_PROP_ACCESS_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_SEAT_BELT_HEIGHT_MOVE                      (0x00000B84)

/**
 * Seat fore/aft position
 *
 * Sets the seat position forward (closer to steering wheel) and backwards.
 * Max value indicates closest to wheel, min value indicates most rearward
 * position.
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_SEAT_FORE_AFT_POS                          (0x00000B85)

/**
 * Seat fore/aft move
 *
 * Moves the seat position forward and aft.
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE|VEHICLE_PROP_ACCESS_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_SEAT_FORE_AFT_MOVE                         (0x00000B86)

/**
 * Seat backrest angle 1 position
 *
 * Backrest angle 1 is the actuator closest to the bottom of the seat.
 * Max value indicates angling forward towards the steering wheel.
 * Min value indicates full recline.
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_SEAT_BACKREST_ANGLE_1_POS                  (0x00000B87)

/**
 * Seat backrest angle 1 move
 *
 * Moves the backrest forward or recline.
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE|VEHICLE_PROP_ACCESS_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_SEAT_BACKREST_ANGLE_1_MOVE                 (0x00000B88)

/**
 * Seat backrest angle 2 position
 *
 * Backrest angle 2 is the next actuator up from the bottom of the seat.
 * Max value indicates angling forward towards the steering wheel.
 * Min value indicates full recline.
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_SEAT_BACKREST_ANGLE_2_POS                  (0x00000B89)

/**
 * Seat backrest angle 2 move
 *
 * Moves the backrest forward or recline.
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE|VEHICLE_PROP_ACCESS_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_SEAT_BACKREST_ANGLE_2_MOVE                 (0x00000B8A)

/**
 * Seat height position
 *
 * Sets the seat height.
 * Max value indicates highest position.
 * Min value indicates lowest position.
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_SEAT_HEIGHT_POS                            (0x00000B8B)

/**
 * Seat height move
 *
 * Moves the seat height.
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE|VEHICLE_PROP_ACCESS_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_SEAT_HEIGHT_MOVE                           (0x00000B8C)

/**
 * Seat depth position
 *
 * Sets the seat depth, distance from back rest to front edge of seat.
 * Max value indicates longest depth position.
 * Min value indicates shortest position.
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_SEAT_DEPTH_POS                             (0x00000B8D)

/**
 * Seat depth move
 *
 * Adjusts the seat depth.
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE|VEHICLE_PROP_ACCESS_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_SEAT_DEPTH_MOVE                            (0x00000B8E)

/**
 * Seat tilt position
 *
 * Sets the seat tilt.
 * Max value indicates front edge of seat higher than back edge.
 * Min value indicates front edge of seat lower than back edge.
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_SEAT_TILT_POS                              (0x00000B8F)

/**
 * Seat tilt move
 *
 * Tilts the seat.
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE|VEHICLE_PROP_ACCESS_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_SEAT_TILT_MOVE                             (0x00000B90)

/**
 * Lumber fore/aft position
 *
 * Pushes the lumbar support forward and backwards
 * Max value indicates most forward position.
 * Min value indicates most rearward position.
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_SEAT_LUMBAR_FORE_AFT_POS                   (0x00000B91)

/**
 * Lumbar fore/aft move
 *
 * Adjusts the lumbar support.
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE|VEHICLE_PROP_ACCESS_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_SEAT_LUMBAR_FORE_AFT_MOVE                  (0x00000B92)

/**
 * Lumbar side support position
 *
 * Sets the amount of lateral lumbar support.
 * Max value indicates widest lumbar setting (i.e. least support)
 * Min value indicates thinnest lumbar setting.
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_SEAT_LUMBAR_SIDE_SUPPORT_POS               (0x00000B93)

/**
 * Lumbar side support move
 *
 * Adjusts the amount of lateral lumbar support.
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE|VEHICLE_PROP_ACCESS_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_SEAT_LUMBAR_SIDE_SUPPORT_MOVE              (0x00000B94)

/**
 * Headrest height position
 *
 * Sets the headrest height.
 * Max value indicates tallest setting.
 * Min value indicates shortest setting.
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_SEAT_HEADREST_HEIGHT_POS                   (0x00000B95)

/**
 * Headrest height move
 *
 * Moves the headrest up and down.
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE|VEHICLE_PROP_ACCESS_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_SEAT_HEADREST_HEIGHT_MOVE                  (0x00000B96)

/**
 * Headrest angle position
 *
 * Sets the angle of the headrest.
 * Max value indicates most upright angle.
 * Min value indicates shallowest headrest angle.
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_SEAT_HEADREST_ANGLE_POS                    (0x00000B97)

/**
 * Headrest angle move
 *
 * Adjusts the angle of the headrest
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE|VEHICLE_PROP_ACCESS_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_SEAT_HEADREST_ANGLE_MOVE                   (0x00000B98)

/**
 * Headrest fore/aft position
 *
 * Adjusts the headrest forwards and backwards.
 * Max value indicates position closest to front of car.
 * Min value indicates position closest to rear of car.
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_SEAT_HEADREST_FORE_AFT_POS                 (0x00000B99)

/**
 * Headrest fore/aft move
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE|VEHICLE_PROP_ACCESS_WRITE
 * @zone_type VEHICLE_ZONE_TYPE_SEAT
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_SEAT_HEADREST_FORE_AFT_MOVE                (0x00000B9A)


// Windows
/**
 * Window Position
 *
 * Max = window up / closed
 * Min = window down / open
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_WINDOW_POS                                 (0x00000BC0)

/**
 * Window Move
 *
 * Max = window up / closed
 * Min = window down / open
 * Magnitude denotes relative speed.  I.e. +2 is faster than +1 in raising the window.
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE|VEHICLE_PROP_ACCESS_WRITE
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_WINDOW_MOVE                                (0x00000BC1)

/**
 * Window Vent Position
 *
 * This feature is used to control the vent feature on a sunroof.
 *
 * Max = vent open
 * Min = vent closed
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_WINDOW_VENT_POS                            (0x00000BC2)

/**
 * Window Vent Move
 *
 * This feature is used to control the vent feature on a sunroof.
 *
 * Max = vent open
 * Min = vent closed
 *
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE|VEHICLE_PROP_ACCESS_WRITE
 * @data_member int32_value
 */
#define VEHICLE_PROPERTY_WINDOW_VENT_MOVE                           (0x00000BC3)

/**
 * Window Lock
 *
 * True indicates windows are locked and can't be moved.
 *
 * @value_type VEHICLE_VALUE_TYPE_BOOLEAN
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @data_member boolean_value
 */
#define VEHICLE_PROPERTY_WINDOW_LOCK                                (0x00000BC4)



/**
 *  H/W specific, non-standard property can be added as necessary. Such property should use
 *  property number in range of [VEHICLE_PROPERTY_CUSTOM_START, VEHICLE_PROPERTY_CUSTOM_END].
 *  Definition of property in this range is completely up to each HAL implementation.
 *  For such property, it is recommended to fill vehicle_prop_config.config_string with some
 *  additional information to help debugging. For example, company XYZ's custom extension may
 *  include config_string of "com.XYZ.some_further_details".
 *  @range_start
 */
#define VEHICLE_PROPERTY_CUSTOM_START                               (0x70000000)
/** @range_end */
#define VEHICLE_PROPERTY_CUSTOM_END                                 (0x73ffffff)

/**
 * Property range allocated for system's internal usage like testing. HAL should never declare
 * property in this range.
 * @range_start
 */
#define VEHICLE_PROPERTY_INTERNAL_START                             (0x74000000)
/**
 * @range_end
 */
#define VEHICLE_PROPERTY_INTERNAL_END                               (0x74ffffff)

/**
 * Value types for various properties.
 */
enum vehicle_value_type {
    VEHICLE_VALUE_TYPE_SHOUD_NOT_USE            = 0x00, // value_type should never set to 0.
    VEHICLE_VALUE_TYPE_STRING                   = 0x01,
    VEHICLE_VALUE_TYPE_BYTES                    = 0x02,
    VEHICLE_VALUE_TYPE_BOOLEAN                  = 0x03,
    VEHICLE_VALUE_TYPE_ZONED_BOOLEAN            = 0x04,
    VEHICLE_VALUE_TYPE_INT64                    = 0x05,
    VEHICLE_VALUE_TYPE_FLOAT                    = 0x10,
    VEHICLE_VALUE_TYPE_FLOAT_VEC2               = 0x11,
    VEHICLE_VALUE_TYPE_FLOAT_VEC3               = 0x12,
    VEHICLE_VALUE_TYPE_FLOAT_VEC4               = 0x13,
    VEHICLE_VALUE_TYPE_INT32                    = 0x20,
    VEHICLE_VALUE_TYPE_INT32_VEC2               = 0x21,
    VEHICLE_VALUE_TYPE_INT32_VEC3               = 0x22,
    VEHICLE_VALUE_TYPE_INT32_VEC4               = 0x23,
    VEHICLE_VALUE_TYPE_ZONED_FLOAT              = 0x30,
    VEHICLE_VALUE_TYPE_ZONED_FLOAT_VEC2         = 0x31,
    VEHICLE_VALUE_TYPE_ZONED_FLOAT_VEC3         = 0x32,
    VEHICLE_VALUE_TYPE_ZONED_FLOAT_VEC4         = 0x33,
    VEHICLE_VALUE_TYPE_ZONED_INT32              = 0x40,
    VEHICLE_VALUE_TYPE_ZONED_INT32_VEC2         = 0x41,
    VEHICLE_VALUE_TYPE_ZONED_INT32_VEC3         = 0x42,
    VEHICLE_VALUE_TYPE_ZONED_INT32_VEC4         = 0x43,
};

/**
 * Units used for int or float type with no attached enum types.
 */
enum vehicle_unit_type {
    VEHICLE_UNIT_TYPE_SHOULD_NOT_USE        = 0x00000000,
    // speed related items
    VEHICLE_UNIT_TYPE_METER_PER_SEC         = 0x00000001,
    VEHICLE_UNIT_TYPE_RPM                   = 0x00000002,
    VEHICLE_UNIT_TYPE_HZ                    = 0x00000003,
    // kind of ratio
    VEHICLE_UNIT_TYPE_PERCENTILE            = 0x00000010,
    // length
    VEHICLE_UNIT_TYPE_MILLIMETER            = 0x00000020,
    VEHICLE_UNIT_TYPE_METER                 = 0x00000021,
    VEHICLE_UNIT_TYPE_KILOMETER             = 0x00000023,
    // temperature
    VEHICLE_UNIT_TYPE_CELSIUS               = 0x00000030,
    VEHICLE_UNIT_TYPE_FAHRENHEIT            = 0x00000031,
    VEHICLE_UNIT_TYPE_KELVIN                = 0x00000032,
    // volume
    VEHICLE_UNIT_TYPE_MILLILITER            = 0x00000040,
    // time
    VEHICLE_UNIT_TYPE_NANO_SECS             = 0x00000050,
    VEHICLE_UNIT_TYPE_SECS                  = 0x00000053,
    VEHICLE_UNIT_TYPE_YEAR                  = 0x00000059,
};

/**
 * This describes how value of property can change.
 */
enum vehicle_prop_change_mode {
    /**
     * Property of this type will *never* change. This property will not support subscription, but
     * will support get
     */
    VEHICLE_PROP_CHANGE_MODE_STATIC         = 0x00,
    /**
     * Property of this type will be reported when there is a change.
     * get call should return the current value.
     * Set operation for this property is assumed to be asynchronous. When the property is read
     * (get) after set, it may still return old value until underlying H/W backing this property
     * has actually changed the state. Once state is changed, the property will dispatch changed
     * value as event.
     */
    VEHICLE_PROP_CHANGE_MODE_ON_CHANGE      = 0x01,
    /**
     * Property of this type change continuously and requires fixed rate of sampling to retrieve
     * the data.
     */
    VEHICLE_PROP_CHANGE_MODE_CONTINUOUS     = 0x02,
    /**
     * Property of this type may be polled to get the current value.
     */
    VEHICLE_PROP_CHANGE_MODE_POLL           = 0x03,
    /**
     * This is for property where change event should be sent only when the value is
     * set from external component. Normal value change will not trigger event.
     * For example, clock property can send change event only when it is set, outside android,
     * for case like user setting time or time getting update. There is no need to send it
     * per every value change.
     */
    VEHICLE_PROP_CHANGE_MODE_ON_SET         = 0x04,
};

/**
 * Property config defines the capabilities of it. User of the API
 * should first get the property config to understand the output from get()
 * commands and also to ensure that set() or events commands are in sync with
 * the expected output.
 */
enum vehicle_prop_access {
    VEHICLE_PROP_ACCESS_READ  = 0x01,
    VEHICLE_PROP_ACCESS_WRITE = 0x02,
    VEHICLE_PROP_ACCESS_READ_WRITE = 0x03
};

/**
 * These permissions define how the OEMs want to distribute their information and security they
 * want to apply. On top of these restrictions, android will have additional
 * 'app-level' permissions that the apps will need to ask the user before the apps have the
 * information.
 * This information should be kept in vehicle_prop_config.permission_model.
 */
enum vehicle_permission_model {
    /**
     * No special restriction, but each property can still require specific android app-level
     * permission.
     */
    VEHICLE_PERMISSION_NO_RESTRICTION = 0,
    /** Signature only. Only APKs signed with OEM keys are allowed. */
    VEHICLE_PERMISSION_OEM_ONLY = 0x1,
    /** System only. APKs built-in to system  can access the property. */
    VEHICLE_PERMISSION_SYSTEM_APP_ONLY = 0x2,
    /** Equivalent to “system|signature” */
    VEHICLE_PERMISSION_OEM_OR_SYSTEM_APP = 0x3
};


/**
 *  Special values for INT32/FLOAT (including ZONED types)
 *  These values represent special state, which is outside MIN/MAX range but can happen.
 *  For example, HVAC temperature may use out of range min / max to represent that
 *  it is working in full power although target temperature has separate min / max.
 *  OUT_OF_RANGE_OFF can represent a state where the property is powered off.
 *  Usually such property will have separate property to control power.
 */

#define VEHICLE_INT_OUT_OF_RANGE_MAX (INT32_MAX)
#define VEHICLE_INT_OUT_OF_RANGE_MIN (INT32_MIN)
#define VEHICLE_INT_OUT_OF_RANGE_OFF (INT32_MIN + 1)

#define VEHICLE_FLOAT_OUT_OF_RANGE_MAX (INFINITY)
#define VEHICLE_FLOAT_OUT_OF_RANGE_MIN (-INFINITY)
#define VEHICLE_FLOAT_OUT_OF_RANGE_OFF (NAN)

/**
 * Car states.
 *
 * The driving states determine what features of the UI will be accessible.
 */
enum vehicle_driving_status {
    VEHICLE_DRIVING_STATUS_UNRESTRICTED      = 0x00,
    VEHICLE_DRIVING_STATUS_NO_VIDEO          = 0x01,
    VEHICLE_DRIVING_STATUS_NO_KEYBOARD_INPUT = 0x02,
    VEHICLE_DRIVING_STATUS_NO_VOICE_INPUT    = 0x04,
    VEHICLE_DRIVING_STATUS_NO_CONFIG         = 0x08,
    VEHICLE_DRIVING_STATUS_LIMIT_MESSAGE_LEN = 0x10
};

/**
 * Various gears which can be selected by user and chosen in system.
 */
enum vehicle_gear {
    // Gear selections present in both automatic and manual cars.
    VEHICLE_GEAR_NEUTRAL    = 0x0001,
    VEHICLE_GEAR_REVERSE    = 0x0002,

    // Gear selections (mostly) present only in automatic cars.
    VEHICLE_GEAR_PARK       = 0x0004,
    VEHICLE_GEAR_DRIVE      = 0x0008,
    VEHICLE_GEAR_LOW        = 0x0010,

    // Other possible gear selections (maybe present in manual or automatic
    // cars).
    VEHICLE_GEAR_1          = 0x0010,
    VEHICLE_GEAR_2          = 0x0020,
    VEHICLE_GEAR_3          = 0x0040,
    VEHICLE_GEAR_4          = 0x0080,
    VEHICLE_GEAR_5          = 0x0100,
    VEHICLE_GEAR_6          = 0x0200,
    VEHICLE_GEAR_7          = 0x0400,
    VEHICLE_GEAR_8          = 0x0800,
    VEHICLE_GEAR_9          = 0x1000
};


/**
 * Various zones in the car.
 *
 * Zones are used for Air Conditioning purposes and divide the car into physical
 * area zones.
 */
enum vehicle_zone {
    VEHICLE_ZONE_ROW_1_LEFT    = 0x00000001,
    VEHICLE_ZONE_ROW_1_CENTER  = 0x00000002,
    VEHICLE_ZONE_ROW_1_RIGHT   = 0x00000004,
    VEHICLE_ZONE_ROW_1_ALL     = 0x00000008,
    VEHICLE_ZONE_ROW_2_LEFT    = 0x00000010,
    VEHICLE_ZONE_ROW_2_CENTER  = 0x00000020,
    VEHICLE_ZONE_ROW_2_RIGHT   = 0x00000040,
    VEHICLE_ZONE_ROW_2_ALL     = 0x00000080,
    VEHICLE_ZONE_ROW_3_LEFT    = 0x00000100,
    VEHICLE_ZONE_ROW_3_CENTER  = 0x00000200,
    VEHICLE_ZONE_ROW_3_RIGHT   = 0x00000400,
    VEHICLE_ZONE_ROW_3_ALL     = 0x00000800,
    VEHICLE_ZONE_ROW_4_LEFT    = 0x00001000,
    VEHICLE_ZONE_ROW_4_CENTER  = 0x00002000,
    VEHICLE_ZONE_ROW_4_RIGHT   = 0x00004000,
    VEHICLE_ZONE_ROW_4_ALL     = 0x00008000,
    VEHICLE_ZONE_ALL           = 0x80000000,
};

/**
 * Various Seats in the car.
 */
enum vehicle_seat {
    VEHICLE_SEAT_ROW_1_LEFT   = 0x0001,
    VEHICLE_SEAT_ROW_1_CENTER = 0x0002,
    VEHICLE_SEAT_ROW_1_RIGHT  = 0x0004,
    VEHICLE_SEAT_ROW_2_LEFT   = 0x0010,
    VEHICLE_SEAT_ROW_2_CENTER = 0x0020,
    VEHICLE_SEAT_ROW_2_RIGHT  = 0x0040,
    VEHICLE_SEAT_ROW_3_LEFT   = 0x0100,
    VEHICLE_SEAT_ROW_3_CENTER = 0x0200,
    VEHICLE_SEAT_ROW_3_RIGHT  = 0x0400
};

/**
 * Various windshields/windows in the car.
 */
enum vehicle_window {
    VEHICLE_WINDOW_FRONT_WINDSHIELD = 0x0001,
    VEHICLE_WINDOW_REAR_WINDSHIELD  = 0x0002,
    VEHICLE_WINDOW_ROOF_TOP         = 0x0004,
    VEHICLE_WINDOW_ROW_1_LEFT       = 0x0010,
    VEHICLE_WINDOW_ROW_1_RIGHT      = 0x0020,
    VEHICLE_WINDOW_ROW_2_LEFT       = 0x0100,
    VEHICLE_WINDOW_ROW_2_RIGHT      = 0x0200,
    VEHICLE_WINDOW_ROW_3_LEFT       = 0x1000,
    VEHICLE_WINDOW_ROW_3_RIGHT      = 0x2000,
};

enum vehicle_door {
    VEHICLE_DOOR_ROW_1_LEFT    = 0x00000001,
    VEHICLE_DOOR_ROW_1_RIGHT   = 0x00000004,
    VEHICLE_DOOR_ROW_2_LEFT    = 0x00000010,
    VEHICLE_DOOR_ROW_2_RIGHT   = 0x00000040,
    VEHICLE_DOOR_ROW_3_LEFT    = 0x00000100,
    VEHICLE_DOOR_ROW_3_RIGHT   = 0x00000400,
    VEHICLE_DOOR_HOOD          = 0x10000000,
    VEHICLE_DOOR_REAR          = 0x20000000,
};

enum vehicle_mirror {
    VEHICLE_MIRROR_DRIVER_LEFT   = 0x00000001,
    VEHICLE_MIRROR_DRIVER_RIGHT  = 0x00000002,
    VEHICLE_MIRROR_DRIVER_CENTER = 0x00000004,
};

enum vehicle_turn_signal {
    VEHICLE_SIGNAL_NONE         = 0x00,
    VEHICLE_SIGNAL_RIGHT        = 0x01,
    VEHICLE_SIGNAL_LEFT         = 0x02,
    VEHICLE_SIGNAL_EMERGENCY    = 0x04
};

enum vehicle_zone_type {
    VEHICLE_ZONE_TYPE_NONE      = 0x00,
    VEHICLE_ZONE_TYPE_ZONE    = 0x01,
    VEHICLE_ZONE_TYPE_SEAT      = 0x02,
    VEHICLE_ZONE_TYPE_DOOR      = 0x04,
    VEHICLE_ZONE_TYPE_WINDOW    = 0x10,
    VEHICLE_ZONE_TYPE_MIRROR    = 0x20,
};

/*
 * Boolean type.
 */
enum vehicle_boolean {
    VEHICLE_FALSE = 0x00,
    VEHICLE_TRUE  = 0x01
};

typedef int32_t vehicle_boolean_t;

/**
 * Vehicle string.
 *
 * Defines a UTF8 encoded sequence of bytes that should be used for string
 * representation throughout.
 */
typedef struct vehicle_str {
    uint8_t* data;
    int32_t len;
} vehicle_str_t;

/**
 * Vehicle byte array.
 * This is for passing generic raw data.
 */
typedef vehicle_str_t vehicle_bytes_t;

typedef struct vehicle_prop_config {
    int32_t prop;

    /**
     * Defines if the property is read or write. Value should be one of
     * enum vehicle_prop_access.
     */
    int32_t access;

    /**
     * Defines if the property is continuous or on-change. Value should be one
     * of enum vehicle_prop_change_mode.
     */
    int32_t change_mode;

    /**
     * Type of data used for this property. This type is fixed per each property.
     * Check vehicle_value_type for allowed value.
     */
    int32_t value_type;

    /**
     * Define necessary permission model to access the data.
     */
    int32_t permission_model;

    /**
     * Some of the properties may have associated zones (such as hvac), in these
     * cases the config should contain an ORed value for the associated zone.
     */
    union {
        /**
         * The value is derived by ORing one or more of enum vehicle_zone members.
         */
        int32_t vehicle_zone_flags;
        /** The value is derived by ORing one or more of enum vehicle_seat members. */
        int32_t vehicle_seat_flags;
        /** The value is derived by ORing one or more of enum vehicle_window members. */
        int32_t vehicle_window_flags;
    };

    /**
     * Property specific configuration information. Usage of this will be defined per each property.
     */
    union {
        /**
         * For generic configuration information
         */
        int32_t config_flags;
        /** The number of presets that are stored by the radio module. Pass 0 if
         * there are no presets available. The range of presets is defined to be
         * from 1 (see VEHICLE_RADIO_PRESET_MIN_VALUE) to vehicle_radio_num_presets.
         */
        int32_t vehicle_radio_num_presets;
        int32_t config_array[4];
    };

    /**
     * Some properties may require additional information passed over this string. Most properties
     * do not need to set this and in that case, config_string.data should be NULL and
     * config_string.len should be 0.
     */
    vehicle_str_t config_string;

    /**
     * Specify minimum allowed value for the property. This is necessary for property which does
     * not have specified enum.
     */
    union {
        float float_min_value;
        int32_t int32_min_value;
        int64_t int64_min_value;
    };

    /**
     * Specify maximum allowed value for the property. This is necessary for property which does
     * not have specified enum.
     */
    union {
        float float_max_value;
        int32_t int32_max_value;
        int64_t int64_max_value;
    };

    /**
     * Array of min values for zoned properties. Zoned property can specify min / max value in two
     * different ways:
     *   1. All zones having the same min / max value: *_min/max_value should be set and this
     *      array should be set to NULL.
     *   2. All zones having separate min / max value: *_min/max_values array should be populated
     *      and its length should be the same as number of active zones specified by *_zone_flags.
     *
     * Should be NULL if each zone does not have separate max values.
     */
    union {
        float* float_min_values;
        int32_t* int32_min_values;
        int64_t* int64_min_values;
    };

    /**
     * Array of max values for zoned properties. See above for its usage.
     * Should be NULL if each zone does not have separate max values.
     * If not NULL, length of array should match that of min_values.
     */
    union {
        float* float_max_values;
        int32_t* int32_max_values;
        int64_t* int64_max_values;
    };

    /**
     * Min sample rate in Hz. Should be 0 for sensor type of VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
     */
    float min_sample_rate;
    /**
     * Max sample rate in Hz. Should be 0 for sensor type of VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
     */
    float max_sample_rate;
    /**
     * Place holder for putting HAL implementation specific data. Usage is wholly up to HAL
     * implementation.
     */
    void* hal_data;
} vehicle_prop_config_t;

/**
 * HVAC property fields.
 *
 * Defines various HVAC properties which are packed into vehicle_hvac_t (see
 * below). We define these properties outside in global scope so that HAL
 * implementation and HAL users (JNI) can typecast vehicle_hvac correctly.
 */
typedef struct vehicle_hvac {
    /**
     * Define one structure for each possible HVAC property.
     * NOTES:
     * a) Fan speed is a number from (0 - 6) where 6 is the highest speed. (TODO define enum)
     * b) Temperature is a floating point Celcius scale.
     * c) Direction is defined in enum vehicle_fan_direction.
     *
     * The HAL should create #entries number of vehicle_hvac_properties and
     * assign it to "properties" variable below.
     */
    union {
        int32_t fan_speed;
        int32_t fan_direction;
        vehicle_boolean_t ac_on;
        vehicle_boolean_t max_ac_on;
        vehicle_boolean_t max_defrost_on;
        vehicle_boolean_t recirc_on;
        vehicle_boolean_t dual_on;
        vehicle_boolean_t auto_on;
        vehicle_boolean_t power_on;

        float temperature_current;
        float temperature_set;

        vehicle_boolean_t defrost_on;
    };
} vehicle_hvac_t;

/*
 * Defines how the values for various properties are represented.
 *
 * There are two ways to populate and access the fields:
 * a) Using the individual fields. Use this mechanism (see
 * info_manufacture_date, fuel_capacity fields etc).
 * b) Using the union accessors (see uint32_value, float_value etc).
 *
 * To add a new field make sure that it does not exceed the total union size
 * (defined in int_array) and it is one of the vehicle_value_type. Then add the
 * field name with its unit to union. If the field type is not yet defined (as
 * of this draft, we don't use int64_t) then add that type to vehicle_value_type
 * and have an accessor (so for int64_t it will be int64_t int64_value).
 */
typedef union vehicle_value {
    /** Define the max size of this structure. */
    int32_t int32_array[4];
    float float_array[4];

    // Easy accessors for union members (HAL implementation SHOULD NOT USE these
    // fields while populating, use the property specific fields below instead).
    int32_t int32_value;
    int64_t int64_value;
    float float_value;
    vehicle_str_t str_value;
    vehicle_bytes_t bytes_value;
    vehicle_boolean_t boolean_value;

    // Vehicle Information.
    vehicle_str_t info_vin;
    vehicle_str_t info_make;
    vehicle_str_t info_model;
    int32_t info_model_year;

    // Represented in milliliters.
    float info_fuel_capacity;

    float vehicle_speed;
    float odometer;

    // Engine sensors.

    // Represented in milliliters.
    //float engine_coolant_level;
    // Represented in celcius.
    float engine_coolant_temperature;
    // Represented in a percentage value.
    //float engine_oil_level;
    // Represented in celcius.
    float engine_oil_temperature;
    float engine_rpm;

    // Event sensors.
    // Value should be one of enum vehicle_gear_selection.
    int32_t gear_selection;
    // Value should be one of enum vehicle_gear.
    int32_t gear_current_gear;
    // Value should be one of enum vehicle_boolean.
    int32_t parking_brake;
    // If cruise_set_speed > 0 then cruise is ON otherwise cruise is OFF.
    // Unit: meters / second (m/s).
    //int32_t cruise_set_speed;
    // Value should be one of enum vehicle_boolean.
    int32_t is_fuel_level_low;
    // Value should be one of enum vehicle_driving_status.
    int32_t driving_status;
    int32_t night_mode;
    // Value should be one of emum vehicle_turn_signal.
    int32_t turn_signals;
    // Value should be one of enum vehicle_boolean.
    //int32_t engine_on;

    // HVAC properties.
    vehicle_hvac_t hvac;

    float outside_temperature;
    float cabin_temperature;

} vehicle_value_t;

/*
 * Encapsulates the property name and the associated value. It
 * is used across various API calls to set values, get values or to register for
 * events.
 */
typedef struct vehicle_prop_value {
    /* property identifier */
    int32_t prop;

    /* value type of property for quick conversion from union to appropriate
     * value. The value must be one of enum vehicle_value_type.
     */
    int32_t value_type;

    /** time is elapsed nanoseconds since boot */
    int64_t timestamp;

    /**
     * Zone information for zoned property. For non-zoned property, this should be ignored.
     */
    union {
        int32_t zone;
        int32_t seat;
        int32_t window;
    };

    vehicle_value_t value;
} vehicle_prop_value_t;

/*
 * Event callback happens whenever a variable that the API user has subscribed
 * to needs to be reported. This may be based purely on threshold and frequency
 * (a regular subscription, see subscribe call's arguments) or when the set()
 * command is executed and the actual change needs to be reported.
 *
 * event_data is OWNED by the HAL and should be copied before the callback
 * finishes.
 */
typedef int (*vehicle_event_callback_fn)(const vehicle_prop_value_t *event_data);


/**
 * Represent the operation where the current error has happened.
 */
enum vehicle_property_operation {
    /** Generic error to this property which is not tied to any operation. */
    VEHICLE_OPERATION_GENERIC = 0,
    /** Error happened while handling property set. */
    VEHICLE_OPERATION_SET = 1,
    /** Error happened while handling property get. */
    VEHICLE_OPERATION_GET = 2,
    /** Error happened while handling property subscription. */
    VEHICLE_OPERATION_SUBSCRIBE = 3,
};

/*
 * Suggests that an error condition has occurred.
 *
 * @param error_code Error code. error_code should be standard error code with
 *                negative value like -EINVAL.
 * @parm property Note a property where error has happened. If this is generic error, property
 *                should be VEHICLE_PROPERTY_INVALID.
 * @param operation Represent the operation where the error has happened. Should be one of
 *                  vehicle_property_operation.
 */
typedef int (*vehicle_error_callback_fn)(int32_t error_code, int32_t property, int32_t operation);

/************************************************************************************/

/*
 * Every hardware module must have a data structure named HAL_MODULE_INFO_SYM
 * and the fields of this data structure must begin with hw_module_t
 * followed by module specific information.
 */
typedef struct vehicle_module {
    struct hw_module_t common;
} vehicle_module_t;


typedef struct vehicle_hw_device {
    struct hw_device_t common;

    /**
     * After calling open on device the user should register callbacks for event and error
     * functions.
     */
    int (*init)(struct vehicle_hw_device* device,
                vehicle_event_callback_fn event_fn, vehicle_error_callback_fn err_fn);
    /**
     * Before calling close the user should destroy the registered callback
     * functions.
     * In case the unsubscribe() call is not called on all properties before
     * release() then release() will unsubscribe the properties itself.
     */
    int (*release)(struct vehicle_hw_device* device);

    /**
     * Enumerate all available properties. The list is returned in "list".
     * @param num_properties number of properties contained in the retuned array.
     * @return array of property configs supported by this car. Note that returned data is const
     *         and caller cannot modify it. HAL implementation should keep this memory until HAL
     *         is released to avoid copying this again.
     */
    vehicle_prop_config_t const *(*list_properties)(struct vehicle_hw_device* device,
            int* num_properties);

    /**
     * Get a vehicle property value immediately. data should be allocated
     * properly.
     * The caller of the API OWNS the data field.
     * Caller will set data->prop, data->value_type, and optionally zone value for zoned property.
     * But HAL implementation needs to fill all entries properly when returning.
     * For pointer type, HAL implementation should allocate necessary memory and caller is
     * responsible for calling release_memory_from_get, which allows HAL to release allocated
     * memory.
     * For VEHICLE_PROP_CHANGE_MODE_STATIC type of property, get should return the same value
     * always.
     * For VEHICLE_PROP_CHANGE_MODE_ON_CHANGE type of property, it should return the latest value.
     * If there is no data available yet, which can happen during initial stage, this call should
     * return immediately with error code of -EAGAIN.
     */
    int (*get)(struct vehicle_hw_device* device, vehicle_prop_value_t *data);

    /**
     * Release memory allocated to data in previous get call. get call for byte or string involves
     * allocating necessary memory from vehicle hal.
     * To be safe, memory allocated by vehicle hal should be released by vehicle hal and vehicle
     * network service will call this when data from vehicle hal is no longer necessary.
     * vehicle hal implementation should only release member of vehicle_prop_value_t like
     * data->str_value.data or data->bytes_value.data but not data itself as data itself is
     * allocated from vehicle network service. Once memory is freed, corresponding pointer should
     * be set to NULL bu vehicle hal.
     */
    void (*release_memory_from_get)(struct vehicle_hw_device* device, vehicle_prop_value_t *data);

    /**
     * Set a vehicle property value.  data should be allocated properly and not
     * NULL.
     * The caller of the API OWNS the data field.
     * timestamp of data will be ignored for set operation.
     * Setting some properties require having initial state available. Depending on the vehicle hal,
     * such initial data may not be available for short time after init. In such case, set call
     * can return -EAGAIN like get call.
     * For a property with separate power control, set can fail if the property is not powered on.
     * In such case, hal should return -ESHUTDOWN error.
     */
    int (*set)(struct vehicle_hw_device* device, const vehicle_prop_value_t *data);

    /**
     * Subscribe to events.
     * Depending on output of list_properties if the property is:
     * a) on-change: sample_rate should be set to 0.
     * b) supports frequency: sample_rate should be set from min_sample_rate to
     * max_sample_rate.
     * For on-change type of properties, vehicle network service will make another get call to check
     * the initial state. Due to this, vehicle hal implementation does not need to send initial
     * state for on-change type of properties.
     * @param device
     * @param prop
     * @param sample_rate
     * @param zones All subscribed zones for zoned property. can be ignored for non-zoned property.
     *              0 means all zones supported instead of no zone.
     */
    int (*subscribe)(struct vehicle_hw_device* device, int32_t prop, float sample_rate,
            int32_t zones);

    /** Cancel subscription on a property. */
    int (*unsubscribe)(struct vehicle_hw_device* device, int32_t prop);

    /**
     * Print out debugging state for the vehicle hal. This will be called by
     * the vehicle network service and will be included into the service' dump.
     *
     * The passed-in file descriptor can be used to write debugging text using
     * dprintf() or write(). The text should be in ASCII encoding only.
     *
     * Performance requirements:
     *
     * This must be a non-blocking call. The HAL should return from this call
     * in 1ms, must return from this call in 10ms. This call must avoid
     * deadlocks, as it may be called at any point of operation.
     * Any synchronization primitives used (such as mutex locks or semaphores)
     * should be acquired with a timeout.
     */
    int (*dump)(struct vehicle_hw_device* device, int fd);

} vehicle_hw_device_t;

__END_DECLS

#endif  // ANDROID_VEHICLE_INTERFACE_H
