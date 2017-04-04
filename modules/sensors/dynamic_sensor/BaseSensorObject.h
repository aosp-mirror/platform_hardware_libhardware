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

#ifndef ANDROID_SENSORHAL_BASE_SENSOR_OBJECT_H
#define ANDROID_SENSORHAL_BASE_SENSOR_OBJECT_H

#include "Utils.h"
#include <cstdint>

struct sensor_t;
struct sensors_event_t;

namespace android {
namespace SensorHalExt {

class SensorEventCallback;

class BaseSensorObject : virtual public REF_BASE(BaseSensorObject) {
public:
    BaseSensorObject();
    virtual ~BaseSensorObject() = default;

    // always called by DynamicSensorManager, callback must point to
    // valid object throughout life cycle of BaseSensorObject
    bool setEventCallback(SensorEventCallback* callback);

    // virtual functions to get sensor information and operate sensor
    virtual const sensor_t* getSensor() const = 0;

    // get uuid of sensor, default implementation set it to all zero, means does not have a uuid.
    virtual void getUuid(uint8_t* uuid) const;

    // enable sensor
    virtual int enable(bool enable) = 0;

    // set sample period and batching period of sensor.
    // both sample period and batch period are in nano-seconds.
    virtual int batch(int64_t samplePeriod, int64_t batchPeriod) = 0;

    // flush sensor, default implementation will send a flush complete event back.
    virtual int flush();

protected:
    // utility function for sub-class
    void generateEvent(const sensors_event_t &e);
private:
    SensorEventCallback* mCallback;
};

} // namespace SensorHalExt
} // namespace android

#endif // ANDROID_SENSORHAL_BASE_SENSOR_OBJECT_H

