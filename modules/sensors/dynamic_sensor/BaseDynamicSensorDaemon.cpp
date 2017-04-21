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

#include "BaseDynamicSensorDaemon.h"
#include "DynamicSensorManager.h"
#include "utils/Log.h"

namespace android {
namespace SensorHalExt {

bool BaseDynamicSensorDaemon::onConnectionChange(const std::string &deviceKey, bool connected) {
    bool ret = false;
    auto i = mDeviceKeySensorMap.find(deviceKey);
    if (connected) {
        if (i == mDeviceKeySensorMap.end()) {
            ALOGV("device %s is connected", deviceKey.c_str());
            // get sensors from implementation
            BaseSensorVector sensors = createSensor(deviceKey);
            if (sensors.empty()) {
                ALOGI("no valid sensor is defined in device %s, ignore", deviceKey.c_str());
            } else {
                ALOGV("discovered %zu sensors from device", sensors.size());
                // build internal table first
                auto result = mDeviceKeySensorMap.emplace(deviceKey, std::move(sensors));
                // then register sensor to dynamic sensor manager, result.first is the iterator
                // of key-value pair; result.first->second is the value, which is s.
                for (auto &i : result.first->second) {
                    mManager.registerSensor(i);
                }
                ALOGV("device %s is registered", deviceKey.c_str());
                ret = true;
            }
        } else {
            ALOGD("device %s already added and is connected again, ignore", deviceKey.c_str());
        }
    } else {
        ALOGV("device %s is disconnected", deviceKey.c_str());
        if (i != mDeviceKeySensorMap.end()) {
            BaseSensorVector sensors = i->second;
            for (auto &sensor : sensors) {
                mManager.unregisterSensor(sensor);
            }
            mDeviceKeySensorMap.erase(i);
            // notify implementation
            removeSensor(deviceKey);
            ALOGV("device %s is unregistered", deviceKey.c_str());
            ret = true;
        } else {
            ALOGV("device not found in registry");
        }
    }

    return ret;
}
} // namespace SensorHalExt
} // namespace android

