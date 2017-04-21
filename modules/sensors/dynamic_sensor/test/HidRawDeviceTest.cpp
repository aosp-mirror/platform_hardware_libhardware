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
#define LOG_TAG "HidRawDeviceTest"

#include "HidRawDevice.h"
#include "HidRawSensor.h"
#include "HidSensorDef.h"
#include "SensorEventCallback.h"
#include "Utils.h"
#include "HidLog.h"
#include "StreamIoUtil.h"

namespace android {
namespace SensorHalExt {

/*
 * Host test that verifies HidRawDevice and HidRawSensor works correctly.
 */
class HidRawDeviceTest {
public:
    static void test(const char *devicePath) {
        using namespace Hid::Sensor::SensorTypeUsage;
        using HidUtil::hexdumpToStream;

        std::unordered_set<unsigned int> interestedUsage{
                ACCELEROMETER_3D, GYROMETER_3D, COMPASS_3D, CUSTOM};

        SP(HidRawDevice) device =
                std::make_shared<HidRawDevice>(std::string(devicePath), interestedUsage);
        const HidDevice::HidDeviceInfo &info = device->getDeviceInfo();

        LOG_V << "Sizeof descriptor: " << info.descriptor.size() << LOG_ENDL;
        LOG_V << "Descriptor: " << LOG_ENDL;
        hexdumpToStream(LOG_V, info.descriptor.begin(), info.descriptor.end());

        if (!device->isValid()) {
            LOG_E << "invalid device" << LOG_ENDL;
            return;
        }

        LOG_V << "Digest: " << LOG_ENDL;
        LOG_V << device->mDigestVector;

        std::vector<uint8_t> buffer;
        // Dump first few feature ID to help debugging.
        // If device does not implement all these features, it will show error messages.
        for (int featureId = 0; featureId <= 5; ++featureId) {
            if (!device->getFeature(featureId, &buffer)) {
                LOG_E << "cannot get feature " << featureId << LOG_ENDL;
            } else {
                LOG_V << "Dump of feature " << featureId << LOG_ENDL;
                hexdumpToStream(LOG_V, buffer.begin(), buffer.end());
            }
        }
        //
        // use HidRawSensor to operate the device, pick first digest
        //
        auto &reportDigest = device->mDigestVector[0];
        SP(HidRawSensor) sensor = std::make_shared<HidRawSensor>(
                device, reportDigest.fullUsage, reportDigest.packets);

        if (!sensor->isValid()) {
            LOG_E << "Sensor is not valid " << LOG_ENDL;
            return;
        }

        const sensor_t *s = sensor->getSensor();
        LOG_V << "Sensor name: " << s->name << ", vendor: " << s->vendor << LOG_ENDL;
        LOG_V << sensor->dump() << LOG_ENDL;

        class Callback : public SensorEventCallback {
            virtual int submitEvent(SP(BaseSensorObject) /*sensor*/, const sensors_event_t &e) {
                LOG_V << "sensor: " << e.sensor << ", type: " << e.type << ", ts: " << e.timestamp
                      << ", values (" << e.data[0] << ", " << e.data[1] << ", " << e.data[2] << ")"
                      << LOG_ENDL;
                return 1;
            }
        };
        Callback callback;
        sensor->setEventCallback(&callback);

        // Request sensor samples at to 10Hz (100ms)
        sensor->batch(100LL*1000*1000 /*ns*/, 0);
        sensor->enable(true);

        // get a couple of events
        for (size_t i = 0; i < 100; ++i) {
            uint8_t id;
            if (!device->receiveReport(&id, &buffer)) {
                LOG_E << "Receive report error" << LOG_ENDL;
                continue;
            }
            sensor->handleInput(id, buffer);
        }

        // clean up
        sensor->enable(false);

        LOG_V << "Done!" << LOG_ENDL;
    }
};
} //namespace SensorHalExt
} //namespace android

int main(int argc, char* argv[]) {
    if (argc != 2) {
        LOG_E << "Usage: " << argv[0] << " hidraw-dev-path" << LOG_ENDL;
        return -1;
    }
    android::SensorHalExt::HidRawDeviceTest::test(argv[1]);
    return 0;
}
