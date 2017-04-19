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
#ifndef HIDUTIL_HIDLOCAL_H_
#define HIDUTIL_HIDLOCAL_H_

#include "HidItem.h"
#include "TriState.h"
#include <cstddef>
#include <vector>

namespace HidUtil {

// A set of local states that parser has to keep track during parsing.
// They are all specified in HID spec v1.11 section 6.2.2.8
struct HidLocal {
    // add a token to change local states, return value indicates if operation is successful
    bool append(const HidItem &i);
    // clear all local states. This need to be done after each main tag
    void clear();

    // multiple usage, designator or strings may exist for single input/output/feature report
    uint32_t getUsage(size_t index) const;
    uint32_t getDesignator(size_t index) const;
    uint32_t getString(size_t index) const;

    std::vector<uint32_t> usage;
    // keep track of usage min when expecting a usage max
    tri_uint usageMin;

    std::vector<uint32_t> designator;
    // keep track of designator min when expecting designator max
    tri_uint designatorMin;

    std::vector<uint32_t> string;
    // keep track of string min when expecting string max
    tri_uint stringMin;

    tri_uint delimeter;
};
} // namespace HidUtil

#endif // HIDUTIL_HIDLOCAL_H_
