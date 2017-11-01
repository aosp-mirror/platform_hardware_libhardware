/*
 * Copyright (C) 2017 The Android Open Source Project
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
#ifndef HID_SENSOR_DEF_H_
#define HID_SENSOR_DEF_H_
namespace Hid {
namespace Sensor {
namespace GeneralUsage {
enum {
    STATE = 0x200201,
    EVENT = 0x200202,
};

} //namespace Usage
namespace PropertyUsage {
enum {
    FRIENDLY_NAME = 0x200301,
    MINIMUM_REPORT_INTERVAL = 0x200304,
    PERSISTENT_UNIQUE_ID = 0x200302,
    POWER_STATE = 0x200319,
    RANGE_MAXIMUM = 0x200314,
    RANGE_MINIMUM = 0x200315,
    REPORTING_STATE = 0x200316,
    REPORT_INTERVAL = 0x20030E,
    RESOLUTION = 0x200313,
    SAMPLING_RATE =0x200317,
    SENSOR_CONNECTION_TYPE = 0x200309,
    SENSOR_DESCRIPTION = 0x200308,
    SENSOR_MANUFACTURER = 0x200305,
    SENSOR_MODEL = 0x200306,
    SENSOR_SERIAL_NUMBER = 0x200307,
    SENSOR_STATUS = 0x200303,
};
} // nsmespace PropertyUsage

namespace SensorTypeUsage {
enum {
    ACCELEROMETER_3D = 0x200073,
    COMPASS_3D = 0x200083,
    CUSTOM = 0x2000E1,
    DEVICE_ORIENTATION = 0x20008A,
    GYROMETER_3D = 0x200076,
};
} // namespace SensorTypeUsage

namespace ReportUsage {
enum {
    ACCELERATION_X_AXIS = 0x200453,
    ACCELERATION_Y_AXIS = 0x200454,
    ACCELERATION_Z_AXIS = 0x200455,
    ANGULAR_VELOCITY_X_AXIS = 0x200457,
    ANGULAR_VELOCITY_Y_AXIS = 0x200458,
    ANGULAR_VELOCITY_Z_AXIS = 0x200459,
    CUSTOM_VALUE_1 = 0x200544,
    CUSTOM_VALUE_2 = 0x200545,
    CUSTOM_VALUE_3 = 0x200546,
    CUSTOM_VALUE_4 = 0x200547,
    CUSTOM_VALUE_5 = 0x200548,
    CUSTOM_VALUE_6 = 0x200549,
    MAGNETIC_FLUX_X_AXIS = 0x200485,
    MAGNETIC_FLUX_Y_AXIS = 0x200486,
    MAGNETIC_FLUX_Z_AXIS = 0x200487,
    MAGNETOMETER_ACCURACY = 0x200488,
    ORIENTATION_QUATERNION = 0x200483,
};
} // namespace ReportUsage

namespace RawMinMax {
enum {
    REPORTING_STATE_MIN = 0,
    REPORTING_STATE_MAX = 5,
    POWER_STATE_MIN = 0,
    POWER_STATE_MAX = 5,
};
} // namespace RawMinMax

namespace StateValue {
enum {
    POWER_STATE_FULL_POWER = 1,
    POWER_STATE_POWER_OFF = 5,

    REPORTING_STATE_ALL_EVENT = 1,
    REPORTING_STATE_NO_EVENT = 0,
};
} // StateValue
} // namespace Sensor
} // namespace Hid
#endif // HID_SENSOR_DEF_H_

