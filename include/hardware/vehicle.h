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
 */
//===== Vehicle Information ====

/**
 * Invalid property value used for argument where invalid property gives different result.
 * @range_start
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
#define VEHICLE_PROPERTY_INFO_MODEL_YEAR                           (0x00000103)

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
 * @config_flags Supported zones
 * @data_member hvac.fan_speed
 * @zone_type VEHICLE_ZONE
 * @data_enum TODO
 */
#define VEHICLE_PROPERTY_HVAC_FAN_SPEED                             (0x00000500)

/**
 * Fan direction setting
 * @value_type VEHICLE_VALUE_TYPE_ZONED_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @config_flags Supported zones
 * @data_member hvac.fan_direction
 * @zone_type VEHICLE_ZONE
 * @data_enum TODO
 */
#define VEHICLE_PROPERTY_HVAC_FAN_DIRECTION                         (0x00000501)

/*
 * Bit flags for fan direction
 */
enum vehicle_hvac_fan_direction_flags {
    VEHICLE_HVAC_FAN_DIRECTION_FACE_FLAG            = 0x1,
    VEHICLE_HVAC_FAN_DIRECTION_FLOOR_FLAG           = 0x2,
    VEHICLE_HVAC_FAN_DIRECTION_FACE_AND_FLOOR_FLAG  = 0x3
};

/**
 * HVAC current temperature.
 * @value_type VEHICLE_VALUE_TYPE_ZONED_FLOAT
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE|VEHICLE_PROP_CHANGE_MODE_CONTINUOUS
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @config_flags Supported zones
 * @zone_type VEHICLE_ZONE
 * @data_member hvac.temperature_current
 */
#define VEHICLE_PROPERTY_HVAC_TEMPERATURE_CURRENT                   (0x00000502)

/**
 * HVAC, target temperature set.
 * @value_type VEHICLE_VALUE_TYPE_ZONED_FLOAT
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE|VEHICLE_PROP_CHANGE_MODE_CONTINUOUS
 * @config_flags Supported zones
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @zone_type VEHICLE_ZONE
 * @data_member hvac.temperature_set
 */
#define VEHICLE_PROPERTY_HVAC_TEMPERATURE_SET                       (0x00000503)

/**
 * On/off defrost
 * @value_type VEHICLE_VALUE_TYPE_ZONED_BOOLEAN
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @config_flags Supported zones
 * @data_member hvac.defrost_on
 */
#define VEHICLE_PROPERTY_HVAC_DEFROSTER                             (0x00000504)

/**
 * On/off AC
 * @value_type VEHICLE_VALUE_TYPE_ZONED_BOOLEAN
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @config_flags Supported zones
 * @zone_type VEHICLE_ZONE
 * @data_member hvac.ac_on
 */
#define VEHICLE_PROPERTY_HVAC_AC_ON                                 (0x00000505)

/**
 * On/off max AC
 * @value_type VEHICLE_VALUE_TYPE_BOOLEAN
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @data_member hvac.max_ac_on
 */
#define VEHICLE_PROPERTY_HVAC_MAX_AC_ON                             (0x00000506)

/**
 * On/off max defrost
 * @value_type VEHICLE_VALUE_TYPE_BOOLEAN
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @data_member hvac.max_defrost_on
 */
#define VEHICLE_PROPERTY_HVAC_MAX_DEFROST_ON                        (0x00000507)

/**
 * On/off re-circulation
 * @value_type VEHICLE_VALUE_TYPE_BOOLEAN
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @data_member hvac.max_recirc_on
 */
#define VEHICLE_PROPERTY_HVAC_RECIRC_ON                             (0x00000508)

/**
 * On/off dual
 * @value_type VEHICLE_VALUE_TYPE_BOOLEAN
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @data_member hvac.dual_on
 */
#define VEHICLE_PROPERTY_HVAC_DUAL_ON                               (0x00000509)

/**
 * Outside temperature
 * @value_type VEHICLE_VALUE_TYPE_FLOAT
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE|VEHICLE_PROP_CHANGE_MODE_CONTINUOUS
 * @access VEHICLE_PROP_ACCESS_READ
 * @data_member outside_temperature
 * @unit VEHICLE_UNIT_TYPE_CELCIUS
 */
#define VEHICLE_PROPERTY_ENV_OUTSIDE_TEMP                           (0x00000703)


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
#define VEHICLE_PROPERTY_RADIO_PRESET                               (0x0000801)

