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
#ifndef HIDUTIL_HIDREPORT_H_
#define HIDUTIL_HIDREPORT_H_

#include "HidGlobal.h"
#include "HidLocal.h"
#include "TriState.h"
#include <cstdint>
#include <memory>
#include <iostream>
#include <utility>

namespace HidUtil {

class HidParser;
class HidTreeNode;

// HidReport represent an input, output or feature report
class HidReport {
    friend std::ostream& operator<<(std::ostream& os, const HidReport& h);
public:
    HidReport(uint32_t type_, uint32_t data, const HidGlobal &global, const HidLocal &local);

    // This is called during parsing process when the parser regroups multi-valued report into one
    void setCollapsed(uint32_t fullUsage);

    // get report id
    unsigned int getReportId() const;
    // get type of report, return constant of HidDef::MainTag
    unsigned int getType() const;
    // Full sensor usage
    unsigned int getFullUsage() const;

    // binary properties
    bool isArray() const;
    bool isData() const;
    bool isVariable() const;

    // logical and physical value range
    std::pair<int64_t, int64_t> getLogicalRange() const;
    std::pair<int64_t, int64_t> getPhysicalRange() const;
    double getExponentValue() const;

    // return HID unit nibbles in an unsigned int
    unsigned int getUnit() const;

    // size in bits
    size_t getSize() const;
    // dimension (if it is vector/matrix) or number of concurrent input values
    // it is also used to calculate memory foot print
    size_t getCount() const;

    // for output to stream
    static std::string reportTypeToString(int type);
    std::string getStringType() const;
    std::string getExponentString() const;
    std::string getUnitString() const;
    std::string getFlagString() const;
    const std::vector<unsigned int>& getUsageVector() const;
private:
    bool mIsCollapsed;

    // mandatary fields
    unsigned int mReportType;
    unsigned int mFlag;
    unsigned int mUsagePage;
    unsigned int mUsage;
    std::vector<unsigned int> mUsageVector;

    int mLogicalMin;        // 32 bit is enough
    int mLogicalMax;
    unsigned int mReportSize;
    unsigned int mReportCount;

    // these below are optional
    tri_int mPhysicalMin;
    tri_int mPhysicalMax;

    tri_uint mExponent;
    tri_uint mUnit;
    tri_uint mReportId;
};

std::ostream& operator<<(std::ostream& os, const HidReport& h);
} // namespace HidUtil
#endif // HIDUTIL_HIDREPORT_H_
