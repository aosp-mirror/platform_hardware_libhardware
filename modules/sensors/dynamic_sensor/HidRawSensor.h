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
#ifndef ANDROID_SENSORHAL_EXT_HIDRAW_SENSOR_H
#define ANDROID_SENSORHAL_EXT_HIDRAW_SENSOR_H

#include "BaseSensorObject.h"
#include "HidDevice.h"
#include "Utils.h"

#include <HidParser.h>
#include <hardware/sensors.h>

namespace android {
namespace SensorHalExt {

using HidUtil::HidParser;
using ReportPacket = HidParser::ReportPacket;
using ReportItem = HidParser::ReportItem;

class HidRawSensor : public BaseSensorObject {
    friend class HidRawSensorTest;
    friend class HidRawDeviceTest;
public:
    HidRawSensor(SP(HidDevice) device, uint32_t usage,
                 const std::vector<HidParser::ReportPacket> &report);

    // implements BaseSensorObject
    virtual const sensor_t* getSensor() const;
    virtual void getUuid(uint8_t* uuid) const;
    virtual int enable(bool enable);
    virtual int batch(int64_t samplePeriod, int64_t batchPeriod); // unit nano-seconds

    // handle input report received
    void handleInput(uint8_t id, const std::vector<uint8_t> &message);

    // indicate if the HidRawSensor is a valid one
    bool isValid() const { return mValid; };

private:

    // structure used for holding descriptor parse result for each report field
    enum {
        TYPE_FLOAT,
        TYPE_INT64,
        TYPE_ACCURACY
    };
    struct ReportTranslateRecord {
        int type;
        int index;
        int64_t maxValue;
        int64_t minValue;
        size_t byteOffset;
        size_t byteSize;
        double a;
        int64_t b;
    };

    // sensor related information parsed from HID descriptor
    struct FeatureValue {
        // information needed to furnish sensor_t structure (see hardware/sensors.h)
        std::string name;
        std::string vendor;
        std::string permission;
        std::string typeString;
        int32_t type;
        int version;
        float maxRange;
        float resolution;
        float power;
        int32_t minDelay;
        int64_t maxDelay;
        size_t fifoSize;
        size_t fifoMaxSize;
        uint32_t reportModeFlag;
        bool isWakeUp;

        // dynamic sensor specific
        std::string uniqueId;
        uint8_t uuid[16];

        // if the device is custom sensor HID device that furnished android specific descriptors
        bool isAndroidCustom;
    };

    // helper function to find the first report item with specified usage, type and id.
    // if parameter id is omitted, this function looks for usage with all ids.
    // return nullptr if nothing is found.
    static const HidParser::ReportItem* find
            (const std::vector<HidParser::ReportPacket> &packets,
            unsigned int usage, int type, int id = -1);

    // helper function to decode std::string from HID feature report buffer.
    static bool decodeString(
            const HidParser::ReportItem &report,
            const std::vector<uint8_t> &buffer, std::string *d);

    // initialize default feature values default based on hid device info
    static void initFeatureValueFromHidDeviceInfo(
            FeatureValue *featureValue, const HidDevice::HidDeviceInfo &info);

    // populates feature values from descripitors and hid feature reports
    bool populateFeatureValueFromFeatureReport(
            FeatureValue *featureValue, const std::vector<HidParser::ReportPacket> &packets);

    // validate feature values and construct sensor_t structure if values are ok.
    bool validateFeatureValueAndBuildSensor();

    // helper function to find sensor control feature usage from packets
    bool findSensorControlUsage(const std::vector<HidParser::ReportPacket> &packets);

    // try to parse sensor description feature value to see if it matches
    // android specified custom sensor definition.
    bool detectAndroidCustomSensor(const std::string &description);

    // process HID sensor spec defined three axis sensors usages: accel, gyro, mag.
    bool processTriAxisUsage(const std::vector<HidParser::ReportPacket> &packets,
            uint32_t usageX, uint32_t usageY, uint32_t usageZ, double defaultScaling = 1);

    // process HID snesor spec defined orientation(quaternion) sensor usages.
    bool processQuaternionUsage(const std::vector<HidParser::ReportPacket> &packets);

    // dump data for test/debug purpose
    std::string dump() const;

    // Features for control sensor
    int mReportingStateId;
    unsigned int mReportingStateOffset;

    int mPowerStateId;
    unsigned int mPowerStateOffset;

    int mReportIntervalId;
    unsigned int mReportIntervalOffset;
    unsigned int mReportIntervalSize;

    // Input report translate table
    std::vector<ReportTranslateRecord> mTranslateTable;
    unsigned mInputReportId;

    FeatureValue mFeatureInfo;
    sensor_t mSensor;

    // runtime states variable
    bool mEnabled;
    int64_t mSamplingPeriod;    // ns
    int64_t mBatchingPeriod;    // ns

    WP(HidDevice) mDevice;
    bool mValid;
};

} // namespace SensorHalExt
} // namespace android
#endif // ANDROID_SENSORHAL_EXT_HIDRAW_SENSOR_H