/**
 * Constants relevant to radio.
 */
enum vehicle_radio_consts {
    /** Minimum value for the radio preset */
    VEHICLE_RADIO_PRESET_MIN_VALUE = 1,
};

/**
 * Represents audio focus state of Android side. Note that car's audio module will own audio
 * focus and grant audio focus to Android side when requested by Android side. The focus has both
 * per stream characteristics and global characteristics.
 *
 * Focus request (get of this property) will take the following form in int32_vec4:
 *   int32_array[0]: vehicle_audio_focus_request type
 *   int32_array[1]: bit flags of streams requested by this focus request. There can be up to
 *                   32 streams.
 *   int32_array[2]: External focus state flags. For request, only flag like
 *                   VEHICLE_AUDIO_EXT_FOCUS_CAR_PLAY_ONLY_FLAG can be used.
 *                   This is for case like radio where android side app still needs to hold focus
 *                   but playback is done outside Android.
 * Note that each focus request can request multiple streams that is expected to be used for
 * the current request. But focus request itself is global behavior as GAIN or GAIN_TRANSIENT
 * expects all sounds played by car's audio module to stop. Note that stream already allocated to
 * android before this focus request should not be affected by focus request.
 *
 * Focus response (set and subscription callback for this property) will take the following form:
 *   int32_array[0]: vehicle_audio_focus_state type
 *   int32_array[1]: bit flags of streams allowed.
 *   int32_array[2]: External focus state: bit flags of currently active audio focus in car
 *                   side (outside Android). Active audio focus does not necessarily mean currently
 *                   playing, but represents the state of having focus or waiting for focus
 *                   (pause state).
 *                   One or combination of flags from vehicle_audio_ext_focus_flag.
 *                   0 means no active audio focus holder outside Android.
 *                   The state will have following values for each vehicle_audio_focus_state_type:
 *                   GAIN: 0 or VEHICLE_AUDIO_EXT_FOCUS_CAR_PLAY_ONLY when radio is active in
 *                       Android side.
 *                   GAIN_TRANSIENT: 0. Can be VEHICLE_AUDIO_EXT_FOCUS_CAR_PERMANENT or
 *                       VEHICLE_AUDIO_EXT_FOCUS_CAR_TRANSIENT if android side has requested
 *                       GAIN_TRANSIENT_MAY_DUCK and car side is ducking.
 *                   LOSS: 0 when no focus is audio is active in car side.
 *                       VEHICLE_AUDIO_EXT_FOCUS_CAR_PERMANENT when car side is playing something
 *                       permanent.
 *                   LOSS_TRANSIENT: always should be VEHICLE_AUDIO_EXT_FOCUS_CAR_TRANSIENT
 *
 * If car does not support VEHICLE_PROPERTY_AUDIO_FOCUS, focus is assumed to be granted always.
 *
 * @value_type VEHICLE_VALUE_TYPE_INT32_VEC3
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @data_member int32_array
 */
#define VEHICLE_PROPERTY_AUDIO_FOCUS                                  (0x00000900)

enum vehicle_audio_focus_request {
    VEHICLE_AUDIO_FOCUS_REQUEST_GAIN = 0x1,
    VEHICLE_AUDIO_FOCUS_REQUEST_GAIN_TRANSIENT = 0x2,
    VEHICLE_AUDIO_FOCUS_REQUEST_GAIN_TRANSIENT_MAY_DUCK = 0x3,
    VEHICLE_AUDIO_FOCUS_REQUEST_RELEASE = 0x4,
};

enum vehicle_audio_focus_state {
    /**
     * Android side has permanent focus and can play allowed streams.
     */
    VEHICLE_AUDIO_FOCUS_STATE_GAIN = 0x1,
    /**
     * Android side has transient focus and can play allowed streams.
     */
    VEHICLE_AUDIO_FOCUS_STATE_GAIN_TRANSIENT = 0x2,
    /**
     * Car audio module is playing guidance kind of sound outside Android. Android side can
     * still play through allowed streams with ducking.
     */
    VEHICLE_AUDIO_FOCUS_STATE_LOSS_TRANSIENT_CAN_DUCK = 0x3,
    /**
     * Car audio module is playing transient sound outside Android. Android side should stop
     * playing any sounds.
     */
    VEHICLE_AUDIO_FOCUS_STATE_LOSS_TRANSIENT = 0x4,
    /**
     * Android side has lost focus and cannot play any sound.
     */
    VEHICLE_AUDIO_FOCUS_STATE_LOSS = 0x5,
    /**
     * car audio module is playing safety critical sound, and Android side cannot request focus
     * until the current state is finished. car audio module should restore it to the previous
     * state when it can allow Android to play.
     */
    VEHICLE_AUDIO_FOCUS_STATE_LOSS_TRANSIENT_EXLCUSIVE = 0x6,
};

