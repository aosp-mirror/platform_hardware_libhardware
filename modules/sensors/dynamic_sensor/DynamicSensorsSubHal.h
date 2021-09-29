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

#ifndef ANDROID_SENSORHAL_EXT_DYNAMIC_SENSORS_SUB_HAL_H
#define ANDROID_SENSORHAL_EXT_DYNAMIC_SENSORS_SUB_HAL_H

#include "DynamicSensorManager.h"

#include <V2_1/SubHal.h>

namespace android {
namespace SensorHalExt {

class DynamicSensorsSubHal :
        public SensorEventCallback,
        public ::android::hardware::sensors::V2_1::implementation::ISensorsSubHal {
    using Event = ::android::hardware::sensors::V2_1::Event;
    using hidl_handle = ::android::hardware::hidl_handle;
    using hidl_string = ::android::hardware::hidl_string;
    template<class T> using hidl_vec = ::android::hardware::hidl_vec<T>;
    using IHalProxyCallback =
        ::android::hardware::sensors::V2_1::implementation::IHalProxyCallback;
    using OperationMode = ::android::hardware::sensors::V1_0::OperationMode;
    using RateLevel = ::android::hardware::sensors::V1_0::RateLevel;
    using Result = ::android::hardware::sensors::V1_0::Result;
    template<class T> using Return = ::android::hardware::Return<T>;
    using SharedMemInfo = ::android::hardware::sensors::V1_0::SharedMemInfo;

public:
    DynamicSensorsSubHal();

    // ISensors.
    Return<Result> setOperationMode(OperationMode mode) override;
    Return<Result> activate(int32_t sensor_handle, bool enabled) override;
    Return<Result> batch(int32_t sensor_handle, int64_t sampling_period_ns,
                         int64_t max_report_latency_ns) override;
    Return<Result> flush(int32_t sensor_handle) override;
    Return<void> registerDirectChannel(
            const SharedMemInfo& mem,
            registerDirectChannel_cb callback) override;
    Return<Result> unregisterDirectChannel(int32_t channel_handle) override;
    Return<void> configDirectReport(
            int32_t sensor_handle, int32_t channel_handle, RateLevel rate,
            configDirectReport_cb callback) override;
    Return<void> getSensorsList_2_1(getSensorsList_2_1_cb callback) override;
    Return<Result> injectSensorData_2_1(const Event& event) override;
    Return<void> debug(
            const hidl_handle& handle,
            const hidl_vec<hidl_string>& args) override;

    // ISensorsSubHal.
    const std::string getName() override { return "Dynamic-SubHAL"; }
    Return<Result> initialize(
            const sp<IHalProxyCallback>& hal_proxy_callback) override;

    // SensorEventCallback.
    int submitEvent(SP(BaseSensorObject) sensor,
                    const sensors_event_t& e) override;

private:
    static constexpr int32_t kDynamicHandleBase = 0;
    static constexpr int32_t kDynamicHandleEnd = 0x1000000;
    static constexpr int32_t kMaxDynamicHandleCount = kDynamicHandleEnd -
                                                      kDynamicHandleBase;

    void onSensorConnected(int handle, const sensor_t* sensor_info);

    std::unique_ptr<DynamicSensorManager> mDynamicSensorManager;
    sp<IHalProxyCallback> mHalProxyCallback;
};

} // namespace SensorHalExt
} // namespace android

#endif // ANDROID_SENSORHAL_EXT_DYNAMIC_SENSORS_SUB_HAL_H

