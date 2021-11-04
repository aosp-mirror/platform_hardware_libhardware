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

#ifndef ANDROID_SENSORHAL_EXT_DYNAMIC_SENSOR_MANAGER_H
#define ANDROID_SENSORHAL_EXT_DYNAMIC_SENSOR_MANAGER_H

#include "SensorEventCallback.h"
#include "RingBuffer.h"
#include <hardware/sensors.h>
#include <utils/RefBase.h>

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace android {
namespace SensorHalExt {

class BaseDynamicSensorDaemon;

class DynamicSensorManager : public SensorEventCallback {
public:
    // handleBase is reserved for the dynamic sensor meta sensor.
    // handleMax must be greater than handleBase + 1.
    // This class has two operation mode depending on callback: 1) extension, 2) stand-alone.
    // In extension mode, callback must not be nullptr. Sensor event generated will be submitted to
    // buffer of primary sensor HAL implementation. In stand-alone mode, callback must be nullptr.
    // Generated sensor events will be added into internal buffer waiting for poll() function to
    // pick up.
    //
    static DynamicSensorManager* createInstance(
            int handleBase, int handleCount, SensorEventCallback *callback);
    virtual ~DynamicSensorManager();

    // calls to add or remove sensor, called from sensor daemon
    bool registerSensor(sp<BaseSensorObject> sensor);
    void unregisterSensor(sp<BaseSensorObject> sensor);

    // Determine if a sensor handle is in the range defined in constructor.
    // It does not test if sensor handle is valid.
    bool owns(int handle) const;

    // handles sensor hal requests.
    int activate(int handle, bool enable);
    int batch(int handle, nsecs_t sample_period, nsecs_t batch_period);
    int setDelay(int handle, nsecs_t sample_period);
    int flush(int handle);
    int poll(sensors_event_t * data, int count);

    // SensorEventCallback
    virtual int submitEvent(sp<BaseSensorObject>, const sensors_event_t &e) override;

    // get meta sensor struct
    const sensor_t& getDynamicMetaSensor() const;
protected:
    DynamicSensorManager(int handleBase, int handleMax, SensorEventCallback* callback);
private:
    // a helper class used for generate connection and disconnection report
    class ConnectionReport {
    public:
        ConnectionReport() {}
        ConnectionReport(int handle, sp<BaseSensorObject> sensor);
        ~ConnectionReport();
        const sensors_event_t& generateConnectionEvent(int metaHandle);
        static void fillDisconnectionEvent(sensors_event_t* event, int metaHandle, int handle);
    private:
        sensor_t mSensor;
        std::string mName;
        std::string mVendor;
        std::string mPermission;
        std::string mStringType;
        sensors_event_t mEvent;
        uint8_t mUuid[16];
        bool mGenerated;
        DISALLOW_EVIL_CONSTRUCTORS(ConnectionReport);
    };

    // returns next available handle to use upon a new sensor connection, or -1 if we run out.
    int getNextAvailableHandle();

    // TF:  int foo(sp<BaseSensorObject> obj);
    template <typename TF>
    int operateSensor(int handle, TF f) const {
        sp<BaseSensorObject> s;
        {
            std::lock_guard<std::mutex> lk(mLock);
            const auto i = mMap.find(handle);
            if (i == mMap.end()) {
                return BAD_VALUE;
            }
            s = i->second.promote();
            if (s == nullptr) {
                // sensor object is already gone
                return BAD_VALUE;
            }
        }
        return f(s);
    }

    // available sensor handle space
    const std::pair<int, int> mHandleRange;
    sensor_t mMetaSensor;
    bool mMetaSensorActive = false;

    // immutable pointer to event callback, used in extention mode.
    SensorEventCallback * const mCallback;

    // RingBuffer used in standalone mode
    static constexpr size_t kFifoSize = 4096; //4K events
    mutable std::mutex mFifoLock;
    RingBuffer mFifo;

    // mapping between handle and SensorObjects
    mutable std::mutex mLock;
    int mNextHandle;
    std::unordered_map<int, wp<BaseSensorObject>> mMap;
    std::unordered_map<void *, int> mReverseMap;
    mutable std::unordered_map<int, ConnectionReport> mPendingReport;

    // daemons
    std::vector<sp<BaseDynamicSensorDaemon>> mDaemonVector;
};

} // namespace SensorHalExt
} // namespace android

#endif // ANDROID_SENSORHAL_EXT_DYNAMIC_SENSOR_MANAGER_H
