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
#include "HidRawSensorDaemon.h"
#include "ConnectionDetector.h"
#include "DynamicSensorManager.h"
#include "HidRawSensorDevice.h"

#include <utils/Log.h>
#include <utils/SystemClock.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <iomanip>
#include <sstream>

#define DEV_PATH                "/dev/"
#define DEV_NAME_REGEX          "^hidraw[0-9]+$"

namespace android {
namespace SensorHalExt {

HidRawSensorDaemon::HidRawSensorDaemon(DynamicSensorManager& manager)
        : BaseDynamicSensorDaemon(manager) {
    mDetector = new FileConnectionDetector(
            this, std::string(DEV_PATH), std::string(DEV_NAME_REGEX));
    mDetector->Init();
}

BaseSensorVector HidRawSensorDaemon::createSensor(const std::string &deviceKey) {
    BaseSensorVector ret;
    sp<HidRawSensorDevice> device(HidRawSensorDevice::create(deviceKey));

    if (device != nullptr) {
        ALOGV("created HidRawSensorDevice(%p) successfully on device %s contains %zu sensors",
              device.get(), deviceKey.c_str(), device->getSensors().size());

        // convert type
        for (auto &i : device->getSensors()) {
            ret.push_back(i);
        }
        mHidRawSensorDevices.emplace(deviceKey, device);
    } else {
        ALOGE("failed to create HidRawSensorDevice object");
    }

    ALOGE("return %zu sensors", ret.size());
    return ret;
}

void HidRawSensorDaemon::removeSensor(const std::string &deviceKey) {
    mHidRawSensorDevices.erase(deviceKey);
}

} // namespace SensorHalExt
} // namespace android

