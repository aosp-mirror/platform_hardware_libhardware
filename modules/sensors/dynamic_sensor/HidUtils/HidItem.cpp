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
#include "HidItem.h"
#include "HidDefs.h"
#include "StreamIoUtil.h"
#include <iostream>

namespace HidUtil {

bool HidItem::dataAsUnsigned(unsigned int *out) const  {
    if (data.size() > 4 || data.size() == 0) {
        return false;
    }

    *out = 0;
    int shift = 0;

    for (auto i : data) {
        *out |= (i << shift);
        shift += 8;
    }
    return true;
}

bool HidItem::dataAsSigned(int *out) const {
    unsigned int u;
    if (!dataAsUnsigned(&u)) {
        return false;
    }
    size_t bitSize_1 = data.size() * 8 - 1;
    unsigned int sign = u & (1 << bitSize_1);
    *out = u | ((sign == 0) ? 0 : ( ~0 << bitSize_1));
    return true;
}

std::vector<HidItem> HidItem::tokenize(const uint8_t *begin, size_t size) {
    // construct a stream
    charvectorbuf<unsigned char> buf(begin, size);
    std::istream is(&buf);
    return tokenize(is);
}

std::vector<HidItem> HidItem::tokenize(const std::vector<uint8_t> &descriptor) {
    // construct a stream
    charvectorbuf<unsigned char> buf(descriptor);
    std::istream is(&buf);
    return tokenize(is);
}

std::vector<HidItem> HidItem::tokenize(std::istream &is) {
    std::vector<HidItem> hidToken;

    // this is important to avoid skipping characters
    is.unsetf(std::ios_base::skipws);
    while (!is.eof()) {
        HidItem i;
        is >> i;
        if (i.valid) {
            hidToken.push_back(i);
        } else {
            break;
        }
    }
    return hidToken;
}

std::istream& operator>>(std::istream &is, HidUtil::HidItem &h) {
    using namespace HidUtil::HidDef::MainTag;
    using namespace HidUtil::HidDef::TagType;

    h.valid = false;
    h.offset = is.tellg();
    h.byteSize = 0;
    unsigned char first;
    is >> first;
    if (!is.eof()) {
        static const size_t lenTable[] = { 0, 1, 2, 4 };
        size_t len = lenTable[first & 0x3]; // low 2 bits are length descriptor
        h.tag = (first >> 4);
        h.type = (first & 0xC) >> 2;

        if (h.tag == LONG_ITEM && h.type == RESERVED) { // long item
            //long item
            unsigned char b = 0;
            is >> b;
            len = b;
            is >> b;
            h.tag = b;
        }

        h.data.resize(len);
        for (auto &i : h.data) {
            if (is.eof()) {
                break;
            }
            is >> i;
        }
        h.byteSize = (ssize_t) is.tellg() - h.offset;
        h.valid = !is.eof();
    }

    return is;
}

std::ostream& operator<<(std::ostream &os, const HidUtil::HidItem &h) {
    os << "offset: " << h.offset << ", size: " << h.byteSize
       << ", tag: " << h.tag << ", type: " << h.type << ", data: ";
    if (h.data.empty()) {
        os << "[empty]";
    } else {
        os << h.data.size() << " byte(s) {";
        for (auto i : h.data) {
            os << (int) i << ", ";
        }
        os << "}";
    }
    return os;
}
} // namespace HidUtil
