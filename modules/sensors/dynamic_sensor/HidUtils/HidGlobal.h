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
#ifndef HIDUTIL_HIDGLOBAL_H_
#define HIDUTIL_HIDGLOBAL_H_

#include "HidItem.h"
#include "TriState.h"

namespace HidUtil {
// A set of global states that parser has to keep track during parsing.
// They are all specified in HID spec v1.11 section 6.2.2.7
struct HidGlobal {
    // add a token and change global states, returns value indicates if operation is successful
    bool append(const HidItem &i);

    tri_uint usagePage;
    tri_int  logicalMin;
    tri_int  logicalMax;
    tri_int  physicalMin;
    tri_int  physicalMax;
    tri_uint exponent;
    tri_uint unit;
    tri_uint reportSize;
    tri_uint reportId;
    tri_uint reportCount;
};

// HID specs allows PUSH and POP to save a snapshot of current global states and come back to the
// saved sates later. HidStack manages this logic. Note that PUSH and POP are also HidItems, so
// there is no explicit push and pop function in this stack implementation.
class HidGlobalStack {
public:
    HidGlobalStack();

    // add a token and change global states, returns value indicates if operation is successful
    // it the token is push/pop, the stack push/pop accordingly.
    bool append(const HidItem &i);

    // get reference to top element on the stack
    const HidGlobal& top() const;
private:
    std::vector<HidGlobal> mStack;
};

} //namespace HidUtil

#endif // HIDUTIL_HIDGLOABL_H_