/**
 * Flags to represent multiple streams by combining these.
 */
enum vehicle_audio_stream_flag {
    VEHICLE_AUDIO_STREAM_STREAM0_FLAG = (0x1<<0),
    VEHICLE_AUDIO_STREAM_STREAM1_FLAG = (0x1<<1),
    VEHICLE_AUDIO_STREAM_STREAM2_FLAG = (0x1<<2),
};

/**
 * Represents stream number (always 0 to N -1 where N is max number of streams).
 * Can be used for audio related property expecting one stream.
 */
enum vehicle_audio_stream {
    VEHICLE_AUDIO_STREAM0 = 0,
    VEHICLE_AUDIO_STREAM1 = 1,
};

/**
 * Flag to represent external focus state (outside Android).
 */
enum vehicle_audio_ext_focus_flag {
    /**
     * No external focus holder.
     */
    VEHICLE_AUDIO_EXT_FOCUS_NONE_FLAG = 0x0,
    /**
     * Car side (outside Android) has component holding GAIN kind of focus state.
     */
    VEHICLE_AUDIO_EXT_FOCUS_CAR_PERMANENT_FLAG = 0x1,
    /**
     * Car side (outside Android) has component holding GAIN_TRANSIENT kind of focus state.
     */
    VEHICLE_AUDIO_EXT_FOCUS_CAR_TRANSIENT_FLAG = 0x2,
    /**
     * Car side is expected to play something while focus is held by Android side. One example
     * can be radio attached in car side. But Android's radio app still should have focus,
     * and Android side should be in GAIN state, but media stream will not be allocated to Android
     * side and car side can play radio any time while this flag is active.
     */
    VEHICLE_AUDIO_EXT_FOCUS_CAR_PLAY_ONLY_FLAG = 0x4,
};

/**
 * Index in int32_array for VEHICLE_PROPERTY_AUDIO_FOCUS property.
 */
enum vehicle_audio_focus_index {
    VEHICLE_AUDIO_FOCUS_INDEX_FOCUS = 0,
    VEHICLE_AUDIO_FOCUS_INDEX_STREAMS = 1,
    VEHICLE_AUDIO_FOCUS_INDEX_EXTERNAL_FOCUS_STATE = 2,
};

/**
 * Property to control audio volume of each audio context.
 *
 * Data type looks like:
 *   int32_array[0] : stream context as defined in vehicle_audio_context_flag.
 *   int32_array[1] : volume level, valid range is 0 to int32_max_value defined in config.
 *                    0 will be mute state. int32_min_value in config should be always 0.
 *   int32_array[2] : One of vehicle_audio_volume_state.
 *
 * This property requires per stream based get. HAL implementation should check stream number
 * in get call to return the right volume.
 *
 * @value_type VEHICLE_VALUE_TYPE_INT32_VEC3
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @config_flags all audio contexts supported.
 * @data_member int32_array
 */
#define VEHICLE_PROPERTY_AUDIO_VOLUME                                 (0x00000901)

/**
 * enum to represent audio volume state.
 */
enum vehicle_audio_volume_state {
    VEHICLE_AUDIO_VOLUME_STATE_OK            = 0,
    /**
     * Audio volume has reached volume limit set in VEHICLE_PROPERTY_AUDIO_VOLUME_LIMIT
     * and user's request to increase volume further is not allowed.
     */
    VEHICLE_AUDIO_VOLUME_STATE_LIMIT_REACHED = 1,
};

/**
 * Index in int32_array for VEHICLE_PROPERTY_AUDIO_VOLUME property.
 */
enum vehicle_audio_volume_index {
    VEHICLE_AUDIO_VOLUME_INDEX_STREAM = 0,
    VEHICLE_AUDIO_VOLUME_INDEX_VOLUME = 1,
    VEHICLE_AUDIO_VOLUME_INDEX_STATE = 2,
};

