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

#ifndef ANDROID_SENSORHAL_EXT_HID_DEVICE_H
#define ANDROID_SENSORHAL_EXT_HID_DEVICE_H
#include "Utils.h"
#include <string>
#include <vector>
#include <unordered_set>

namespace android {
namespace SensorHalExt {

class HidDevice : virtual public REF_BASE(HidDevice) {
public:
    virtual ~HidDevice() = default;

    struct HidDeviceInfo {
        std::string name;
        std::string physicalPath;
        std::string busType;
        uint16_t vendorId;
        uint16_t productId;
        std::vector<uint8_t> descriptor;
    };

    virtual const HidDeviceInfo& getDeviceInfo() = 0;

    // get feature from device
    virtual bool getFeature(uint8_t id, std::vector<uint8_t> *out) = 0;

    // write feature to device
    virtual bool setFeature(uint8_t id, const std::vector<uint8_t> &in) = 0;

    // send report to default output endpoint
    virtual bool sendReport(uint8_t id, std::vector<uint8_t> &data) = 0;

    // receive from default input endpoint
    virtual bool receiveReport(uint8_t *id, std::vector<uint8_t> *data) = 0;
};

} // namespace SensorHalExt
} // namespace android
#endif // ANDROID_SENSORHAL_EXT_HID_DEVICE_H
