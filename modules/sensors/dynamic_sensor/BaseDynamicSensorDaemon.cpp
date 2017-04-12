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
    auto i = mDevices.find(deviceKey);
    if (connected) {
        if (i == mDevices.end()) {
            ALOGV("device %s is connected", deviceKey.c_str());
            BaseSensorObject* s = createSensor(deviceKey);
            if (s) {
                mDevices.emplace(deviceKey, sp<BaseSensorObject>(s));
                mManager.registerSensor(s);
                ALOGV("device %s is registered", deviceKey.c_str());
                ret = true;
            }
        } else {
            ALOGD("device %s already added and is connected again, ignore", deviceKey.c_str());
        }
    } else {
        ALOGV("device %s is disconnected", deviceKey.c_str());
        if (i != mDevices.end()) {
            mManager.unregisterSensor(i->second.get());
            mDevices.erase(i);
            ALOGV("device %s is unregistered", deviceKey.c_str());
            ret = true;
        } else {
            ALOGD("device not found in registry");
        }
    }

    return ret;
}
} // namespace SensorHalExt
} // namespace android