/**
 * Property for handling volume limit set by user. This limits maximum volume that can be set
 * per each context.
 *   int32_array[0] : stream context as defined in vehicle_audio_context_flag.
 *   int32_array[1] : maximum volume set to the stream. If there is no restriction, this value
 *                    will be  bigger than VEHICLE_PROPERTY_AUDIO_VOLUME's max value.
 *
 * If car does not support this feature, this property should not be populated by HAL.
 * This property requires per stream based get. HAL implementation should check stream number
 * in get call to return the right volume.
 *
 * @value_type VEHICLE_VALUE_TYPE_INT32_VEC2
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_READ_WRITE
 * @config_flags all audio contexts supported.
 * @data_member int32_array
 */
#define VEHICLE_PROPERTY_AUDIO_VOLUME_LIMIT                           (0x00000902)

/**
 * Index in int32_array for VEHICLE_PROPERTY_AUDIO_VOLUME_LIMIT property.
 */
enum vehicle_audio_volume_limit_index {
    VEHICLE_AUDIO_VOLUME_LIMIT_INDEX_STREAM = 0,
    VEHICLE_AUDIO_VOLUME_LIMIT_INDEX_MAX_VOLUME = 1,
};

/**
 * Property to share audio routing policy of android side. This property is set at the beginning
 * to pass audio policy in android side down to vehicle HAL and car audio module.
 * This can be used as a hint to adjust audio policy or other policy decision.
 *
 *   int32_array[0] : audio stream where the audio for the application context will be routed
 *                    by default. Note that this is the default setting from system, but each app
 *                    may still use different audio stream for whatever reason.
 *   int32_array[1] : All audio contexts that will be sent through the physical stream. Flag
 *                    is defined in vehicle_audio_context_flag.

 * Setting of this property will be done for all available physical streams based on audio H/W
 * variant information acquired from VEHICLE_PROPERTY_AUDIO_HW_VARIANT property.
 *
 * @value_type VEHICLE_VALUE_TYPE_INT32_VEC2
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_WRITE
 * @data_member int32_array
 */
#define VEHICLE_PROPERTY_AUDIO_ROUTING_POLICY                         (0x00000903)

/**
 * Index in int32_array for VEHICLE_PROPERTY_AUDIO_ROUTING_POLICY property.
 */
enum vehicle_audio_routing_policy_index {
    VEHICLE_AUDIO_ROUTING_POLICY_INDEX_STREAM = 0,
    VEHICLE_AUDIO_ROUTING_POLICY_INDEX_CONTEXTS = 1,
};

/**
* Property to return audio H/W variant type used in this car. This allows android side to
* support different audio policy based on H/W variant used. Note that other components like
* CarService may need overlay update to support additional variants. If this property does not
* exist, default audio policy will be used.
*
* @value_type VEHICLE_VALUE_TYPE_INT32
* @change_mode VEHICLE_PROP_CHANGE_MODE_STATIC
* @access VEHICLE_PROP_ACCESS_READ
* @config_flags Additional info on audio H/W. Should use vehicle_audio_hw_variant_config_flag for
*               this.
* @data_member int32_value
*/
#define VEHICLE_PROPERTY_AUDIO_HW_VARIANT                             (0x00000904)

/**
 * Flag to be used in vehicle_prop_config.config_flags for VEHICLE_PROPERTY_AUDIO_HW_VARIANT.
 */
enum vehicle_audio_hw_variant_config_flag {
    /**
     * This is a flag to disable the default behavior of not sending focus request for radio module.
     * By default, when radio app request audio focus, that focus request is filtered out and
     * is not sent to car audio module as radio is supposed to be played by car radio module and
     * android side should have have audio focus for media stream.
     * But in some H/W, radio may be directly played from android side, and in that case,
     * android side should take focus for media stream. This flag should be enabled in such case.
     */
    VEHICLE_AUDIO_HW_VARIANT_FLAG_PASS_RADIO_AUDIO_FOCUS_FLAG = 0x1,
};

/**
 * Property to share currently active audio context in android side.
 * This can be used as a hint to adjust audio policy or other policy decision. Note that there
 * can be multiple context active at the same time.
 *
 * @value_type VEHICLE_VALUE_TYPE_INT32
 * @change_mode VEHICLE_PROP_CHANGE_MODE_ON_CHANGE
 * @access VEHICLE_PROP_ACCESS_WRITE
 * @data_member int32
 */
