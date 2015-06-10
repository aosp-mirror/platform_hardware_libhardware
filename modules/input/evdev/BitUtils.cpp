/*
 * Copyright (C) 2015 The Android Open Source Project
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

#define LOG_TAG "BitUtils"
//#define LOG_NDEBUG 0

#include "BitUtils.h"

#include <utils/Log.h>

// Enables debug output for hasKeyInRange
#define DEBUG_KEY_RANGE 0

namespace android {

#if DEBUG_KEY_RANGE
static const char* bitstrings[16] = {
    "0000", "0001", "0010", "0011",
    "0100", "0101", "0110", "0111",
    "1000", "1001", "1010", "1011",
    "1100", "1101", "1110", "1111",
};
#endif

bool testBitInRange(const uint8_t arr[], size_t start, size_t end) {
#if DEBUG_KEY_RANGE
    ALOGD("testBitInRange(%d, %d)", start, end);
#endif
    // Invalid range! This is nonsense; just say no.
    if (end <= start) return false;

    // Find byte array indices. The end is not included in the range, nor is
    // endIndex. Round up for endIndex.
    size_t startIndex = start / 8;
    size_t endIndex = (end + 7) / 8;
#if DEBUG_KEY_RANGE
    ALOGD("startIndex=%d, endIndex=%d", startIndex, endIndex);
#endif
    for (size_t i = startIndex; i < endIndex; ++i) {
        uint8_t bits = arr[i];
        uint8_t mask = 0xff;
#if DEBUG_KEY_RANGE
        ALOGD("block %04d: %s%s", i, bitstrings[bits >> 4], bitstrings[bits & 0x0f]);
#endif
        if (bits) {
            // Mask off bits before our start bit
            if (i == startIndex) {
                mask &= 0xff << (start % 8);
            }
            // Mask off bits after our end bit
            if (i == endIndex - 1 && (end % 8)) {
                mask &= 0xff >> (8 - (end % 8));
            }
#if DEBUG_KEY_RANGE
            ALOGD("mask: %s%s", bitstrings[mask >> 4], bitstrings[mask & 0x0f]);
#endif
            // Test the index against the mask
            if (bits & mask) return true;
        }
    }
    return false;
}
}  // namespace android
