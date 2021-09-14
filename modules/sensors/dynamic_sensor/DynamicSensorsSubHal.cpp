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

#include "DynamicSensorsSubHal.h"

#include <cutils/properties.h>
#include <hardware/sensors-base.h>
#include <log/log.h>

using ::android::hardware::sensors::V1_0::Result;
using ::android::hardware::sensors::V2_1::SensorInfo;
using ::android::hardware::sensors::V2_1::SensorType;
template<class T> using Return = ::android::hardware::Return<T>;
using ::android::hardware::Void;

namespace android {
namespace SensorHalExt {

DynamicSensorsSubHal::DynamicSensorsSubHal() {
    // initialize dynamic sensor manager
    int32_t base = property_get_int32("sensor.dynamic_sensor_hal.handle_base",
                                      kDynamicHandleBase);
    int32_t count = property_get_int32("sensor.dynamic_sensor_hal.handle_count",
                                       kMaxDynamicHandleCount);
    mDynamicSensorManager.reset(
            DynamicSensorManager::createInstance(base, count,
                                                 nullptr /* callback */));
}

// ISensors.
Return<Result> DynamicSensorsSubHal::setOperationMode(OperationMode mode) {
    return (mode == static_cast<OperationMode>(SENSOR_HAL_NORMAL_MODE) ?
            Result::OK : Result::BAD_VALUE);
}

Return<Result> DynamicSensorsSubHal::activate(int32_t sensor_handle __unused,
                                              bool enabled __unused) {
    ALOGE("DynamicSensorsSubHal::activate not supported.");

    return Result::INVALID_OPERATION;
}

Return<Result> DynamicSensorsSubHal::batch(
        int32_t sensor_handle __unused, int64_t sampling_period_ns __unused,
        int64_t max_report_latency_ns __unused) {
    ALOGE("DynamicSensorsSubHal::batch not supported.");

    return Result::INVALID_OPERATION;
}

Return<Result> DynamicSensorsSubHal::flush(int32_t sensor_handle __unused) {
    ALOGE("DynamicSensorsSubHal::flush not supported.");

    return Result::INVALID_OPERATION;
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
        const sp<IHalProxyCallback>& hal_proxy_callback __unused) {
    ALOGD("DynamicSensorsSubHal::initialize invoked.");

    return Result::OK;
}

} // namespace SensorHalExt
} // namespace android

using ::android::hardware::sensors::V2_1::implementation::ISensorsSubHal;
ISensorsSubHal* sensorsHalGetSubHal_2_1(uint32_t* version) {
    static android::SensorHalExt::DynamicSensorsSubHal subHal;

    *version = SUB_HAL_2_1_VERSION;
    return &subHal;
}

