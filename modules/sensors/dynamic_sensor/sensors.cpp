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


#include "DynamicSensorManager.h"
#include "sensors.h"

#include <cutils/properties.h>
#include <media/stagefright/foundation/ADebug.h>
#include <utils/Log.h>

#include <errno.h>
#include <string.h>
using namespace android;

////////////////////////////////////////////////////////////////////////////////

SensorContext::SensorContext(const struct hw_module_t *module) {
    memset(&device, 0, sizeof(device));

    device.common.tag = HARDWARE_DEVICE_TAG;
    device.common.version = SENSORS_DEVICE_API_VERSION_1_3;
    device.common.module = const_cast<hw_module_t *>(module);
    device.common.close = CloseWrapper;
    device.activate = ActivateWrapper;
    device.setDelay = SetDelayWrapper;
    device.poll = PollWrapper;
    device.batch = BatchWrapper;
    device.flush = FlushWrapper;

    // initialize dynamic sensor manager
    int32_t base = property_get_int32("sensor.dynamic_sensor_hal.handle_base", kDynamicHandleBase);
    int32_t count =
            property_get_int32("sensor.dynamic_sensor_hal.handle_count", kMaxDynamicHandleCount);
    mDynamicSensorManager.reset(DynamicSensorManager::createInstance(base, count, nullptr));
}

int SensorContext::close() {
    delete this;
    return 0;
}

int SensorContext::activate(int handle, int enabled) {
    return mDynamicSensorManager->activate(handle, enabled);
}

int SensorContext::setDelay(int handle, int64_t delayNs) {
    return mDynamicSensorManager->setDelay(handle, delayNs);
}

int SensorContext::poll(sensors_event_t *data, int count) {
    return mDynamicSensorManager->poll(data, count);
}

int SensorContext::batch(
        int handle,
        int64_t sampling_period_ns,
        int64_t max_report_latency_ns) {
    return mDynamicSensorManager->batch(handle, sampling_period_ns, max_report_latency_ns);
}

int SensorContext::flush(int handle) {
    return mDynamicSensorManager->flush(handle);
}

// static
int SensorContext::CloseWrapper(struct hw_device_t *dev) {
    return reinterpret_cast<SensorContext *>(dev)->close();
}

// static
int SensorContext::ActivateWrapper(
        struct sensors_poll_device_t *dev, int handle, int enabled) {
    return reinterpret_cast<SensorContext *>(dev)->activate(handle, enabled);
}

// static
int SensorContext::SetDelayWrapper(
        struct sensors_poll_device_t *dev, int handle, int64_t delayNs) {
    return reinterpret_cast<SensorContext *>(dev)->setDelay(handle, delayNs);
}

// static
int SensorContext::PollWrapper(
        struct sensors_poll_device_t *dev, sensors_event_t *data, int count) {
    return reinterpret_cast<SensorContext *>(dev)->poll(data, count);
}

// static
int SensorContext::BatchWrapper(
        struct sensors_poll_device_1 *dev,
        int handle,
        int flags,
        int64_t sampling_period_ns,
        int64_t max_report_latency_ns) {
    (void) flags;
    return reinterpret_cast<SensorContext *>(dev)->batch(
            handle, sampling_period_ns, max_report_latency_ns);
}

// static
int SensorContext::FlushWrapper(struct sensors_poll_device_1 *dev, int handle) {
    return reinterpret_cast<SensorContext *>(dev)->flush(handle);
}

size_t SensorContext::getSensorList(sensor_t const **list) {
    *list = &(mDynamicSensorManager->getDynamicMetaSensor());
    return 1;
}

////////////////////////////////////////////////////////////////////////////////

static sensor_t const *sensor_list;

static int open_sensors(
        const struct hw_module_t *module,
        const char *,
        struct hw_device_t **dev) {
    SensorContext *ctx = new SensorContext(module);
    ctx->getSensorList(&sensor_list);
    *dev = &ctx->device.common;
    return 0;
}

static struct hw_module_methods_t sensors_module_methods = {
    .open = open_sensors
};

static int get_sensors_list(
        struct sensors_module_t *,
        struct sensor_t const **list) {
    *list = sensor_list;
    return 1;
}

static int set_operation_mode(unsigned int mode) {
    return (mode) ? -EINVAL : 0;
}

struct sensors_module_t HAL_MODULE_INFO_SYM = {
    .common = {
            .tag = HARDWARE_MODULE_TAG,
            .version_major = 1,
            .version_minor = 0,
            .id = SENSORS_HARDWARE_MODULE_ID,
            .name = "Google Dynamic Sensor Manager",
            .author = "Google",
            .methods = &sensors_module_methods,
            .dso  = NULL,
            .reserved = {0},
    },
    .get_sensors_list = get_sensors_list,
    .set_operation_mode = set_operation_mode,
};
