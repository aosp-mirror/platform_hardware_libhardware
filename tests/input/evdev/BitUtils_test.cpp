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

#include "BitUtils.h"

#include <gtest/gtest.h>

namespace android {
namespace tests {

TEST(BitInRange, testInvalidRange) {
    uint8_t arr[2] = { 0xff, 0xff };
    EXPECT_FALSE(testBitInRange(arr, 0, 0));
    EXPECT_FALSE(testBitInRange(arr, 1, 0));
}

TEST(BitInRange, testNoBits) {
    uint8_t arr[1];
    arr[0] = 0;
    EXPECT_FALSE(testBitInRange(arr, 0, 8));
}

TEST(BitInRange, testOneBit) {
    uint8_t arr[1];
    for (int i = 0; i < 8; ++i) {
        arr[0] = 1 << i;
        EXPECT_TRUE(testBitInRange(arr, 0, 8));
    }
}

TEST(BitInRange, testZeroStart) {
    uint8_t arr[1] = { 0x10 };
    for (int i = 0; i < 5; ++i) {
        EXPECT_FALSE(testBitInRange(arr, 0, i));
    }
    for (int i = 5; i <= 8; ++i) {
        EXPECT_TRUE(testBitInRange(arr, 0, i));
    }
}

TEST(BitInRange, testByteBoundaryEnd) {
    uint8_t arr[1] = { 0x10 };
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(testBitInRange(arr, i, 8));
    }
    for (int i = 5; i <= 8; ++i) {
        EXPECT_FALSE(testBitInRange(arr, i, 8));
    }
}

TEST(BitInRange, testMultiByteArray) {
    // bits set: 11 and 16
    uint8_t arr[3] = { 0x00, 0x08, 0x01 };
    for (int start = 0; start < 24; ++start) {
        for (int end = start + 1; end <= 24; ++end) {
            if (start > 16 || end <= 11 || (start > 11 && end <= 16)) {
                EXPECT_FALSE(testBitInRange(arr, start, end))
                    << "range = (" << start << ", " << end << ")";
            } else {
                EXPECT_TRUE(testBitInRange(arr, start, end))
                    << "range = (" << start << ", " << end << ")";
            }
        }
    }
}

}  // namespace tests
}  // namespace android
