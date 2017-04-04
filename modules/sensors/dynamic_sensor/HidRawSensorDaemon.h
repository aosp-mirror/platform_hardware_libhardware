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

#ifndef ANDROID_SENSORHAL_EXT_HIDRAW_SENSOR_DAEMON_H
#define ANDROID_SENSORHAL_EXT_HIDRAW_SENSOR_DAEMON_H

#include "BaseDynamicSensorDaemon.h"

#include <HidParser.h>
#include <hardware/sensors.h>
#include <utils/Thread.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


using HidUtil::HidParser;
using HidUtil::HidReport;
using HidUtil::HidItem;

namespace android {
namespace SensorHalExt {

class HidRawSensorDevice;
class ConnectionDetector;

class HidRawSensorDaemon : public BaseDynamicSensorDaemon {
    friend class HidRawSensorDaemonTest;
public:
    HidRawSensorDaemon(DynamicSensorManager& manager);
    virtual ~HidRawSensorDaemon() = default;
private:
    virtual BaseSensorVector createSensor(const std::string &deviceKey);
    virtual void removeSensor(const std::string &deviceKey);

    class HidRawSensor;
    void registerExisting();

    sp<ConnectionDetector> mDetector;
    std::unordered_map<std::string, sp<HidRawSensorDevice>> mHidRawSensorDevices;
};

} // namespace SensorHalExt
} // namespace android

#endif // ANDROID_SENSORHAL_EXT_HIDRAW_SENSOR_DAEMON_H

