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
#ifndef ANDROID_SENSORHAL_EXT_HIDRAW_DEVICE_H
#define ANDROID_SENSORHAL_EXT_HIDRAW_DEVICE_H

#include "HidDevice.h"

#include <HidParser.h>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace android {
namespace SensorHalExt {

using HidUtil::HidParser;
using HidUtil::HidReport;

class HidRawDevice : public HidDevice {
    friend class HidRawDeviceTest;
public:
    HidRawDevice(const std::string &devName, const std::unordered_set<unsigned int> &usageSet);
    virtual ~HidRawDevice();

    // test if the device initialized successfully
    bool isValid();

    // implement HidDevice pure virtuals
    virtual HidDeviceInfo& getDeviceInfo() override { return mDeviceInfo; }
    virtual bool getFeature(uint8_t id, std::vector<uint8_t> *out) override;
    virtual bool setFeature(uint8_t id, const std::vector<uint8_t> &in) override;
    virtual bool sendReport(uint8_t id, std::vector<uint8_t> &data) override;
    virtual bool receiveReport(uint8_t *id, std::vector<uint8_t> *data) override;

protected:
    bool populateDeviceInfo();
    size_t getReportSize(int type, uint8_t id);
    bool generateDigest(const std::unordered_set<uint32_t> &usage);
    size_t calculateReportBitSize(const std::vector<HidReport> &reportItems);
    const HidParser::ReportPacket *getReportPacket(unsigned int type, unsigned int id);

    typedef std::pair<unsigned int, unsigned int> ReportTypeIdPair;
    struct UnsignedIntPairHash {
        size_t operator()(const ReportTypeIdPair& v) const {
            std::hash<unsigned int> hash;
            return hash(v.first) ^ hash(v.second);
        }
    };

    std::unordered_map<ReportTypeIdPair, const HidParser::ReportPacket *, UnsignedIntPairHash>
            mReportTypeIdMap;

    HidParser::DigestVector mDigestVector;
private:
    std::mutex mIoBufferLock;
    std::vector<uint8_t> mIoBuffer;

    int mDevFd;
    HidDeviceInfo mDeviceInfo;
    bool mMultiIdDevice;
    int mValid;

    HidRawDevice(const HidRawDevice &) = delete;
    void operator=(const HidRawDevice &) = delete;
};

} // namespace SensorHalExt
} // namespace android

#endif // ANDROID_SENSORHAL_EXT_HIDRAW_DEVICE_H