#define VEHICLE_PROPERTY_AUDIO_CONTEXT                        (0x00000905)
/**
 * Flags to tell the current audio context.
 */
enum vehicle_audio_context_flag {
    /** Music playback is currently active. */
    VEHICLE_AUDIO_CONTEXT_MUSIC_FLAG                      = 0x1,
    /** Navigation is currently running. */
    VEHICLE_AUDIO_CONTEXT_NAVIGATION_FLAG                 = 0x2,
    /** Voice command session is currently running. */
    VEHICLE_AUDIO_CONTEXT_VOICE_COMMAND_FLAG              = 0x4,
    /** Voice call is currently active. */
    VEHICLE_AUDIO_CONTEXT_CALL_FLAG                       = 0x8,
    /** Alarm is active. This may be only used in VEHICLE_PROPERTY_AUDIO_ROUTING_POLICY. */
    VEHICLE_AUDIO_CONTEXT_ALARM_FLAG                      = 0x10,
    /**
     * Notification sound is active. This may be only used in VEHICLE_PROPERTY_AUDIO_ROUTING_POLICY.
     */
    VEHICLE_AUDIO_CONTEXT_NOTIFICATION_FLAG               = 0x20,
    /**
     * Context unknown. Only used for VEHICLE_PROPERTY_AUDIO_ROUTING_POLICY to represent default
     * stream for unknown contents.
     */
    VEHICLE_AUDIO_CONTEXT_UNKNOWN_FLAG                    = 0x40,
    /** Safety alert / warning is played. */
    VEHICLE_AUDIO_CONTEXT_SAFETY_ALERT_FLAG               = 0x80,
    /** CD / DVD kind of audio is played */
    VEHICLE_AUDIO_CONTEXT_CD_ROM                         = 0x100,
    /** Aux audio input is played */
    VEHICLE_AUDIO_CONTEXT_AUX_AUDIO                      = 0x200,
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
#define VEHICLE_PROPERTY_AP_POWER_STATE                               (0x00000A00)

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
#define VEHICLE_PROPERTY_DISPLAY_BRIGHTNESS                        (0x00000A01)


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
 *  H/W specific, non-standard property can be added as necessary. Such property should use
 *  property number in range of [VEHICLE_PROPERTY_CUSTOM_START, VEHICLE_PROPERTY_CUSTOM_END].
 *  Definition of property in this range is completely up to each HAL implementation.
 *  For such property, it is recommended to fill vehicle_prop_config.config_string with some
 *  additional information to help debugging. For example, company XYZ's custom extension may
 *  include config_string of "com.XYZ.some_further_details".
 *  @range_start
 */
#define VEHICLE_PROPERTY_CUSTOM_START                       (0x70000000)
/** @range_end */
#define VEHICLE_PROPERTY_CUSTOM_END                         (0x73ffffff)

/**
 * Property range allocated for system's internal usage like testing. HAL should never declare
 * property in this range.
 * @range_start
 */
#define VEHICLE_PROPERTY_INTERNAL_START                     (0x74000000)
/**
 * @range_end
 */
#define VEHICLE_PROPERTY_INTERNAL_END                       (0x74ffffff)

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
    VEHICLE_VALUE_TYPE_ZONED_INT32              = 0x40,
    VEHICLE_VALUE_TYPE_ZONED_INT32_VEC2         = 0x41,
    VEHICLE_VALUE_TYPE_ZONED_INT32_VEC3         = 0x42,
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
    VEHICLE_UNIT_TYPE_CELCIUS               = 0x00000030,
    // volume
    VEHICLE_UNIT_TYPE_MILLILITER            = 0x00000040,
    // time
    VEHICLE_UNIT_TYPE_NANO_SECS             = 0x00000050,
    VEHICLE_UNOT_TYPE_SECS                  = 0x00000053,
    VEHICLE_UNIT_TYPE_YEAR                  = 0x00000059,
};

/**
 * Error code used in HAL implemnentation. Follows utils/Errors.h
 */
