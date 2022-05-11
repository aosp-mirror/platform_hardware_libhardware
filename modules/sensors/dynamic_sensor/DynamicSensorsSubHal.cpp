/*
 * Copyright (C) 2021 The Android Open Source Project
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
#include "DynamicSensorsSubHal.h"

#include <convertV2_1.h>
#include <hardware/sensors-base.h>
#include <log/log.h>

#include <chrono>
#include <thread>

using ::android::hardware::sensors::V1_0::Result;
using ::android::hardware::sensors::V2_0::implementation::ScopedWakelock;
using ::android::hardware::sensors::V2_1::implementation::convertFromSensorEvent;
using ::android::hardware::sensors::V2_1::SensorInfo;
using ::android::hardware::sensors::V2_1::SensorType;
template<class T> using Return = ::android::hardware::Return<T>;
using ::android::hardware::Void;

namespace android {
namespace SensorHalExt {

static Result ResultFromStatus(status_t err) {
    switch (err) {
        case ::android::OK:
            return Result::OK;
        case ::android::PERMISSION_DENIED:
            return Result::PERMISSION_DENIED;
        case ::android::NO_MEMORY:
            return Result::NO_MEMORY;
        case ::android::BAD_VALUE:
            return Result::BAD_VALUE;
        default:
            return Result::INVALID_OPERATION;
    }
}

DynamicSensorsSubHal::DynamicSensorsSubHal() {
    // initialize dynamic sensor manager
    mDynamicSensorManager.reset(
            DynamicSensorManager::createInstance(kDynamicHandleBase,
                                                 kMaxDynamicHandleCount,
                                                 this /* callback */));
}

// ISensors.
Return<Result> DynamicSensorsSubHal::setOperationMode(OperationMode mode) {
    return (mode == static_cast<OperationMode>(SENSOR_HAL_NORMAL_MODE) ?
            Result::OK : Result::BAD_VALUE);
}

Return<Result> DynamicSensorsSubHal::activate(int32_t sensor_handle,
                                              bool enabled) {
    int rc = mDynamicSensorManager->activate(sensor_handle, enabled);
    return ResultFromStatus(rc);
}

Return<Result> DynamicSensorsSubHal::batch(
        int32_t sensor_handle, int64_t sampling_period_ns,
        int64_t max_report_latency_ns) {
    int rc = mDynamicSensorManager->batch(sensor_handle, sampling_period_ns,
                                          max_report_latency_ns);
    return ResultFromStatus(rc);
}

Return<Result> DynamicSensorsSubHal::flush(int32_t sensor_handle) {
    int rc = mDynamicSensorManager->flush(sensor_handle);
    return ResultFromStatus(rc);
}

Return<void> DynamicSensorsSubHal::registerDirectChannel(
        const SharedMemInfo& mem __unused,
        registerDirectChannel_cb callback __unused) {
    ALOGE("DynamicSensorsSubHal::registerDirectChannel not supported.");

    return Void();
}

Return<Result> DynamicSensorsSubHal::unregisterDirectChannel(
        int32_t channel_handle __unused) {
    ALOGE("DynamicSensorsSubHal::unregisterDirectChannel not supported.");

    return Result::INVALID_OPERATION;
}

Return<void> DynamicSensorsSubHal::configDirectReport(
        int32_t sensor_handle __unused, int32_t channel_handle __unused,
        RateLevel rate __unused, configDirectReport_cb callback __unused) {
    ALOGE("DynamicSensorsSubHal::configDirectReport not supported.");

    return Void();
}

Return<void> DynamicSensorsSubHal::getSensorsList_2_1(
        getSensorsList_2_1_cb callback) {
    const sensor_t& sensor_info = mDynamicSensorManager->getDynamicMetaSensor();
    std::vector<SensorInfo> sensors;

    ALOGD("DynamicSensorsSubHal::getSensorsList_2_1 invoked.");

    // get the dynamic sensor info
    sensors.resize(1);
    sensors[0].sensorHandle = sensor_info.handle;
    sensors[0].name = sensor_info.name;
    sensors[0].vendor = sensor_info.vendor;
    sensors[0].version = 1;
    sensors[0].type = static_cast<SensorType>(sensor_info.type);
    sensors[0].typeAsString = sensor_info.stringType;
    sensors[0].maxRange = sensor_info.maxRange;
    sensors[0].resolution = sensor_info.resolution;
    sensors[0].power = sensor_info.power;
    sensors[0].minDelay = sensor_info.minDelay;
    sensors[0].fifoReservedEventCount = sensor_info.fifoReservedEventCount;
    sensors[0].fifoMaxEventCount = sensor_info.fifoMaxEventCount;
    sensors[0].requiredPermission = sensor_info.requiredPermission;
    sensors[0].maxDelay = sensor_info.maxDelay;
    sensors[0].flags = sensor_info.flags;

    callback(sensors);

    return Void();
}

