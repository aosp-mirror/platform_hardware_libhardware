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
#include "BaseSensorObject.h"
#include "DummyDynamicAccelDaemon.h"
#include "HidRawSensorDaemon.h"
#include "DynamicSensorManager.h"

#include <utils/Log.h>
#include <utils/SystemClock.h>

#include <cassert>

namespace android {
namespace SensorHalExt {

DynamicSensorManager* DynamicSensorManager::createInstance(
        int handleBase, int handleCount, SensorEventCallback *callback) {
    auto m = new DynamicSensorManager(handleBase, handleBase + handleCount - 1, callback);
    m->mDaemonVector.push_back(new DummyDynamicAccelDaemon(*m));
    m->mDaemonVector.push_back(new HidRawSensorDaemon(*m));
    return m;
}

DynamicSensorManager::DynamicSensorManager(
        int handleBase, int handleMax, SensorEventCallback* callback) :
        mHandleRange(handleBase, handleMax),
        mCallback(callback),
        mFifo(callback ? 0 : kFifoSize),
        mNextHandle(handleBase+1) {
    assert(handleBase > 0 && handleMax > handleBase + 1); // handleBase is reserved

    mMetaSensor = (const sensor_t) {
        "Dynamic Sensor Manager",
        "Google",
        1,                                          // version
        handleBase,                                 // handle
        SENSOR_TYPE_DYNAMIC_SENSOR_META,
        1,                                          // maxRange
        1,                                          // resolution
        1e-6f,                                      // power, very small number instead of 0
                                                    // to avoid sigularity in app
        (int32_t)(1000),                            // minDelay
        0,                                          // fifoReservedEventCount
        0,                                          // fifoMaxEventCount
        SENSOR_STRING_TYPE_DYNAMIC_SENSOR_META,
        "",                                         // requiredPermission
        (long)(1000),                               // maxDelay
        SENSOR_FLAG_SPECIAL_REPORTING_MODE | SENSOR_FLAG_WAKE_UP,
        { NULL, NULL }
    };
}

DynamicSensorManager::~DynamicSensorManager() {
    // free all daemons first
    mDaemonVector.clear();
}

bool DynamicSensorManager::owns(int handle) const {
    return handle >= mHandleRange.first && handle < mHandleRange.second;
}

int DynamicSensorManager::activate(int handle, bool enable) {
    if (handle == mHandleRange.first) {
        // ignored
        return 0;
    }

    // in case there is a pending report, now it is time to remove it as it is no longer necessary.
    {
        std::lock_guard<std::mutex> lk(mLock);
        mPendingReport.erase(handle);
    }

    return operateSensor(handle,
            [&enable] (sp<BaseSensorObject> s)->int {
                return s->enable(enable);
            });
}

int DynamicSensorManager::batch(int handle, nsecs_t sample_period, nsecs_t batch_period) {
    if (handle == mHandleRange.first) {
        // ignored
        return 0;
    }
    return operateSensor(handle,
            [&sample_period, &batch_period] (sp<BaseSensorObject> s)->int {
                return s->batch(sample_period, batch_period);
            });
}

int DynamicSensorManager::setDelay(int handle, nsecs_t sample_period) {
    return batch(handle, sample_period, 0);
}

int DynamicSensorManager::flush(int handle) {
    if (handle == mHandleRange.first) {
        // submit a flush complete here
        static const sensors_event_t event = {
            .type = SENSOR_TYPE_META_DATA,
            .sensor = mHandleRange.first,
            .timestamp = TIMESTAMP_AUTO_FILL,  // timestamp will be filled at dispatcher
        };
        submitEvent(nullptr, event);
        return 0;
    }
    return operateSensor(handle, [] (sp<BaseSensorObject> s)->int {return s->flush();});
}

int DynamicSensorManager::poll(sensors_event_t * data, int count) {
    assert(mCallback == nullptr);
    std::lock_guard<std::mutex> lk(mFifoLock);
    return mFifo.read(data, count);
}

bool DynamicSensorManager::registerSensor(sp<BaseSensorObject> sensor) {
    std::lock_guard<std::mutex> lk(mLock);
    if (mReverseMap.find(sensor.get()) != mReverseMap.end()) {
        ALOGE("trying to add the same sensor twice, ignore");
        return false;
    }
    int handle = getNextAvailableHandle();
    if (handle < 0) {
        ALOGE("Running out of handle, quit.");
        return false;
    }

    // these emplace will always be successful
    mMap.emplace(handle, sensor);
    mReverseMap.emplace(sensor.get(), handle);
    sensor->setEventCallback(this);

    auto entry = mPendingReport.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(handle),
            std::forward_as_tuple(handle, sensor));
    if (entry.second) {
        submitEvent(nullptr, entry.first->second.generateConnectionEvent(mHandleRange.first));
    }
    return entry.second;
}

void DynamicSensorManager::unregisterSensor(sp<BaseSensorObject> sensor) {
    std::lock_guard<std::mutex> lk(mLock);
    auto i = mReverseMap.find(sensor.get());
    if (i == mReverseMap.end()) {
        ALOGE("cannot remove a non-exist sensor");
        return;
    }
    int handle = i->second;
    mReverseMap.erase(i);
    mMap.erase(handle);

    // will not clean up mPendingReport here, it will be cleaned up when at first activate call.
    // sensorservice is guranteed to call activate upon arrival of dynamic sensor meta connection
    // event.

    // send disconnection event
    sensors_event_t event;
    ConnectionReport::fillDisconnectionEvent(&event, mHandleRange.first, handle);
    submitEvent(nullptr, event);
}

int DynamicSensorManager::submitEvent(sp<BaseSensorObject> source, const sensors_event_t &e) {
    int handle;
    if (source == nullptr) {
        handle = mHandleRange.first;
    } else {
        std::lock_guard<std::mutex> lk(mLock);
        auto i = mReverseMap.find(source.get());
        if (i == mReverseMap.end()) {
            ALOGE("cannot submit event for sensor that has not been registered");
            return NAME_NOT_FOUND;
        }
        handle = i->second;
    }

    // making a copy of events, prepare for editing
    sensors_event_t event = e;
    event.version = sizeof(event);

    // special case of flush complete
    if (event.type == SENSOR_TYPE_META_DATA) {
        event.sensor = 0;
        event.meta_data.sensor = handle;
    } else {
        event.sensor = handle;
    }

    // set timestamp if it is default value
    if (event.timestamp == TIMESTAMP_AUTO_FILL) {
        event.timestamp = elapsedRealtimeNano();
    }

    if (mCallback) {
        // extention mode, calling callback directly
        int ret;

        ret = mCallback->submitEvent(nullptr, event);
        if (ret < 0) {
            ALOGE("DynamicSensorManager callback failed, ret: %d", ret);
        }
    } else {
        // standalone mode, add event to internal buffer for poll() to pick up
        std::lock_guard<std::mutex> lk(mFifoLock);
        if (mFifo.write(&event, 1) < 0) {
            ALOGE("DynamicSensorManager fifo full");
        }
    }
    return 0;
}

int DynamicSensorManager::getNextAvailableHandle() {
    if (mNextHandle == mHandleRange.second) {
        return -1;
    }
    return mNextHandle++;
}

const sensor_t& DynamicSensorManager::getDynamicMetaSensor() const {
    return mMetaSensor;
}

DynamicSensorManager::ConnectionReport::ConnectionReport(
        int handle, sp<BaseSensorObject> sensor) :
        mSensor(*(sensor->getSensor())),
        mName(mSensor.name),
        mVendor(mSensor.vendor),
        mPermission(mSensor.requiredPermission),
        mStringType(mSensor.stringType),
        mGenerated(false) {
    mSensor.name = mName.c_str();
    mSensor.vendor = mVendor.c_str();
    mSensor.requiredPermission = mPermission.c_str();
    mSensor.stringType = mStringType.c_str();
    mSensor.handle = handle;
    memset(&mEvent, 0, sizeof(mEvent));
    mEvent.version = sizeof(mEvent);
    sensor->getUuid(mUuid);
    ALOGV("Connection report init: name = %s, handle = %d", mSensor.name, mSensor.handle);
}

DynamicSensorManager::ConnectionReport::~ConnectionReport() {
    ALOGV("Connection report dtor: name = %s, handle = %d", mSensor.name, mSensor.handle);
}

const sensors_event_t& DynamicSensorManager::ConnectionReport::
        generateConnectionEvent(int metaHandle) {
    if (!mGenerated) {
        mEvent.sensor = metaHandle;
        mEvent.type = SENSOR_TYPE_DYNAMIC_SENSOR_META;
        mEvent.timestamp = elapsedRealtimeNano();
        mEvent.dynamic_sensor_meta =
                (dynamic_sensor_meta_event_t) {true, mSensor.handle, &mSensor, {0}};
        memcpy(&mEvent.dynamic_sensor_meta.uuid, &mUuid, sizeof(mEvent.dynamic_sensor_meta.uuid));
        mGenerated = true;
    }
    return mEvent;
}

void DynamicSensorManager::ConnectionReport::
        fillDisconnectionEvent(sensors_event_t* event, int metaHandle, int handle) {
    memset(event, 0, sizeof(sensors_event_t));
    event->version = sizeof(sensors_event_t);
    event->sensor = metaHandle;
    event->type = SENSOR_TYPE_DYNAMIC_SENSOR_META;
    event->timestamp = elapsedRealtimeNano();
    event->dynamic_sensor_meta.connected = false;
    event->dynamic_sensor_meta.handle = handle;
}

} // namespace SensorHalExt
} // namespace android
