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
#ifndef HIDUTIL_HIDITEM_H_
#define HIDUTIL_HIDITEM_H_

#include <cstdlib>
#include <vector>
#include <istream>
#include <ostream>

namespace HidUtil {

struct HidItem {
    bool valid;
    unsigned int type;
    unsigned int tag;
    ssize_t offset;
    ssize_t byteSize;
    std::vector<uint8_t> data;

    bool dataAsUnsigned(unsigned int *out) const;
    bool dataAsSigned(int *out) const;

    // friend stream functions
    friend std::istream& operator>>(std::istream &is, HidItem &h);
    friend std::ostream& operator<<(std::ostream &os, const HidItem &h);

    // tokenize from a unsigned char vector
    static std::vector<HidItem> tokenize(const std::vector<uint8_t> &descriptor);
    static std::vector<HidItem> tokenize(const uint8_t *begin, size_t size);
    static std::vector<HidItem> tokenize(std::istream &is);
};

// parsing in from binary stream
std::istream& operator>>(std::istream &is, HidUtil::HidItem &h);

// output as human readable string to stream
std::ostream& operator<<(std::ostream &os, const HidUtil::HidItem &h);
} //namespace HidUtil

#endif // HIDUTIL_HIDITEM_H_
