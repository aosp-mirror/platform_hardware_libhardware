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
#include "HidDefs.h"
#include "HidLocal.h"
#include "HidLog.h"
#include <cstddef>

namespace HidUtil {

constexpr uint32_t INVALID_USAGE = 0xFFFF;
constexpr uint32_t INVALID_DESIGNATOR = 0xFFFF;
constexpr uint32_t INVALID_STRING = 0xFFFF;

uint32_t HidLocal::getUsage(size_t index) const {
    if (usage.empty()) {
        return INVALID_USAGE;
    }
    return (index >= usage.size()) ? usage.back() : usage[index];
}

uint32_t HidLocal::getDesignator(size_t index) const {
    if (designator.empty()) {
        return INVALID_DESIGNATOR;
    }
    return (index >= designator.size()) ? designator.back() : designator[index];
}

uint32_t HidLocal::getString(size_t index) const {
    if (string.empty()) {
        return INVALID_STRING;
    }
    return (index >= string.size()) ? string.back() : string[index];
}

void HidLocal::clear() {
    *this = HidLocal();
}

bool HidLocal::append(const HidItem &i) {
    using namespace HidDef::LocalTag;

    bool ret = true;
    unsigned unsignedInteger;
    bool unsignedError = !i.dataAsUnsigned(&unsignedInteger);
    bool valueError = false;

    switch (i.tag) {
        case USAGE:
            usage.push_back(unsignedInteger);
            valueError = unsignedError;
            break;
        case USAGE_MINIMUM:
            usageMin = unsignedInteger;
            valueError = unsignedError;
            break;
        case USAGE_MAXIMUM:
            if (!usageMin.isSet()) {
                LOG_E << "usage min not set when saw usage max " << i << LOG_ENDL;
                ret = false;
            } else {
                uint32_t usagemax = unsignedInteger;
                valueError = unsignedError;
                for (size_t j = usageMin.get(0); j <= usagemax; ++j) {
                    usage.push_back(j);
                }
                usageMin.clear();
            }
            break;
        case STRING_INDEX:
            string.push_back(unsignedInteger);
            valueError = unsignedError;
            break;
        case STRING_MINIMUM:
            stringMin = unsignedInteger;
            valueError = unsignedError;
            break;
        case STRING_MAXIMUM: {
            if (!usageMin.isSet()) {
                LOG_E << "string min not set when saw string max " << i << LOG_ENDL;
                ret = false;
            } else {
                uint32_t stringMax = unsignedInteger;
                valueError = unsignedError;
                for (size_t j = stringMin.get(0); j <= stringMax; ++j) {
                    string.push_back(j);
                }
                stringMin.clear();
            }
            break;
        }
        case DELIMITOR:
            delimeter = unsignedInteger;
            valueError = unsignedError;
            break;
        default:
            LOG_E << "unknown local tag, " << i << LOG_ENDL;
            ret = false;
    }
    if (valueError) {
        LOG_E << "Cannot get unsigned data at " << i << LOG_ENDL;
        ret = false;
    }
    return ret;
}
} //namespace HidUtil
