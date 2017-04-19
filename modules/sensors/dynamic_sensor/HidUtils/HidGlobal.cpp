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
#include "HidGlobal.h"
#include "HidLog.h"

namespace HidUtil {
using namespace HidDef::GlobalTag;

bool HidGlobal::append(const HidItem &i) {
    using namespace HidDef::TagType;
    if (i.type != GLOBAL) {
        LOG_E << "HidGlobal::append cannot process tag that is not global, " << i << LOG_ENDL;
        return false;
    }

    if (i.tag == PUSH || i.tag == POP) {
        LOG_E << "PUSH and POP should be handled in HidGlobalStack, " << i << LOG_ENDL;
        return false;
    }

    int signedInteger;
    unsigned unsignedInteger;
    bool signedError = !i.dataAsSigned(&signedInteger);
    bool unsignedError = !i.dataAsUnsigned(&unsignedInteger);

    bool valueError = false;
    bool ret = true;
    switch (i.tag) {
        case USAGE_PAGE:
            usagePage = unsignedInteger;
            valueError = unsignedError;
            break;
        case LOGICAL_MINIMUM:
            logicalMin = signedInteger;
            valueError = signedError;
            break;
        case LOGICAL_MAXIMUM:
            logicalMax = signedInteger;
            valueError = signedError;
            break;
        case PHYSICAL_MINIMUM:
            physicalMin = signedInteger;
            valueError = signedError;
            break;
        case PHYSICAL_MAXIMUM:
            physicalMax = signedInteger;
            valueError = signedError;
            break;
        case UNIT_EXPONENT:
            exponent = unsignedInteger;
            valueError = unsignedError;
            break;
        case UNIT:
            unit = unsignedInteger;
            valueError = unsignedError;
            break;
        case REPORT_SIZE:
            reportSize = unsignedInteger;
            valueError = unsignedError;
            break;
        case REPORT_ID:
            reportId = unsignedInteger;
            valueError = unsignedError;
            break;
        case REPORT_COUNT:
            reportCount = unsignedInteger;
            valueError = unsignedError;
            break;
        default:
            LOG_E << "unknown global tag, " << i << LOG_ENDL;
            ret = false;
    }

    if (valueError) {
        LOG_E << "Cannot get signed / unsigned data at " << i << LOG_ENDL;
        ret = false;
    }
    return ret;
}

bool HidGlobalStack::append(const HidItem &i) {
    using namespace HidDef::TagType;
    if (i.type != GLOBAL) {
        return false;
    }

    bool ret = true;
    if (i.tag == PUSH) {
        mStack.push_back(top());
    } else if (i.tag == POP) {
        mStack.pop_back();
        if (mStack.size() == 0) {
            mStack.push_back(HidGlobal()); // fail-safe
            ret = false;
        }
    } else {
        ret = mStack.back().append(i);
    }
    return ret;
}

HidGlobalStack::HidGlobalStack() {
    // default element
    mStack.push_back(HidGlobal());
}

const HidGlobal& HidGlobalStack::top() const {
    return mStack.back();
}

} // namespace HidUtil
