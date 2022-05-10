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
#include "SensorEventCallback.h"
#include "Utils.h"

#include <cstring>

namespace android {
namespace SensorHalExt {

BaseSensorObject::BaseSensorObject() : mCallback(nullptr) {
}

bool BaseSensorObject::setEventCallback(SensorEventCallback* callback) {
    if (mCallback != nullptr) {
        return false;
    }
    mCallback = callback;
    return true;
}

void BaseSensorObject::getUuid(uint8_t* uuid) const {
    // default uuid denoting uuid feature is not supported on this sensor.
    memset(uuid, 0, 16);
}

int BaseSensorObject::flush() {
    static const sensors_event_t event = {
        .type = SENSOR_TYPE_META_DATA,
        .meta_data.what = META_DATA_FLUSH_COMPLETE,
        .timestamp = TIMESTAMP_AUTO_FILL  // timestamp will be filled at dispatcher
    };
    generateEvent(event);
    return 0;
}

void BaseSensorObject::generateEvent(const sensors_event_t &e) {
    if (mCallback) {
        mCallback->submitEvent(SP_THIS, e);
    }
}

} // namespace SensorHalExt
} // namespace android

