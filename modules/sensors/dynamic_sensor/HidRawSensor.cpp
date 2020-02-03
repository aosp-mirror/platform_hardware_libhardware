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
#include "HidRawSensor.h"
#include "HidSensorDef.h"

#include <utils/Errors.h>
#include "HidLog.h"

#include <algorithm>
#include <cfloat>
#include <codecvt>
#include <iomanip>
#include <sstream>

namespace android {
namespace SensorHalExt {

namespace {
const std::string CUSTOM_TYPE_PREFIX("com.google.hardware.sensor.hid_dynamic.");
}

HidRawSensor::HidRawSensor(
        SP(HidDevice) device, uint32_t usage, const std::vector<HidParser::ReportPacket> &packets)
        : mReportingStateId(-1), mPowerStateId(-1), mReportIntervalId(-1), mInputReportId(-1),
        mEnabled(false), mSamplingPeriod(1000LL*1000*1000), mBatchingPeriod(0),
        mDevice(device), mValid(false) {
    if (device == nullptr) {
        return;
    }
    memset(&mSensor, 0, sizeof(mSensor));

    const HidDevice::HidDeviceInfo &info =  device->getDeviceInfo();
    initFeatureValueFromHidDeviceInfo(&mFeatureInfo, info);

    if (!populateFeatureValueFromFeatureReport(&mFeatureInfo, packets)) {
        LOG_E << "populate feature from feature report failed" << LOG_ENDL;
        return;
    }

    if (!findSensorControlUsage(packets)) {
        LOG_E << "finding sensor control usage failed" << LOG_ENDL;
        return;
    }

    // build translation table
    bool translationTableValid = false;
    switch (usage) {
        using namespace Hid::Sensor::SensorTypeUsage;
        using namespace Hid::Sensor::ReportUsage;
        case ACCELEROMETER_3D:
            // Hid unit default g
            // Android unit m/s^2
            // 1g = 9.81 m/s^2
            mFeatureInfo.typeString = SENSOR_STRING_TYPE_ACCELEROMETER;
            mFeatureInfo.type = SENSOR_TYPE_ACCELEROMETER;
            mFeatureInfo.isWakeUp = false;

            translationTableValid = processTriAxisUsage(packets,
                                         ACCELERATION_X_AXIS,
                                         ACCELERATION_Y_AXIS,
                                         ACCELERATION_Z_AXIS, 9.81);
            break;
        case GYROMETER_3D:
            // Hid unit default degree/s
            // Android unit rad/s
            // 1 degree/s = pi/180 rad/s
            mFeatureInfo.typeString = SENSOR_STRING_TYPE_GYROSCOPE;
            mFeatureInfo.type = SENSOR_TYPE_GYROSCOPE;
            mFeatureInfo.isWakeUp = false;

            translationTableValid = processTriAxisUsage(packets,
                                         ANGULAR_VELOCITY_X_AXIS,
                                         ANGULAR_VELOCITY_Y_AXIS,
                                         ANGULAR_VELOCITY_Z_AXIS, M_PI/180);
            break;
        case COMPASS_3D: {
            // Hid unit default mGauss
            // Android unit uT
            // 1uT  = 0.1 nGauss
            mFeatureInfo.typeString = SENSOR_STRING_TYPE_MAGNETIC_FIELD;
            mFeatureInfo.type = SENSOR_TYPE_MAGNETIC_FIELD;

            if (!processTriAxisUsage(packets,
                                     MAGNETIC_FLUX_X_AXIS,
                                     MAGNETIC_FLUX_Y_AXIS,
                                     MAGNETIC_FLUX_Z_AXIS, 0.1)) {
                break;
            }
            const HidParser::ReportItem *pReportAccuracy = find(packets,
                                                                  MAGNETOMETER_ACCURACY,
                                                                  HidParser::REPORT_TYPE_INPUT,
                                                                  mInputReportId);

            if (pReportAccuracy == nullptr) {
                LOG_E << "Cannot find accuracy field in usage "
                      << std::hex << usage << std::dec << LOG_ENDL;
                break;
            }
            if (!pReportAccuracy->isByteAligned()) {
                LOG_E << "Accuracy field must align to byte" << LOG_ENDL;
                break;
            }
            if (pReportAccuracy->minRaw != 0 || pReportAccuracy->maxRaw != 2) {
                LOG_E << "Accuracy field value range must be [0, 2]" << LOG_ENDL;
                break;
            }
            ReportTranslateRecord accuracyRecord = {
                .type = TYPE_ACCURACY,
                .maxValue = 2,
                .minValue = 0,
                .byteOffset = pReportAccuracy->bitOffset / 8,
                .byteSize = pReportAccuracy->bitSize / 8,
                .a = 1,
                .b = 1};
            mTranslateTable.push_back(accuracyRecord);
            translationTableValid = true;
            break;
        }
        case DEVICE_ORIENTATION:
            translationTableValid = processQuaternionUsage(packets);
            break;
        case CUSTOM: {
            if (!mFeatureInfo.isAndroidCustom) {
                LOG_E << "Invalid android custom sensor" << LOG_ENDL;
                break;
            }
            const HidParser::ReportPacket *pPacket = nullptr;
            const uint32_t usages[] = {
                CUSTOM_VALUE_1, CUSTOM_VALUE_2, CUSTOM_VALUE_3,
                CUSTOM_VALUE_4, CUSTOM_VALUE_5, CUSTOM_VALUE_6
            };
            for (const auto &packet : packets) {
                if (packet.type == HidParser::REPORT_TYPE_INPUT && std::any_of(
                        packet.reports.begin(), packet.reports.end(),
                        [&usages] (const HidParser::ReportItem &d) {
                               return std::find(std::begin(usages), std::end(usages), d.usage)
                                       != std::end(usages);
                        })) {
                    pPacket = &packet;
                    break;
                }
            }

            if (pPacket == nullptr) {
                LOG_E << "Cannot find CUSTOM_VALUE_X in custom sensor" << LOG_ENDL;
                break;
            }

            double range = 0;
            double resolution = 1;

            for (const auto &digest : pPacket->reports) {
                if (digest.minRaw >= digest.maxRaw) {
                    LOG_E << "Custome usage " << digest.usage << ", min must < max" << LOG_ENDL;
                    return;
                }

                if (!digest.isByteAligned()
                        || (digest.bitSize != 8 && digest.bitSize != 16 && digest.bitSize != 32)) {
                    LOG_E << "Custome usage " << std::hex << digest.usage << std::hex
                          << ", each input must be 8/16/32 bits and must align to byte boundary"
                          << LOG_ENDL;
                    return;
                }

                ReportTranslateRecord record = {
                    .minValue = digest.minRaw,
                    .maxValue = digest.maxRaw,
                    .byteOffset = digest.bitOffset / 8,
                    .byteSize = digest.bitSize / 8,
                    .a = digest.a,
                    .b = digest.b,
                    .type = TYPE_FLOAT
                };
                // keep track of range and resolution
                range = std::max(std::max(std::abs((digest.maxRaw + digest.b) * digest.a),
                                          std::abs((digest.minRaw + digest.b) * digest.a)),
                                 range);
                resolution = std::min(digest.a, resolution);

                for (size_t i = 0; i < digest.count; ++i) {
                    if (mTranslateTable.size() == 16) {
                        LOG_I << "Custom usage has more than 16 inputs, ignore the rest" << LOG_ENDL;
                        break;
                    }
                    record.index = mTranslateTable.size();
                    mTranslateTable.push_back(record);
                    record.byteOffset += digest.bitSize / 8;
                }
                if (mTranslateTable.size() == 16) {
                    break;
                }
            }
            mFeatureInfo.maxRange = range;
            mFeatureInfo.resolution = resolution;
            mInputReportId = pPacket->id;
            translationTableValid = !mTranslateTable.empty();
            break;
        }
        default:
            LOG_I << "unsupported sensor usage " << usage << LOG_ENDL;
    }

    bool sensorValid = validateFeatureValueAndBuildSensor();
    mValid = translationTableValid && sensorValid;
    LOG_V << "HidRawSensor init, translationTableValid: " << translationTableValid
          << ", sensorValid: " << sensorValid << LOG_ENDL;
}

bool HidRawSensor::processQuaternionUsage(const std::vector<HidParser::ReportPacket> &packets) {
    const HidParser::ReportItem *pReportQuaternion
            = find(packets,
                   Hid::Sensor::ReportUsage::ORIENTATION_QUATERNION,
                   HidParser::REPORT_TYPE_INPUT);

    if (pReportQuaternion == nullptr) {
        return false;
    }

    const HidParser::ReportItem &quat = *pReportQuaternion;
    if ((quat.bitSize != 16 && quat.bitSize != 32) || !quat.isByteAligned()) {
        LOG_E << "Quaternion usage input must be 16 or 32 bits and aligned at byte boundary" << LOG_ENDL;
        return false;
    }

    double min, max;
    quat.decode(quat.mask(quat.minRaw), &min);
    quat.decode(quat.mask(quat.maxRaw), &max);
    if (quat.count != 4 || min > -1 || max < 1) {
        LOG_E << "Quaternion usage need 4 inputs with range [-1, 1]" << LOG_ENDL;
        return false;
    }

    if (quat.minRaw > quat.maxRaw) {
        LOG_E << "Quaternion usage min must <= max" << LOG_ENDL;
        return false;
    }

    ReportTranslateRecord record = {
        .minValue = quat.minRaw,
        .maxValue = quat.maxRaw,
        .byteOffset = quat.bitOffset / 8,
        .byteSize = quat.bitSize / 8,
        .b = quat.b,
        .type = TYPE_FLOAT,
    };

    // Android X Y Z maps to HID X -Z Y
    // Android order xyzw, HID order wxyz
    // X
    record.index = 0;
    record.a = quat.a;
    record.byteOffset = (quat.bitOffset + quat.bitSize) / 8;
    mTranslateTable.push_back(record);
    // Y
    record.index = 1;
    record.a = -quat.a;
    record.byteOffset = (quat.bitOffset + 3 * quat.bitSize) / 8;
    mTranslateTable.push_back(record);
    // Z
    record.index = 2;
    record.a = quat.a;
    record.byteOffset = (quat.bitOffset + 2 * quat.bitSize) / 8;
    mTranslateTable.push_back(record);
    // W
    record.index = 3;
    record.a = quat.a;
    record.byteOffset = quat.bitOffset / 8;
    mTranslateTable.push_back(record);

    mFeatureInfo.typeString = SENSOR_STRING_TYPE_ROTATION_VECTOR;
    mFeatureInfo.type = SENSOR_TYPE_ROTATION_VECTOR;
    mFeatureInfo.maxRange = 1;
    mFeatureInfo.resolution = quat.a;
    mFeatureInfo.reportModeFlag = SENSOR_FLAG_CONTINUOUS_MODE;

    mInputReportId = quat.id;

    return true;
}

bool HidRawSensor::processTriAxisUsage(const std::vector<HidParser::ReportPacket> &packets,
        uint32_t usageX, uint32_t usageY, uint32_t usageZ, double defaultScaling) {
    const HidParser::ReportItem *pReportX = find(packets, usageX, HidParser::REPORT_TYPE_INPUT);
    const HidParser::ReportItem *pReportY = find(packets, usageY, HidParser::REPORT_TYPE_INPUT);
    const HidParser::ReportItem *pReportZ = find(packets, usageZ, HidParser::REPORT_TYPE_INPUT);

    if (pReportX == nullptr || pReportY == nullptr|| pReportZ == nullptr) {
        LOG_E << "Three axis sensor does not find all 3 axis" << LOG_ENDL;
        return false;
    }

    const HidParser::ReportItem &reportX = *pReportX;
    const HidParser::ReportItem &reportY = *pReportY;
    const HidParser::ReportItem &reportZ = *pReportZ;
    if (reportX.id != reportY.id || reportY.id != reportZ.id) {
        LOG_E << "All 3 axis should be in the same report" << LOG_ENDL;
        return false;
    }
    if (reportX.minRaw >= reportX.maxRaw
            || reportX.minRaw != reportY.minRaw
            || reportX.maxRaw != reportY.maxRaw
            || reportY.minRaw != reportZ.minRaw
            || reportY.maxRaw != reportZ.maxRaw) {
        LOG_E << "All 3 axis should have same min and max value and min must < max" << LOG_ENDL;
        return false;
    }
    if (reportX.a != reportY.a || reportY.a != reportY.a) {
        LOG_E << "All 3 axis should have same resolution" << LOG_ENDL;
        return false;
    }
    if (reportX.count != 1 || reportY.count != 1 || reportZ.count != 1
            || (reportX.bitSize != 16 && reportX.bitSize != 32)
            || reportX.bitSize != reportY.bitSize || reportY.bitSize != reportZ.bitSize
            || !reportX.isByteAligned()
            || !reportY.isByteAligned()
            || !reportZ.isByteAligned() ) {
        LOG_E << "All 3 axis should have count == 1, same size == 16 or 32 "
              "and align at byte boundary" << LOG_ENDL;
        return false;
    }

    if (reportX.unit != 0 || reportY.unit != 0 || reportZ.unit != 0) {
        LOG_E << "Specified unit for usage is not supported" << LOG_ENDL;
        return false;
    }

    if (reportX.a != reportY.a || reportY.a != reportZ.a
        || reportX.b != reportY.b || reportY.b != reportZ.b) {
        LOG_W << "Scaling for 3 axis are different. It is recommended to keep them the same" << LOG_ENDL;
    }

    // set features
    mFeatureInfo.maxRange = std::max(
        std::abs((reportX.maxRaw + reportX.b) * reportX.a),
        std::abs((reportX.minRaw + reportX.b) * reportX.a));
    mFeatureInfo.resolution = reportX.a * defaultScaling;
    mFeatureInfo.reportModeFlag = SENSOR_FLAG_CONTINUOUS_MODE;

    ReportTranslateRecord record = {
        .minValue = reportX.minRaw,
        .maxValue = reportX.maxRaw,
        .byteSize = reportX.bitSize / 8,
        .type = TYPE_FLOAT
    };

    // Reorder and swap axis
    //
    // HID class devices are encouraged, where possible, to use a right-handed
    // coordinate system. If a user is facing a device, report values should increase as
    // controls are moved from left to right (X), from far to near (Y) and from high to
    // low (Z).
    //

    // Android X axis = Hid X axis
    record.index = 0;
    record.a = reportX.a * defaultScaling;
    record.b = reportX.b;
    record.byteOffset = reportX.bitOffset / 8;
    mTranslateTable.push_back(record);

    // Android Y axis = - Hid Z axis
    record.index = 1;
    record.a = -reportZ.a * defaultScaling;
    record.b = reportZ.b;
    record.byteOffset = reportZ.bitOffset / 8;
    mTranslateTable.push_back(record);

    // Android Z axis = Hid Y axis
    record.index = 2;
    record.a = reportY.a * defaultScaling;
    record.b = reportY.b;
    record.byteOffset = reportY.bitOffset / 8;
    mTranslateTable.push_back(record);

    mInputReportId = reportX.id;
    return true;
}

const HidParser::ReportItem *HidRawSensor::find(
        const std::vector<HidParser::ReportPacket> &packets,
        unsigned int usage, int type, int id) {
    for (const auto &packet : packets) {
        if (packet.type != type) {
            continue;
        }
        auto i = std::find_if(
                packet.reports.begin(), packet.reports.end(),
                [usage, id](const HidParser::ReportItem &p) {
                    return p.usage == usage
                            && (id == -1 || p.id == static_cast<unsigned int>(id));
                });
        if (i != packet.reports.end()) {
            return &(*i);
        }
    }
    return nullptr;
};

void HidRawSensor::initFeatureValueFromHidDeviceInfo(
        FeatureValue *featureValue, const HidDevice::HidDeviceInfo &info) {
    featureValue->name = info.name;

    std::ostringstream ss;
    ss << info.busType << " "
       << std::hex << std::setfill('0') << std::setw(4) << info.vendorId
       << ":" << std::setw(4) << info.productId;
    featureValue->vendor = ss.str();

    featureValue->permission = "";
    featureValue->typeString = "";
    featureValue->type = -1; // invalid type
    featureValue->version = 1;

    featureValue->maxRange = -1.f;
    featureValue->resolution = FLT_MAX;
    featureValue->power = 1.f; // default value, does not have a valid source yet

    featureValue->minDelay = 0;
    featureValue->maxDelay = 0;

    featureValue->fifoSize = 0;
    featureValue->fifoMaxSize = 0;

    featureValue->reportModeFlag = SENSOR_FLAG_SPECIAL_REPORTING_MODE;
    featureValue->isWakeUp = false;
    memset(featureValue->uuid, 0, sizeof(featureValue->uuid));
    featureValue->isAndroidCustom = false;
}

bool HidRawSensor::populateFeatureValueFromFeatureReport(
        FeatureValue *featureValue, const std::vector<HidParser::ReportPacket> &packets) {
    SP(HidDevice) device = PROMOTE(mDevice);
    if (device == nullptr) {
        return false;
    }

    std::vector<uint8_t> buffer;
    for (const auto &packet : packets) {
        if (packet.type != HidParser::REPORT_TYPE_FEATURE) {
            continue;
        }

        if (!device->getFeature(packet.id, &buffer)) {
            continue;
        }

        std::string str;
        using namespace Hid::Sensor::PropertyUsage;
        for (const auto & r : packet.reports) {
            switch (r.usage) {
                case FRIENDLY_NAME:
                    if (!r.isByteAligned() || r.bitSize != 16 || r.count < 1) {
                        // invalid friendly name
                        break;
                    }
                    if (decodeString(r, buffer, &str) && !str.empty()) {
                        featureValue->name = str;
                    }
                    break;
                case SENSOR_MANUFACTURER:
                    if (!r.isByteAligned() || r.bitSize != 16 || r.count < 1) {
                        // invalid manufacturer
                        break;
                    }
                    if (decodeString(r, buffer, &str) && !str.empty()) {
                        featureValue->vendor = str;
                    }
                    break;
                case PERSISTENT_UNIQUE_ID:
                    if (!r.isByteAligned() || r.bitSize != 16 || r.count < 1) {
                        // invalid unique id string
                        break;
                    }
                    if (decodeString(r, buffer, &str) && !str.empty()) {
                        featureValue->uniqueId = str;
                    }
                    break;
                case SENSOR_DESCRIPTION:
                    if (!r.isByteAligned() || r.bitSize != 16 || r.count < 1
                            || (r.bitOffset / 8 + r.count * 2) > buffer.size() ) {
                        // invalid description
                        break;
                    }
                    if (decodeString(r, buffer, &str)) {
                        mFeatureInfo.isAndroidCustom = detectAndroidCustomSensor(str);
                    }
                    break;
                default:
                    // do not care about others
                    break;
            }
        }
    }
    return true;
}

bool HidRawSensor::validateFeatureValueAndBuildSensor() {
    if (mFeatureInfo.name.empty() || mFeatureInfo.vendor.empty() || mFeatureInfo.typeString.empty()
            || mFeatureInfo.type <= 0 || mFeatureInfo.maxRange <= 0
            || mFeatureInfo.resolution <= 0) {
        return false;
    }

    switch (mFeatureInfo.reportModeFlag) {
        case SENSOR_FLAG_CONTINUOUS_MODE:
        case SENSOR_FLAG_ON_CHANGE_MODE:
            if (mFeatureInfo.minDelay < 0) {
                return false;
            }
            if (mFeatureInfo.maxDelay != 0 && mFeatureInfo.maxDelay < mFeatureInfo.minDelay) {
                return false;
            }
            break;
        case SENSOR_FLAG_ONE_SHOT_MODE:
            if (mFeatureInfo.minDelay != -1 && mFeatureInfo.maxDelay != 0) {
                return false;
            }
            break;
        case SENSOR_FLAG_SPECIAL_REPORTING_MODE:
            if (mFeatureInfo.minDelay != -1 && mFeatureInfo.maxDelay != 0) {
                return false;
            }
            break;
        default:
            break;
    }

    if (mFeatureInfo.fifoMaxSize < mFeatureInfo.fifoSize) {
        return false;
    }

    // initialize uuid field, use name, vendor and uniqueId
    if (mFeatureInfo.name.size() >= 4
            && mFeatureInfo.vendor.size() >= 4
            && mFeatureInfo.typeString.size() >= 4
            && mFeatureInfo.uniqueId.size() >= 4) {
        uint32_t tmp[4], h;
        std::hash<std::string> stringHash;
        h = stringHash(mFeatureInfo.uniqueId);
        tmp[0] = stringHash(mFeatureInfo.name) ^ h;
        tmp[1] = stringHash(mFeatureInfo.vendor) ^ h;
        tmp[2] = stringHash(mFeatureInfo.typeString) ^ h;
        tmp[3] = tmp[0] ^ tmp[1] ^ tmp[2];
        memcpy(mFeatureInfo.uuid, tmp, sizeof(mFeatureInfo.uuid));
    }

    mSensor = (sensor_t) {
        mFeatureInfo.name.c_str(),                 // name
        mFeatureInfo.vendor.c_str(),               // vendor
        mFeatureInfo.version,                      // version
        -1,                                        // handle, dummy number here
        mFeatureInfo.type,
        mFeatureInfo.maxRange,                     // maxRange
        mFeatureInfo.resolution,                   // resolution
        mFeatureInfo.power,                        // power
        mFeatureInfo.minDelay,                     // minDelay
        (uint32_t)mFeatureInfo.fifoSize,           // fifoReservedEventCount
        (uint32_t)mFeatureInfo.fifoMaxSize,        // fifoMaxEventCount
        mFeatureInfo.typeString.c_str(),           // type string
        mFeatureInfo.permission.c_str(),           // requiredPermission
        (long)mFeatureInfo.maxDelay,               // maxDelay
        mFeatureInfo.reportModeFlag | (mFeatureInfo.isWakeUp ? 1 : 0),
        { NULL, NULL }
    };
    return true;
}

bool HidRawSensor::decodeString(
        const HidParser::ReportItem &report, const std::vector<uint8_t> &buffer, std::string *d) {
    if (!report.isByteAligned() || report.bitSize != 16 || report.count < 1) {
        return false;
    }

    size_t offset = report.bitOffset / 8;
    if (offset + report.count * 2 > buffer.size()) {
        return false;
    }

    std::vector<uint16_t> data(report.count);
    auto i = data.begin();
    auto j = buffer.begin() + offset;
    for ( ; i != data.end(); ++i, j += sizeof(uint16_t)) {
        // hid specified little endian
        *i = *j + (*(j + 1) << 8);
    }
    std::wstring wstr(data.begin(), data.end());

    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    *d = converter.to_bytes(wstr);
    return true;
}

std::vector<std::string> split(const std::string &text, char sep) {
    std::vector<std::string> tokens;
    size_t start = 0, end = 0;
    while ((end = text.find(sep, start)) != std::string::npos) {
        if (end != start) {
            tokens.push_back(text.substr(start, end - start));
        }
        start = end + 1;
    }
    if (end != start) {
        tokens.push_back(text.substr(start));
    }
    return tokens;
}

bool HidRawSensor::detectAndroidCustomSensor(const std::string &description) {
    size_t nullPosition = description.find('\0');
    if (nullPosition == std::string::npos) {
        return false;
    }
    const std::string prefix("#ANDROID#");
    if (description.find(prefix, nullPosition + 1) != nullPosition + 1) {
        return false;
    }

    std::string str(description.c_str() + nullPosition + 1 + prefix.size());

    // Format for predefined sensor types:
    // #ANDROID#nn,[C|X|T|S],[B|0],[W|N]
    // Format for vendor type sensor
    // #ANDROID#xxx.yyy.zzz,[C|X|T|S],[B|0],[W|N]
    //
    // C: continuous
    // X: on-change
    // T: one-shot
    // S: special trigger
    //
    // B: body permission
    // 0: no permission required
    std::vector<std::string> segments;
    size_t start = 0, end = 0;
    while ((end = str.find(',', start)) != std::string::npos) {
        if (end != start) {
            segments.push_back(str.substr(start, end - start));
        }
        start = end + 1;
    }
    if (end != start) {
        segments.push_back(str.substr(start));
    }

    if (segments.size() < 4) {
        LOG_E << "Not enough segments in android custom description" << LOG_ENDL;
        return false;
    }

    // type
    bool typeParsed = false;
    if (!segments[0].empty()) {
        if (::isdigit(segments[0][0])) {
            int type = ::atoi(segments[0].c_str());
            // all supported types here
            switch (type) {
                case SENSOR_TYPE_HEART_RATE:
                    mFeatureInfo.type = SENSOR_TYPE_HEART_RATE;
                    mFeatureInfo.typeString = SENSOR_STRING_TYPE_HEART_RATE;
                    typeParsed = true;
                    break;
                case SENSOR_TYPE_AMBIENT_TEMPERATURE:
                    mFeatureInfo.type = SENSOR_TYPE_AMBIENT_TEMPERATURE;
                    mFeatureInfo.typeString = SENSOR_STRING_TYPE_AMBIENT_TEMPERATURE;
                    typeParsed = true;
                    break;
                case SENSOR_TYPE_LIGHT:
                    mFeatureInfo.type = SENSOR_TYPE_LIGHT;
                    mFeatureInfo.typeString = SENSOR_STRING_TYPE_LIGHT;
                    typeParsed = true;
                    break;
                case SENSOR_TYPE_PRESSURE:
                    mFeatureInfo.type = SENSOR_TYPE_PRESSURE;
                    mFeatureInfo.typeString = SENSOR_STRING_TYPE_PRESSURE;
                    typeParsed = true;
                    break;
                default:
                    LOG_W << "Android type " << type << " has not been supported yet" << LOG_ENDL;
                    break;
            }
        } else {
            // assume a xxx.yyy.zzz format
            std::ostringstream s;
            bool lastIsDot = true;
            for (auto c : segments[0]) {
                if (::isalpha(c)) {
                    s << static_cast<char>(c);
                    lastIsDot = false;
                } else if (!lastIsDot && c == '.') {
                    s << static_cast<char>(c);
                    lastIsDot = true;
                } else {
                    break;
                }
            }
            if (s.str() == segments[0]) {
                mFeatureInfo.type = SENSOR_TYPE_DEVICE_PRIVATE_BASE;
                mFeatureInfo.typeString = CUSTOM_TYPE_PREFIX + s.str();
                typeParsed = true;
            }
        }
    }

    // reporting type
    bool reportingModeParsed = false;
    if (segments[1].size() == 1) {
        switch (segments[1][0]) {
            case 'C':
                mFeatureInfo.reportModeFlag = SENSOR_FLAG_CONTINUOUS_MODE;
                reportingModeParsed = true;
                break;
            case 'X':
                mFeatureInfo.reportModeFlag = SENSOR_FLAG_ON_CHANGE_MODE;
                reportingModeParsed = true;
                break;
            case 'T':
                mFeatureInfo.reportModeFlag = SENSOR_FLAG_ONE_SHOT_MODE;
                reportingModeParsed = true;
                break;
            case 'S':
                mFeatureInfo.reportModeFlag = SENSOR_FLAG_SPECIAL_REPORTING_MODE;
                reportingModeParsed = true;
                break;
            default:
                LOG_E << "Undefined reporting mode designation " << segments[1] << LOG_ENDL;
        }
    }

    // permission parsed
    bool permissionParsed = false;
    if (segments[2].size() == 1) {
        switch (segments[2][0]) {
            case 'B':
                mFeatureInfo.permission = SENSOR_PERMISSION_BODY_SENSORS;
                permissionParsed = true;
                break;
            case '0':
                mFeatureInfo.permission = "";
                permissionParsed = true;
                break;
            default:
                LOG_E << "Undefined permission designation " << segments[2] << LOG_ENDL;
        }
    }

    // wake up
    bool wakeUpParsed = false;
    if (segments[3].size() == 1) {
        switch (segments[3][0]) {
            case 'W':
                mFeatureInfo.isWakeUp = true;
                wakeUpParsed = true;
                break;
            case 'N':
                mFeatureInfo.isWakeUp = false;
                wakeUpParsed = true;
                break;
            default:
                LOG_E << "Undefined wake up designation " << segments[3] << LOG_ENDL;
        }
    }

    int ret = typeParsed && reportingModeParsed && permissionParsed && wakeUpParsed;
    if (!ret) {
        LOG_D << "detectAndroidCustomSensor typeParsed: " << typeParsed
              << " reportingModeParsed: "  << reportingModeParsed
              << " permissionParsed: " << permissionParsed
              << " wakeUpParsed: " << wakeUpParsed << LOG_ENDL;
    }
    return ret;
}

bool HidRawSensor::findSensorControlUsage(const std::vector<HidParser::ReportPacket> &packets) {
    using namespace Hid::Sensor::PropertyUsage;
    using namespace Hid::Sensor::RawMinMax;

    //REPORTING_STATE
    const HidParser::ReportItem *reportingState
            = find(packets, REPORTING_STATE, HidParser::REPORT_TYPE_FEATURE);

    if (reportingState == nullptr
            || !reportingState->isByteAligned()
            || reportingState->bitSize != 8
            || reportingState->minRaw != REPORTING_STATE_MIN
            || reportingState->maxRaw != REPORTING_STATE_MAX) {
        LOG_W << "Cannot find valid reporting state feature" << LOG_ENDL;
    } else {
        mReportingStateId = reportingState->id;
        mReportingStateOffset = reportingState->bitOffset / 8;
    }

    //POWER_STATE
    const HidParser::ReportItem *powerState
            = find(packets, POWER_STATE, HidParser::REPORT_TYPE_FEATURE);
    if (powerState == nullptr
            || !powerState->isByteAligned()
            || powerState->bitSize != 8
            || powerState->minRaw != POWER_STATE_MIN
            || powerState->maxRaw != POWER_STATE_MAX) {
        LOG_W << "Cannot find valid power state feature" << LOG_ENDL;
    } else {
        mPowerStateId = powerState->id;
        mPowerStateOffset = powerState->bitOffset / 8;
    }

    //REPORT_INTERVAL
    const HidParser::ReportItem *reportInterval
            = find(packets, REPORT_INTERVAL, HidParser::REPORT_TYPE_FEATURE);
    if (reportInterval == nullptr
            || !reportInterval->isByteAligned()
            || reportInterval->minRaw < 0
            || (reportInterval->bitSize != 16 && reportInterval->bitSize != 32)) {
        LOG_W << "Cannot find valid report interval feature" << LOG_ENDL;
    } else {
        mReportIntervalId = reportInterval->id;
        mReportIntervalOffset = reportInterval->bitOffset / 8;
        mReportIntervalSize = reportInterval->bitSize / 8;

        mFeatureInfo.minDelay = std::max(static_cast<int64_t>(1), reportInterval->minRaw) * 1000;
        mFeatureInfo.maxDelay = std::min(static_cast<int64_t>(1000000),
                                    reportInterval->maxRaw) * 1000; // maximum 1000 second
    }
    return true;
    return (mPowerStateId >= 0 || mReportingStateId >= 0) && mReportIntervalId >= 0;
}

const sensor_t* HidRawSensor::getSensor() const {
    return &mSensor;
}

void HidRawSensor::getUuid(uint8_t* uuid) const {
    memcpy(uuid, mFeatureInfo.uuid, sizeof(mFeatureInfo.uuid));
}

int HidRawSensor::enable(bool enable) {
    using namespace Hid::Sensor::StateValue;
    SP(HidDevice) device = PROMOTE(mDevice);

    if (device == nullptr) {
        return NO_INIT;
    }

    if (enable == mEnabled) {
        return NO_ERROR;
    }

    std::vector<uint8_t> buffer;
    bool setPowerOk = true;
    if (mPowerStateId >= 0) {
        setPowerOk = false;
        uint8_t id = static_cast<uint8_t>(mPowerStateId);
        if (device->getFeature(id, &buffer)
                && buffer.size() > mPowerStateOffset) {
            buffer[mPowerStateOffset] = enable ? POWER_STATE_FULL_POWER : POWER_STATE_POWER_OFF;
            setPowerOk = device->setFeature(id, buffer);
        } else {
            LOG_E << "enable: changing POWER STATE failed" << LOG_ENDL;
        }
    }

    bool setReportingOk = true;
    if (mReportingStateId >= 0) {
        setReportingOk = false;
        uint8_t id = static_cast<uint8_t>(mReportingStateId);
        if (device->getFeature(id, &buffer)
                && buffer.size() > mReportingStateOffset) {
            buffer[mReportingStateOffset]
                    = enable ? REPORTING_STATE_ALL_EVENT : REPORTING_STATE_NO_EVENT;
            setReportingOk = device->setFeature(id, buffer);
        } else {
            LOG_E << "enable: changing REPORTING STATE failed" << LOG_ENDL;
        }
    }

    if (setPowerOk && setReportingOk) {
        mEnabled = enable;
        return NO_ERROR;
    } else {
        return INVALID_OPERATION;
    }
}

int HidRawSensor::batch(int64_t samplingPeriod, int64_t batchingPeriod) {
    SP(HidDevice) device = PROMOTE(mDevice);
    if (device == nullptr) {
        return NO_INIT;
    }

    if (samplingPeriod < 0 || batchingPeriod < 0) {
        return BAD_VALUE;
    }

    bool needRefresh = mSamplingPeriod != samplingPeriod || mBatchingPeriod != batchingPeriod;
    std::vector<uint8_t> buffer;

    bool ok = true;
    if (needRefresh && mReportIntervalId >= 0) {
        ok = false;
        uint8_t id = static_cast<uint8_t>(mReportIntervalId);
        if (device->getFeature(id, &buffer)
                && buffer.size() >= mReportIntervalOffset + mReportIntervalSize) {
            int64_t periodMs = samplingPeriod / 1000000; //ns -> ms
            switch (mReportIntervalSize) {
                case sizeof(uint16_t):
                    periodMs = std::min(periodMs, static_cast<int64_t>(UINT16_MAX));
                    buffer[mReportIntervalOffset] = periodMs & 0xFF;
                    buffer[mReportIntervalOffset + 1] = (periodMs >> 8) & 0xFF;
                    break;
                case sizeof(uint32_t):
                    periodMs = std::min(periodMs, static_cast<int64_t>(UINT32_MAX));
                    buffer[mReportIntervalOffset] = periodMs & 0xFF;
                    buffer[mReportIntervalOffset + 1] = (periodMs >> 8) & 0xFF;
                    buffer[mReportIntervalOffset + 2] = (periodMs >> 16) & 0xFF;
                    buffer[mReportIntervalOffset + 3] = (periodMs >> 24) & 0xFF;
                    break;
            }
            ok = device->setFeature(id, buffer);
        }
    }

    if (ok) {
        mSamplingPeriod = samplingPeriod;
        mBatchingPeriod = batchingPeriod;
        return NO_ERROR;
    } else {
        return INVALID_OPERATION;
    }
}

void HidRawSensor::handleInput(uint8_t id, const std::vector<uint8_t> &message) {
    if (id != mInputReportId || mEnabled == false) {
        return;
    }
    sensors_event_t event = {
        .version = sizeof(event),
        .sensor = -1,
        .type = mSensor.type
    };
    bool valid = true;
    for (const auto &rec : mTranslateTable) {
        int64_t v = (message[rec.byteOffset + rec.byteSize - 1] & 0x80) ? -1 : 0;
        for (int i = static_cast<int>(rec.byteSize) - 1; i >= 0; --i) {
            v = (v << 8) | message[rec.byteOffset + i]; // HID is little endian
        }

        switch (rec.type) {
            case TYPE_FLOAT:
                if (v > rec.maxValue || v < rec.minValue) {
                    valid = false;
                }
                event.data[rec.index] = rec.a * (v + rec.b);
                break;
            case TYPE_INT64:
                if (v > rec.maxValue || v < rec.minValue) {
                    valid = false;
                }
                event.u64.data[rec.index] = v + rec.b;
                break;
            case TYPE_ACCURACY:
                event.magnetic.status = (v & 0xFF) + rec.b;
                break;
        }
    }
    if (!valid) {
        LOG_V << "Range error observed in decoding, discard" << LOG_ENDL;
    }
    event.timestamp = -1;
    generateEvent(event);
}

std::string HidRawSensor::dump() const {
    std::ostringstream ss;
    ss << "Feature Values " << LOG_ENDL
          << "  name: " << mFeatureInfo.name << LOG_ENDL
          << "  vendor: " << mFeatureInfo.vendor << LOG_ENDL
          << "  permission: " << mFeatureInfo.permission << LOG_ENDL
          << "  typeString: " << mFeatureInfo.typeString << LOG_ENDL
          << "  type: " << mFeatureInfo.type << LOG_ENDL
          << "  maxRange: " << mFeatureInfo.maxRange << LOG_ENDL
          << "  resolution: " << mFeatureInfo.resolution << LOG_ENDL
          << "  power: " << mFeatureInfo.power << LOG_ENDL
          << "  minDelay: " << mFeatureInfo.minDelay << LOG_ENDL
          << "  maxDelay: " << mFeatureInfo.maxDelay << LOG_ENDL
          << "  fifoSize: " << mFeatureInfo.fifoSize << LOG_ENDL
          << "  fifoMaxSize: " << mFeatureInfo.fifoMaxSize << LOG_ENDL
          << "  reportModeFlag: " << mFeatureInfo.reportModeFlag << LOG_ENDL
          << "  isWakeUp: " << (mFeatureInfo.isWakeUp ? "true" : "false") << LOG_ENDL
          << "  uniqueId: " << mFeatureInfo.uniqueId << LOG_ENDL
          << "  uuid: ";

    ss << std::hex << std::setfill('0');
    for (auto d : mFeatureInfo.uuid) {
          ss << std::setw(2) << static_cast<int>(d) << " ";
    }
    ss << std::dec << std::setfill(' ') << LOG_ENDL;

    ss << "Input report id: " << mInputReportId << LOG_ENDL;
    for (const auto &t : mTranslateTable) {
        ss << "  type, index: " << t.type << ", " << t.index
              << "; min,max: " << t.minValue << ", " << t.maxValue
              << "; byte-offset,size: " << t.byteOffset << ", " << t.byteSize
              << "; scaling,bias: " << t.a << ", " << t.b << LOG_ENDL;
    }

    ss << "Control features: " << LOG_ENDL;
    ss << "  Power state ";
    if (mPowerStateId >= 0) {
        ss << "found, id: " << mPowerStateId
              << " offset: " << mPowerStateOffset << LOG_ENDL;
    } else {
        ss << "not found" << LOG_ENDL;
    }

    ss << "  Reporting state ";
    if (mReportingStateId >= 0) {
        ss << "found, id: " << mReportingStateId
              << " offset: " << mReportingStateOffset << LOG_ENDL;
    } else {
        ss << "not found" << LOG_ENDL;
    }

    ss << "  Report interval ";
    if (mReportIntervalId >= 0) {
        ss << "found, id: " << mReportIntervalId
              << " offset: " << mReportIntervalOffset
              << " size: " << mReportIntervalSize << LOG_ENDL;
    } else {
        ss << "not found" << LOG_ENDL;
    }
    return ss.str();
}

} // namespace SensorHalExt
} // namespace android
