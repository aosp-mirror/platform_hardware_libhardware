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

#ifndef ANDROID_SENSORHAL_EXT_DUMMY_DYNAMIC_ACCEL_DAEMON_H
#define ANDROID_SENSORHAL_EXT_DUMMY_DYNAMIC_ACCEL_DAEMON_H

#include "BaseDynamicSensorDaemon.h"
#include "BaseSensorObject.h"

#include <hardware/sensors.h>
#include <utils/Thread.h>
#include <mutex>
#include <unordered_set>

namespace android {
namespace SensorHalExt {

class ConnectionDetector;

/**
 * This daemon simulates dynamic sensor connection without the need of actually connect peripheral
 * to Android. It is for debugging and testing. It can handle one concurrent connections at maximum.
 */
class DummyDynamicAccelDaemon : public BaseDynamicSensorDaemon {
public:
    DummyDynamicAccelDaemon(DynamicSensorManager& manager);
    virtual ~DummyDynamicAccelDaemon() = default;
private:
    class DummySensor : public BaseSensorObject, public Thread {
    public:
        DummySensor(const std::string &name);
        ~DummySensor();
        virtual const sensor_t* getSensor() const;
        virtual void getUuid(uint8_t* uuid) const;
        virtual int enable(bool enable);
        virtual int batch(nsecs_t sample_period, nsecs_t batch_period);
    private:
        // implement Thread function
        virtual bool threadLoop() override;

        void waitUntilNextSample();

        sensor_t mSensor;
        std::string mSensorName;

        std::mutex mLock;
        std::mutex mRunLock;
        bool mRunState;
    };
    // implement BaseDynamicSensorDaemon function
    virtual BaseSensorVector createSensor(const std::string &deviceKey) override;

    sp<ConnectionDetector> mFileDetector;
    sp<ConnectionDetector> mSocketDetector;
};
} // namespace SensorHalExt
} // namespace android

#endif // ANDROID_SENSORHAL_EXT_DYNAMIC_SENSOR_DAEMON_H

