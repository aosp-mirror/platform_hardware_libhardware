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

#ifndef ANDROID_SENSORHAL_DSE_SENSOR_EVENT_CALLBACK_H
#define ANDROID_SENSORHAL_DSE_SENSOR_EVENT_CALLBACK_H

#include "Utils.h"
#include <hardware/sensors.h>

namespace android {
namespace SensorHalExt {
class BaseSensorObject;

// if timestamp in sensors_event_t has this value, it will be filled at dispatcher.
constexpr int64_t TIMESTAMP_AUTO_FILL = -1;

class SensorEventCallback {
public:
    virtual int submitEvent(SP(BaseSensorObject) sensor, const sensors_event_t &e) = 0;
    virtual ~SensorEventCallback() = default;
};

} // namespace SensorHalExt
} // namespace android

#endif // ANDROID_SENSORHAL_DSE_SENSOR_EVENT_CALLBACK_H
