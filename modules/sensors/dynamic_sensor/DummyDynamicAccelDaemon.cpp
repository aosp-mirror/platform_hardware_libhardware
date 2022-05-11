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

#include "BaseSensorObject.h"
#include "ConnectionDetector.h"
#include "DummyDynamicAccelDaemon.h"
#include "DynamicSensorManager.h"

#include <cutils/properties.h>
#include <utils/Log.h>
#include <utils/SystemClock.h>
#include <utils/misc.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <algorithm>            //std::max

#define SYSPROP_PREFIX                  "vendor.dynamic_sensor.mock"
#define FILE_NAME_BASE                  "dummy_accel_file"
#define FILE_NAME_REGEX                 ("^" FILE_NAME_BASE "[0-9]$")

namespace android {
namespace SensorHalExt {

DummyDynamicAccelDaemon::DummyDynamicAccelDaemon(DynamicSensorManager& manager)
        : BaseDynamicSensorDaemon(manager) {
    char property[PROPERTY_VALUE_MAX+1];

    property_get(SYSPROP_PREFIX ".file", property, "");
    if (strcmp(property, "") != 0) {
        mFileDetector = new FileConnectionDetector(
                this, std::string(property), std::string(FILE_NAME_REGEX));
        mFileDetector->Init();
    }

    property_get(SYSPROP_PREFIX ".socket", property, "");
    if (strcmp(property, "") != 0) {
        mSocketDetector = new SocketConnectionDetector(this, atoi(property));
        mSocketDetector->Init();
    }
}

BaseSensorVector DummyDynamicAccelDaemon::createSensor(const std::string &deviceKey) {
    BaseSensorVector ret;
    if (deviceKey.compare(0, 1, "/") == 0) {
        // file detector result, deviceKey is file absolute path
        const size_t len = ::strlen(FILE_NAME_BASE) + 1; // +1 for number
        if (deviceKey.length() < len) {
            ALOGE("illegal file device key %s", deviceKey.c_str());
        } else {
            size_t start = deviceKey.length() - len;
            ret.emplace_back(new DummySensor(deviceKey.substr(start)));
        }
    } else if (deviceKey.compare(0, ::strlen("socket:"), "socket:") == 0) {
        ret.emplace_back(new DummySensor(deviceKey));
    } else {
        // unknown deviceKey
        ALOGE("unknown deviceKey: %s", deviceKey.c_str());
    }
    return ret;
}

DummyDynamicAccelDaemon::DummySensor::DummySensor(const std::string &name)
        : Thread(false /*canCallJava*/), mRunState(false) {
    mSensorName = "Dummy Accel - " + name;
    // fake sensor information for dummy sensor
    mSensor = (struct sensor_t) {
        mSensorName.c_str(),
        "DemoSense, Inc.",
        1,                                         // version
        -1,                                        // handle, dummy number here
        SENSOR_TYPE_ACCELEROMETER,
        9.8 * 8.0f,                                // maxRange
        9.8 * 8.0f / 32768.0f,                     // resolution
        0.5f,                                      // power
        (int32_t)(1.0E6f / 50),                    // minDelay
        0,                                         // fifoReservedEventCount
        0,                                         // fifoMaxEventCount
        SENSOR_STRING_TYPE_ACCELEROMETER,
        "",                                        // requiredPermission
        (long)(1.0E6f / 50),                       // maxDelay
        SENSOR_FLAG_CONTINUOUS_MODE,
        { NULL, NULL }
    };
    mRunLock.lock();
    run("DummySensor");
}

DummyDynamicAccelDaemon::DummySensor::~DummySensor() {
    requestExitAndWait();
    // unlock mRunLock so thread can be unblocked
    mRunLock.unlock();
}

const sensor_t* DummyDynamicAccelDaemon::DummySensor::getSensor() const {
    return &mSensor;
}

void DummyDynamicAccelDaemon::DummySensor::getUuid(uint8_t* uuid) const {
    // at maximum, there will be always one instance, so we can hardcode
    size_t hash = std::hash<std::string>()(mSensorName);
    memset(uuid, 'x', 16);
    memcpy(uuid, &hash, sizeof(hash));
}

int DummyDynamicAccelDaemon::DummySensor::enable(bool enable) {
    std::lock_guard<std::mutex> lk(mLock);
    if (mRunState != enable) {
        if (enable) {
            mRunLock.unlock();
        } else {
            mRunLock.lock();
        }
        mRunState = enable;
    }
    return 0;
}

int DummyDynamicAccelDaemon::DummySensor::batch(int64_t /*samplePeriod*/, int64_t /*batchPeriod*/) {
    // Dummy sensor does not support changing rate and batching. But return successful anyway.
    return 0;
}

void DummyDynamicAccelDaemon::DummySensor::waitUntilNextSample() {
    // block when disabled (mRunLock locked)
    mRunLock.lock();
    mRunLock.unlock();

    if (!Thread::exitPending()) {
        // sleep 20 ms (50Hz)
        usleep(20000);
    }
}

bool DummyDynamicAccelDaemon::DummySensor::threadLoop() {
    // designated intialization will leave the unspecified fields zeroed
    sensors_event_t event = {
        .version = sizeof(event),
        .sensor = -1,
        .type = SENSOR_TYPE_ACCELEROMETER,
    };

    int64_t startTimeNs = elapsedRealtimeNano();

    ALOGI("Dynamic Dummy Accel started for sensor %s", mSensorName.c_str());
    while (!Thread::exitPending()) {
        waitUntilNextSample();

        if (Thread::exitPending()) {
            break;
        }
        int64_t nowTimeNs = elapsedRealtimeNano();
        float t = (nowTimeNs - startTimeNs) / 1e9f;

        event.data[0] = 2 * ::sin(3 * M_PI * t);
        event.data[1] = 3 * ::cos(3 * M_PI * t);
        event.data[2] = 1.5 * ::sin(6 * M_PI * t);
        event.timestamp = nowTimeNs;
        generateEvent(event);
    }

    ALOGI("Dynamic Dummy Accel thread ended for sensor %s", mSensorName.c_str());
    return false;
}

} // namespace SensorHalExt
} // namespace android

