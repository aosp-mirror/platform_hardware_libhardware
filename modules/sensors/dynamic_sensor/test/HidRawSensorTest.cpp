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
#define LOG_TAG "HidRawSensorTest"

#include "HidDevice.h"
#include "HidLog.h"
#include "HidLog.h"
#include "HidParser.h"
#include "HidRawSensor.h"
#include "HidSensorDef.h"
#include "StreamIoUtil.h"
#include "TestHidDescriptor.h"
#include "Utils.h"

#include <deque>
#include <unordered_map>

namespace android {
namespace SensorHalExt {

class HidRawDummyDevice : public HidDevice {
public:
    struct DataPair {
        uint8_t id;
        std::vector<uint8_t> data;
    };

    HidRawDummyDevice() {
        // dummy values
        mInfo = {
          .name = "Test sensor name",
          .physicalPath = "/physical/path",
          .busType = "USB",
          .vendorId = 0x1234,
          .productId = 0x5678,
          .descriptor = {0}
      };
    }

    virtual const HidDeviceInfo& getDeviceInfo() {
        return mInfo;
    }

    // get feature from device
    virtual bool getFeature(uint8_t id, std::vector<uint8_t> *out) {
        auto i = mFeature.find(id);
        if (i == mFeature.end()) {
            return false;
        }
        *out = i->second;
        return true;
    }

    // write feature to device
    virtual bool setFeature(uint8_t id, const std::vector<uint8_t> &in) {
        auto i = mFeature.find(id);
        if (i == mFeature.end() || in.size() != i->second.size()) {
            return false;
        }
        i->second = in;
        return true;
    }

    // send report to default output endpoint
    virtual bool sendReport(uint8_t id, std::vector<uint8_t> &data) {
        DataPair pair = {
            .id = id,
            .data = data
        };
        mOutput.push_back(pair);
        return true;
    }

    // receive from default input endpoint
    virtual bool receiveReport(uint8_t * /*id*/, std::vector<uint8_t> * /*data*/) {
        // not necessary, as input report can be directly feed to HidRawSensor for testing purpose
        return false;
    }

    bool dequeuOutputReport(DataPair *pair) {
        if (!mOutput.empty()) {
            return false;
        }
        *pair = mOutput.front();
        mOutput.pop_front();
        return true;
    }

private:
    HidDeviceInfo mInfo;
    std::deque<DataPair> mOutput;
    std::unordered_map<uint8_t, std::vector<uint8_t>> mFeature;
};

class HidRawSensorTest {
public:
    static bool test() {
        bool ret = true;
        using namespace Hid::Sensor::SensorTypeUsage;
        std::unordered_set<unsigned int> interestedUsage{
                ACCELEROMETER_3D, GYROMETER_3D, COMPASS_3D, CUSTOM};
        SP(HidDevice) device(new HidRawDummyDevice());

        HidParser hidParser;
        for (const TestHidDescriptor *p = gDescriptorArray; ; ++p) {
            if (p->data == nullptr || p->len == 0) {
                break;
            }
            const char *name = p->name != nullptr ? p->name : "unnamed";
            if (!hidParser.parse(p->data, p->len)) {
                LOG_E << name << " parsing error!" << LOG_ENDL;
                ret = false;
                continue;
            }

            hidParser.filterTree();
            LOG_V << name << "  digest: " << LOG_ENDL;
            auto digestVector = hidParser.generateDigest(interestedUsage);
            LOG_V << digestVector;

            if (digestVector.empty()) {
                LOG_V << name << " does not contain interested usage" << LOG_ENDL;
                continue;
            }

            LOG_V << name << "  sensor: " << LOG_ENDL;
            for (const auto &digest : digestVector) {
                LOG_I << "Sensor usage " << std::hex << digest.fullUsage << std::dec << LOG_ENDL;
                auto *s = new HidRawSensor(device, digest.fullUsage, digest.packets);
                if (s->mValid) {
                    LOG_V << "Usage " << std::hex << digest.fullUsage << std::dec << LOG_ENDL;
                    LOG_V << s->dump();
                } else {
                    LOG_V << "Sensor of usage " << std::hex << digest.fullUsage << std::dec
                          << " not valid!" << LOG_ENDL;
                }
            }
            LOG_V << LOG_ENDL;
        }
        return ret;
    }
};

}// namespace SensorHalExt
}// namespace android

int main() {
    return android::SensorHalExt::HidRawSensorTest::test() ? 0 : 1;
}
