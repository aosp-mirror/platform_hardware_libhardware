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

#include "HidRawSensorDevice.h"
#include "HidRawSensor.h"
#include "HidSensorDef.h"

#include <utils/Log.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>

#include <set>

namespace android {
namespace SensorHalExt {

using namespace Hid::Sensor::SensorTypeUsage;
using namespace Hid::Sensor::PropertyUsage;

const std::unordered_set<unsigned int> HidRawSensorDevice::sInterested{
        ACCELEROMETER_3D, GYROMETER_3D, COMPASS_3D, CUSTOM};

sp<HidRawSensorDevice> HidRawSensorDevice::create(const std::string &devName) {
    sp<HidRawSensorDevice> device(new HidRawSensorDevice(devName));
    // offset +1 strong count added by constructor
    device->decStrong(device.get());

    if (device->mValid) {
        return device;
    } else {
        return nullptr;
    }
}

HidRawSensorDevice::HidRawSensorDevice(const std::string &devName)
        : RefBase(), HidRawDevice(devName, sInterested),
          Thread(false /*canCallJava*/), mValid(false) {
    // create HidRawSensor objects from digest
    // HidRawSensor object will take sp<HidRawSensorDevice> as parameter, so increment strong count
    // to prevent "this" being destructed.
    this->incStrong(this);

    if (!HidRawDevice::isValid()) {
        return;
    }

    for (const auto &digest : mDigestVector) { // for each usage - vec<ReportPacket> pair
        uint32_t usage = static_cast<uint32_t>(digest.fullUsage);
        sp<HidRawSensor> s(new HidRawSensor(this, usage, digest.packets));
        if (s->isValid()) {
            for (const auto &packet : digest.packets) {
                if (packet.type == HidParser::REPORT_TYPE_INPUT) { // only used for input mapping
                    mSensors.emplace(packet.id/* report id*/, s);
                }
            }
        }
    }
    if (mSensors.size() == 0) {
        return;
    }

    run("HidRawSensor");
    mValid = true;
}

HidRawSensorDevice::~HidRawSensorDevice() {
    ALOGV("~HidRawSensorDevice %p", this);
    requestExitAndWait();
    ALOGV("~HidRawSensorDevice %p, thread exited", this);
}

bool HidRawSensorDevice::threadLoop() {
    ALOGV("Hid Raw Device thread started %p", this);
    std::vector<uint8_t> buffer;
    bool ret;
    uint8_t usageId;

    while(!Thread::exitPending()) {
        ret = receiveReport(&usageId, &buffer);
        if (!ret) {
            break;
        }

        auto i = mSensors.find(usageId);
        if (i == mSensors.end()) {
            ALOGW("Input of unknow usage id %u received", usageId);
            continue;
        }

        i->second->handleInput(usageId, buffer);
    }

    ALOGI("Hid Raw Device thread ended for %p", this);
    return false;
}

BaseSensorVector HidRawSensorDevice::getSensors() const {
    BaseSensorVector ret;
    std::set<sp<BaseSensorObject>> set;
    for (const auto &s : mSensors) {
        if (set.find(s.second) == set.end()) {
            ret.push_back(s.second);
            set.insert(s.second);
        }
    }
    return ret;
}

} // namespace SensorHalExt
} // namespace android
