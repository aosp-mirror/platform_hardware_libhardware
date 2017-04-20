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
#include "HidReport.h"
#include "HidDefs.h"
#include <cmath>
#include <sstream>
#include <iomanip>

namespace HidUtil {
HidReport::HidReport(uint32_t type, uint32_t data,
                     const HidGlobal &global, const HidLocal &local)
        : mReportType(type),
          mFlag(data),
          mUsagePage(global.usagePage.get(0)),   // default value 0
          mUsage(local.getUsage(0)),
          mUsageVector(local.usage),
          mLogicalMin(global.logicalMin.get(0)), // default value 0
          mLogicalMax(global.logicalMax.get(0)),
          mReportSize(global.reportSize),
          mReportCount(global.reportCount),
          mPhysicalMin(global.physicalMin),
          mPhysicalMax(global.physicalMax),
          mExponent(global.exponent),
          mUnit(global.unit),
          mReportId(global.reportId) { }

std::string HidReport::getStringType() const {
    return reportTypeToString(mReportType);
}

std::string HidReport::reportTypeToString(int type) {
    using namespace HidDef::MainTag;
    switch(type) {
        case INPUT:
            return "INPUT";
        case OUTPUT:
            return "OUTPUT";
        case FEATURE:
            return "FEATURE";
        default:
            return "<<UNKNOWN>>";
    }
}

double HidReport::getExponentValue() const {
    if (!mExponent.isSet()) {
        return 1;
    }
    // default exponent is 0
    int exponentInt = mExponent.get(0);
    if (exponentInt > 15 || exponentInt < 0) {
        return NAN;
    }
    return pow(10.0, static_cast<double>((exponentInt <= 7) ? exponentInt : exponentInt - 16));
}

std::string HidReport::getExponentString() const {
    int exponentInt = mExponent.get(0);
    if (exponentInt > 15 || exponentInt < 0) {
        return "[error]";
    }
    return std::string("x10^")
            + std::to_string((exponentInt <= 7) ? exponentInt : exponentInt - 16);
}

std::string HidReport::getUnitString() const {
    if (!mUnit.isSet()) {
        return "default";
    }
    return "[not implemented]";

    std::ostringstream ret;
    ret << std::hex << std::setfill('0') << std::setw(2) << mUnit.get(0);
    return ret.str();
}

std::string HidReport::getFlagString() const {
    using namespace HidDef::ReportFlag;
    std::string ret;
    ret += (mFlag & DATA_CONST) ? "Const " : "Data ";
    ret += (mFlag & ARRAY_VARIABLE) ? "Variable " : "Array ";
    ret += (mFlag & WRAP) ? "Wrap " : "";
    ret += (mFlag & NONLINEAR) ? "Nonlinear " : "";
    ret += (mFlag & NO_PREFERRED) ? "NoPreferred " : "";
    ret += (mFlag & NULL_STATE) ? "NullState " : "";
    ret += (mFlag & VOLATILE) ? "Volatile " : "";
    ret += (mFlag & BUFFERED_BYTES) ? "BufferedBytes " : "";
    return ret;
}

// isArray() will return true for reports that may contains multiple values, e.g. keyboard scan
// code, which can have multiple value, each denoting a key pressed down at the same time. It will
// return false if repor represent a vector or matrix.
//
// This slightly deviates from HID's definition, it is more convenient this way as matrix/vector
// input is treated similarly as variables.
bool HidReport::isArray() const {
    using namespace HidDef::ReportFlag;
    return (mFlag & ARRAY_VARIABLE) == 0 && mIsCollapsed;
}

bool HidReport::isVariable() const {
    return !isArray();
}

bool HidReport::isData() const {
    using namespace HidDef::ReportFlag;
    return (mFlag & DATA_CONST) == 0;
}

std::ostream& operator<<(std::ostream& os, const HidReport& h) {
    os << h.getStringType() << ", "
       << "usage: " << std::hex << h.getFullUsage() << std::dec << ", ";

    if (h.isData()) {
        auto range = h.getLogicalRange();
        os << "logMin: " << range.first << ", "
           << "logMax: " << range.second << ", ";

        if (range == h.getPhysicalRange()) {
            os << "phy===log, ";
        } else {
            range = h.getPhysicalRange();
            os << "phyMin: " << range.first << ", "
               << "phyMax: " << range.second << ", ";
        }

        if (h.isArray()) {
            os << "map: (" << std::hex;
            for (auto i : h.getUsageVector()) {
                os << i << ",";
            }
            os << "), " << std::dec;
        }

        os << "exponent: " << h.getExponentString() << ", "
           << "unit: " << h.getUnitString() << ", ";
    } else {
        os << "constant: ";
    }
    os << "size: " << h.getSize() << "bit x " << h.getCount() << ", "
       << "id: " << h.mReportId;

    return os;
}

std::pair<int64_t, int64_t> HidReport::getLogicalRange() const {
    int64_t a = mLogicalMin;
    int64_t b = mLogicalMax;

    if (a > b) {
        // might be unsigned
        a = a & ((static_cast<int64_t>(1) << getSize()) - 1);
        b = b & ((static_cast<int64_t>(1) << getSize()) - 1);
        if (a > b) {
            // bad hid descriptor
            return {0, 0};
        }
    }
    return {a, b};
}

std::pair<int64_t, int64_t> HidReport::getPhysicalRange() const {
    if (!(mPhysicalMin.isSet() && mPhysicalMax.isSet())) {
        // physical range undefined, use logical range
        return getLogicalRange();
    }

    int64_t a = mPhysicalMin.get(0);
    int64_t b = mPhysicalMax.get(0);

    if (a > b) {
        a = a & ((static_cast<int64_t>(1) << getSize()) - 1);
        b = b & ((static_cast<int64_t>(1) << getSize()) - 1);
        if (a > b) {
            return {0, 0};
        }
    }
    return {a, b};
}

unsigned int HidReport::getFullUsage() const {
    return mUsage | (mUsagePage << 16);
}

size_t HidReport::getSize() const {
    return mReportSize;
}

size_t HidReport::getCount() const {
    return mReportCount;
}

unsigned int HidReport::getUnit() const {
    return mUnit.get(0); // default unit is 0 means default unit
}

unsigned HidReport::getReportId() const {
    // if report id is not specified, it defaults to zero
    return mReportId.get(0);
}

unsigned HidReport::getType() const {
    return mReportType;
}

void HidReport::setCollapsed(uint32_t fullUsage) {
    mUsage = fullUsage & 0xFFFF;
    mUsagePage = fullUsage >> 16;
    mIsCollapsed = true;
}

const std::vector<unsigned int>& HidReport::getUsageVector() const {
    return mUsageVector;
}
} // namespace HidUtil