enum vehicle_error_code {
    VEHICLE_NO_ERROR    = 0x0,
    VEHICLE_ERROR_UNKNOWN       = (-2147483647 - 1), // INT32_MIN value
    VEHICLE_ERROR_NO_MEMORY           = -12, //ENOMEM
    VEHICLE_ERROR_INVALID_OPERATION   = -38, //ENOSYS
    VEHICLE_ERROR_BAD_VALUE           = -22, //EINVAL
    VEHICLE_ERROR_BAD_TYPE            = (VEHICLE_ERROR_UNKNOWN + 1),
    VEHICLE_ERROR_NAME_NOT_FOUND      = -2, //ENOENT
    VEHICLE_ERROR_PERMISSION_DENIED   = -1, //EPERM
    VEHICLE_ERROR_NO_INIT             = -19, //ENODEV
    VEHICLE_ERROR_ALREADY_EXISTS      = -17, //EEXIST
    VEHICLE_ERROR_DEAD_OBJECT         = -32, //EPIPE
    VEHICLE_ERROR_FAILED_TRANSACTION  = (VEHICLE_ERROR_UNKNOWN + 2),
    VEHICLE_ERROR_BAD_INDEX           = -75, //EOVERFLOW
    VEHICLE_ERROR_NOT_ENOUGH_DATA     = -61, //ENODATA
    VEHICLE_ERROR_WOULD_BLOCK         = -11, //EWOULDBLOCK
    VEHICLE_ERROR_TIMED_OUT           = -110, //ETIMEDOUT
    VEHICLE_ERROR_UNKNOWN_TRANSACTION = -74, //EBADMSG
    VEHICLE_FDS_NOT_ALLOWED     = (VEHICLE_ERROR_UNKNOWN + 7),
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
     * Property of this type will be reported when there is a change. get should return the
     * current value.
     */
    VEHICLE_PROP_CHANGE_MODE_ON_CHANGE      = 0x01,
    /**
     * Property of this type change continuously and requires fixed rate of sampling to retrieve
     * the data.
     */
    VEHICLE_PROP_CHANGE_MODE_CONTINUOUS     = 0x02,
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
    VEHICLE_GEAR_PARKING    = 0x0004,
    VEHICLE_GEAR_DRIVE      = 0x0008,
    VEHICLE_GEAR_L          = 0x0010,

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
    VEHICLE_SEAT_DRIVER_LHD        = 0x0001,
    VEHICLE_SEAT_DRIVER_RHD        = 0x0002,
    VEHICLE_SEAT_ROW_1_PASSENGER_1 = 0x0010,
    VEHICLE_SEAT_ROW_1_PASSENGER_2 = 0x0020,
    VEHICLE_SEAT_ROW_1_PASSENGER_3 = 0x0040,
    VEHICLE_SEAT_ROW_2_PASSENGER_1 = 0x0100,
    VEHICLE_SEAT_ROW_2_PASSENGER_2 = 0x0200,
    VEHICLE_SEAT_ROW_2_PASSENGER_3 = 0x0400,
    VEHICLE_SEAT_ROW_3_PASSENGER_1 = 0x1000,
    VEHICLE_SEAT_ROW_3_PASSENGER_2 = 0x2000,
    VEHICLE_SEAT_ROW_3_PASSENGER_3 = 0x4000
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

enum vehicle_turn_signal {
    VEHICLE_SIGNAL_NONE         = 0x00,
    VEHICLE_SIGNAL_RIGHT        = 0x01,
    VEHICLE_SIGNAL_LEFT         = 0x02,
    VEHICLE_SIGNAL_EMERGENCY    = 0x04
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

typedef struct vehicle_zoned_int32 {
    union {
        int32_t zone;
        int32_t seat;
        int32_t window;
    };
    int32_t value;
} vehicle_zoned_int32_t;

typedef struct vehicle_zoned_int32_array {
    union {
        int32_t zone;
        int32_t seat;
        int32_t window;
    };
    int32_t values[3];
} vehicle_zoned_int32_array_t;

typedef struct vehicle_zoned_float {
    union {
        int32_t zone;
        int32_t seat;
        int32_t window;
    };
    float value;
} vehicle_zoned_float_t;

typedef struct vehicle_zoned_float_array {
    union {
        int32_t zone;
        int32_t seat;
        int32_t window;
    };
    float values[3];
} vehicle_zoned_float_array_t;

typedef struct vehicle_zoned_boolean {
    union {
        int32_t zone;
        int32_t seat;
        int32_t window;
    };
    vehicle_boolean_t value;
} vehicle_zoned_boolean_t;


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
         * For generic configuration information
         */
        int32_t config_flags;

