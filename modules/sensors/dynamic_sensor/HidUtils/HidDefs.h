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
#ifndef HIDUTIL_HIDDEFS_H_
#define HIDUTIL_HIDDEFS_H_

namespace HidUtil {

// HID specification constants definition
//
// Definitions are from HID specification v1.11, which can be obtained from http://www.usb.org
//
// Preferred namespace for namespace restriction than enum class as enum class has strong type
// which is inconvenient in a parser, in which input binary values has to be compared with these
// definitions frequnetly.
namespace HidDef {
// Hid spec 6.2.2.3
namespace TagType {
enum {
    MAIN,
    GLOBAL,
    LOCAL,
    RESERVED
};
} // namespace TagType

// HID spec 6.2.2.4
namespace ReportFlag {
enum {
    DATA_CONST = 1,
    ARRAY_VARIABLE = 2,
    WRAP = 4,
    NONLINEAR = 8,
    NO_PREFERRED = 0x10,
    NULL_STATE = 0x20,
    VOLATILE = 0x40,
    // bit 7 reserved
    BUFFERED_BYTES = 0x100
};
} // namespace ReportFlag

// HID spec 6.2.2.5
namespace MainTag {
enum {
    INPUT = 8,
    OUTPUT = 9,
    COLLECTION = 10,
    FEATURE = 11,
    END_COLLECTION = 12,
    LONG_ITEM = 15,
};
} // namespace MainTag

// HID spec 6.2.2.6
namespace CollectionType {
enum {
    PHYSICAL = 0,
    APPLICATION,
    LOGICAL,
    REPORT,
    NAMED_ARRAY,
    USAGE_SWITCH,
    USAGE_MODIFIER
};
} // namespace CollectionType

// HID spec 6.2.2.7
namespace GlobalTag {
enum {
    USAGE_PAGE,
    LOGICAL_MINIMUM,
    LOGICAL_MAXIMUM,
    PHYSICAL_MINIMUM,
    PHYSICAL_MAXIMUM,
    UNIT_EXPONENT,
    UNIT,
    REPORT_SIZE,
    REPORT_ID,
    REPORT_COUNT,
    PUSH,
    POP
};
} //namespace GlobalTag

// HID spec 6.2.2.8
namespace LocalTag {
enum HidLocalTag {
    USAGE,
    USAGE_MINIMUM,
    USAGE_MAXIMUM,
    DESIGNATOR_INDEX,
    DESIGNATOR_MINIMUM,
    DESIGNATOR_MAXIMUM,
    // there is a hole here in the spec
    STRING_INDEX = 7,
    STRING_MINIMUM,
    STRING_MAXIMUM,
    DELIMITOR
};
} // namespace LocalTag

} //namespace HidDef
} //namespace HidUtil

#endif // HIDUTIL_HIDDEFS_H_
