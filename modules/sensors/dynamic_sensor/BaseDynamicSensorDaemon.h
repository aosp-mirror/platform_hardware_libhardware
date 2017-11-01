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

#ifndef ANDROID_SENSORHAL_EXT_BASE_DYNAMIC_SENSOR_DAEMON_H
#define ANDROID_SENSORHAL_EXT_BASE_DYNAMIC_SENSOR_DAEMON_H

#include "BaseSensorObject.h"

#include <utils/RefBase.h>
#include <string>
#include <unordered_map>

namespace android {
namespace SensorHalExt {

class DynamicSensorManager;

typedef std::vector<sp<BaseSensorObject>> BaseSensorVector;

class BaseDynamicSensorDaemon : public RefBase {
public:
    BaseDynamicSensorDaemon(DynamicSensorManager& manager) : mManager(manager) {}
    virtual ~BaseDynamicSensorDaemon() = default;

    virtual bool onConnectionChange(const std::string &deviceKey, bool connected);
protected:
    virtual BaseSensorVector createSensor(const std::string &deviceKey) = 0;
    virtual void removeSensor(const std::string &/*deviceKey*/) {};

    DynamicSensorManager &mManager;
    std::unordered_map<std::string, BaseSensorVector> mDeviceKeySensorMap;
};

} // namespace SensorHalExt
} // namespace android

#endif // ANDROID_SENSORHAL_EXT_BASE_DYNAMIC_SENSOR_DAEMON_H