        /**
         * The value is derived by ORing one or more of enum vehicle_zone members.
         */
        int32_t vehicle_zone_flags;
        /** The value is derived by ORing one or more of enum vehicle_seat members. */
        int32_t vehicle_seat_flags;
        /** The value is derived by ORing one or more of enum vehicle_window members. */
        int32_t vehicle_window_flags;

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
typedef vehicle_zoned_int32_t vehicle_hvac_fan_speed_t;

typedef vehicle_zoned_int32_t vehicle_hvac_fan_direction_t;

typedef vehicle_zoned_float_t vehicle_hvac_zone_temperature_t;

//TODO Typical seat heat/cooling is done in fixed steps. Needs better definition.
//typedef struct vehicle_hvac_seat_temperature {
//    // Value should be one of enum vehicle_seat.
//    int32_t seat;
//    float temperature;
//} vehicle_hvac_heated_seat_temperature_t;

typedef vehicle_zoned_boolean_t vehicle_hvac_defrost_on_t;

typedef vehicle_zoned_boolean_t vehicle_hvac_ac_on_t;

typedef vehicle_boolean_t vehicle_hvac_max_ac_on_t;

typedef vehicle_boolean_t vehicle_hvac_max_defrost_on_t;

typedef vehicle_boolean_t vehicle_hvac_recirc_on_t;

typedef vehicle_boolean_t vehicle_hvac_dual_on_t;

typedef struct vehicle_hvac {
    /**
     * Define one structure for each possible HVAC property.
     * NOTES:
     * a) Zone is defined in enum vehicle_zone.
     * b) Fan speed is a number from (0 - 6) where 6 is the highest speed. (TODO define enum)
     * c) Temperature is a floating point Celcius scale.
     * d) Direction is defined in enum vehicle_fan_direction.
     *
     * The HAL should create #entries number of vehicle_hvac_properties and
     * assign it to "properties" variable below.
     */
    union {
        vehicle_hvac_fan_speed_t fan_speed;
        vehicle_hvac_fan_direction_t fan_direction;
        vehicle_hvac_ac_on_t ac_on;
        vehicle_hvac_max_ac_on_t max_ac_on;
        vehicle_hvac_max_defrost_on_t max_defrost_on;
        vehicle_hvac_recirc_on_t recirc_on;
        vehicle_hvac_dual_on_t dual_on;

        vehicle_hvac_zone_temperature_t temperature_current;
        vehicle_hvac_zone_temperature_t temperature_set;

        //TODO Heated seat.
        //vehicle_hvac_heated_seat_t heated_seat;

        vehicle_hvac_defrost_on_t defrost_on;
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
    vehicle_zoned_int32_t zoned_int32_value;
    vehicle_zoned_int32_array_t zoned_int32_array;
    vehicle_zoned_float_t zoned_float_value;
    vehicle_zoned_float_array_t zoned_float_array;
    vehicle_zoned_boolean_t zoned_boolean_value;

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
 * Suggests that an error condition has occured. error_code should be one of
 * enum vehicle_error_code.
 *
 * @param error_code Error code. It should be one of enum vehicle_error_code.
 *                   See error code for details.
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
     * responsible for freeing memory for the pointer.
     * For VEHICLE_PROP_CHANGE_MODE_STATIC type of property, get should return the same value
     * always.
     * For VEHICLE_PROP_CHANGE_MODE_ON_CHANGE type of property, it should return the latest value.
     */
    int (*get)(struct vehicle_hw_device* device, vehicle_prop_value_t *data);

    /**
     * Set a vehicle property value.  data should be allocated properly and not
     * NULL.
     * The caller of the API OWNS the data field.
     * timestamp of data will be ignored for set operation.
     */
    int (*set)(struct vehicle_hw_device* device, const vehicle_prop_value_t *data);

    /**
     * Subscribe to events.
     * Depending on output of list_properties if the property is:
     * a) on-change: sample_rate should be set to 0.
     * b) supports frequency: sample_rate should be set from min_sample_rate to
     * max_sample_rate.
     * Subscribing to properties in-correctly may result in error callbacks and
     * will depend on HAL implementation.
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
} vehicle_hw_device_t;

__END_DECLS

#endif  // ANDROID_VEHICLE_INTERFACE_H
