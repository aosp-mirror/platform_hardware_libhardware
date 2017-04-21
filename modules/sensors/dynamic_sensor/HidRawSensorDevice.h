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
#ifndef ANDROID_SENSORHAL_EXT_HIDRAW_SENSOR_DEVICE_H
#define ANDROID_SENSORHAL_EXT_HIDRAW_SENSOR_DEVICE_H

#include "BaseSensorObject.h"
#include "BaseDynamicSensorDaemon.h" // BaseSensorVector
#include "HidRawDevice.h"
#include "HidRawSensor.h"

#include <HidParser.h>
#include <utils/Thread.h>
#include <string>
#include <vector>

namespace android {
namespace SensorHalExt {

class HidRawSensorDevice : public HidRawDevice, public Thread {
public:
    static sp<HidRawSensorDevice> create(const std::string &devName);
    virtual ~HidRawSensorDevice();

    // get a list of sensors associated with this device
    BaseSensorVector getSensors() const;
private:
    static const std::unordered_set<unsigned int> sInterested;

    // constructor will result in +1 strong count
    explicit HidRawSensorDevice(const std::string &devName);
    // implement function of Thread
    virtual bool threadLoop() override;
    std::unordered_map<unsigned int/*reportId*/, sp<HidRawSensor>> mSensors;
    bool mValid;
};

} // namespace SensorHalExt
} // namespace android

#endif // ANDROID_SENSORHAL_EXT_HIDRAW_DEVICE_H

