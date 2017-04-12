/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef SENSORS_H_
#define SENSORS_H_

#include <hardware/hardware.h>
#include <hardware/sensors.h>
#include <media/stagefright/foundation/ABase.h>
#include <utils/RefBase.h>

#include <memory>
#include <unordered_set>
#include <vector>

namespace android {
    namespace SensorHalExt {
        class DynamicSensorManager;
    } // namespace BaseSensorObject
} // namespace android

using android::SensorHalExt::DynamicSensorManager;

class SensorContext {
public:
    struct sensors_poll_device_1 device;

    explicit SensorContext(const struct hw_module_t *module);

    size_t getSensorList(sensor_t const **list);

private:

    int close();
    int activate(int handle, int enabled);
    int setDelay(int handle, int64_t delayNs);
    int poll(sensors_event_t *data, int count);

    int batch(int handle, int64_t sampling_period_ns,
              int64_t max_report_latency_ns);

    int flush(int handle);

    // static wrappers
    static int CloseWrapper(struct hw_device_t *dev);

    static int ActivateWrapper(
            struct sensors_poll_device_t *dev, int handle, int enabled);

    static int SetDelayWrapper(
            struct sensors_poll_device_t *dev, int handle, int64_t delayNs);

    static int PollWrapper(
            struct sensors_poll_device_t *dev, sensors_event_t *data, int count);

    static int BatchWrapper(
            struct sensors_poll_device_1 *dev,
            int handle,
            int flags,
            int64_t sampling_period_ns,
            int64_t max_report_latency_ns);

    static int FlushWrapper(struct sensors_poll_device_1 *dev, int handle);

    static constexpr int32_t kDynamicHandleBase = 0x10000;
    static constexpr int32_t kDynamicHandleEnd = 0x1000000;
    static constexpr int32_t kMaxDynamicHandleCount = kDynamicHandleEnd - kDynamicHandleBase;

    std::unique_ptr<DynamicSensorManager> mDynamicSensorManager;

    DISALLOW_EVIL_CONSTRUCTORS(SensorContext);
};

#endif  // SENSORS_H_