Return<Result> DynamicSensorsSubHal::injectSensorData_2_1(
        const Event& event __unused) {
    ALOGE("DynamicSensorsSubHal::injectSensorData_2_1 not supported.");

    return Result::INVALID_OPERATION;
}

Return<void> DynamicSensorsSubHal::debug(
        const hidl_handle& handle __unused,
        const hidl_vec<hidl_string>& args __unused) {
    return Void();
}

// ISensorsSubHal.
Return<Result> DynamicSensorsSubHal::initialize(
        const sp<IHalProxyCallback>& hal_proxy_callback) {
    ALOGD("DynamicSensorsSubHal::initialize invoked.");

    mHalProxyCallback = hal_proxy_callback;

    return Result::OK;
}

// SensorEventCallback.
int DynamicSensorsSubHal::submitEvent(SP(BaseSensorObject) sensor,
                                      const sensors_event_t& e) {
    std::vector<Event> events;
    Event hal_event;
    bool wakeup;

    if (e.type == SENSOR_TYPE_DYNAMIC_SENSOR_META) {
        const dynamic_sensor_meta_event_t* sensor_meta;

        sensor_meta = static_cast<const dynamic_sensor_meta_event_t*>(
                &(e.dynamic_sensor_meta));
        if (sensor_meta->connected != 0) {
            // The sensor framework must be notified of the connected sensor
            // through the callback before handling the sensor added event. If
            // it isn't, it will assert when looking up the sensor handle when
            // processing the sensor added event.
            //
            // TODO (b/201529167): Fix dynamic sensors addition / removal when
            //                     converting to AIDL.
            // The sensor framework runs in a separate process from the sensor
            // HAL, and it processes events in a dedicated thread, so it's
            // possible the event handling can be done before the callback is
            // run. Thus, a delay is added after sending notification of the
            // connected sensor.
            onSensorConnected(sensor_meta->handle, sensor_meta->sensor);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
       }
    }

    convertFromSensorEvent(e, &hal_event);
    events.push_back(hal_event);
    if (sensor && sensor->getSensor()) {
        wakeup = sensor->getSensor()->flags & SENSOR_FLAG_WAKE_UP;
    } else {
        wakeup = false;
    }
    ScopedWakelock wakelock = mHalProxyCallback->createScopedWakelock(wakeup);
    mHalProxyCallback->postEvents(events, std::move(wakelock));

    return 0;
}

void DynamicSensorsSubHal::onSensorConnected(
        int handle, const sensor_t* sensor_info) {
    hidl_vec<SensorInfo> sensor_list;

    sensor_list.resize(1);
    sensor_list[0].sensorHandle = handle;
    sensor_list[0].name = sensor_info->name;
    sensor_list[0].vendor = sensor_info->vendor;
    sensor_list[0].version = 1;
    sensor_list[0].type = static_cast<SensorType>(sensor_info->type);
    sensor_list[0].typeAsString = sensor_info->stringType;
    sensor_list[0].maxRange = sensor_info->maxRange;
    sensor_list[0].resolution = sensor_info->resolution;
    sensor_list[0].power = sensor_info->power;
    sensor_list[0].minDelay = sensor_info->minDelay;
    sensor_list[0].fifoReservedEventCount = sensor_info->fifoReservedEventCount;
    sensor_list[0].fifoMaxEventCount = sensor_info->fifoMaxEventCount;
    sensor_list[0].requiredPermission = sensor_info->requiredPermission;
    sensor_list[0].maxDelay = sensor_info->maxDelay;
    sensor_list[0].flags = sensor_info->flags;

    mHalProxyCallback->onDynamicSensorsConnected_2_1(sensor_list);
}

} // namespace SensorHalExt
} // namespace android

using ::android::hardware::sensors::V2_1::implementation::ISensorsSubHal;
ISensorsSubHal* sensorsHalGetSubHal_2_1(uint32_t* version) {
    static android::SensorHalExt::DynamicSensorsSubHal subHal;

    *version = SUB_HAL_2_1_VERSION;
    return &subHal;
}

