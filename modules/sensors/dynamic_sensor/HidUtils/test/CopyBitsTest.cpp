/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include "HidUtils.h"
#include <gtest/gtest.h>

using HidUtil::copyBits;

TEST(CopyBitsTest, CopyBits) {
    const struct {
        uint32_t src;
        uint32_t dst;
        int src_bit_offset;
        int dst_bit_offset;
        int bit_count;
        uint32_t expected_dst;
    } kTestVectorList[] = {
        { 0x00000005, 0x00000000,  0,  0,  8, 0x00000005 },
        { 0x00000005, 0x00000000,  0,  4,  8, 0x00000050 },
        { 0x0000000C, 0x00000020,  0,  4,  8, 0x000000C0 },
        { 0x00000005, 0x0000F02F,  0,  4,  8, 0x0000F05F },
        { 0x12345678, 0x87654321,  5, 11, 17, 0x8D159B21 },
        { 0x12345678, 0x87654321, 11,  5, 17, 0x8748D141 },
    };

    for (auto test_vector : kTestVectorList) {
        uint32_t dst = test_vector.dst;
        copyBits(&(test_vector.src), &dst, sizeof(dst),
                 test_vector.src_bit_offset, test_vector.dst_bit_offset,
                 test_vector.bit_count);
        EXPECT_EQ(test_vector.expected_dst, dst);
    }
}

TEST(CopyBitsTest, Overflow) {
    const struct {
        uint32_t src;
        uint32_t dst;
        unsigned int src_bit_offset;
        unsigned int dst_bit_offset;
        unsigned int bit_count;
        uint32_t expected_dst;
    } kTestVectorList[] = {
        { 0x000000FF, 0x00000000,  0,        0,        8, 0x000000FF },
        { 0x000000FF, 0x00000000,  0,       24,        8, 0xFF000000 },
        { 0x000000FF, 0x00000000,  0,       25,        8, 0x00000000 },
        { 0x000000FF, 0x00000000,  0,       32,        8, 0x00000000 },
        { 0x000000FF, 0x00000000,  0, UINT_MAX,        8, 0x00000000 },
        { 0x000000FF, 0x00000000,  0,        8, UINT_MAX, 0x00000000 },
    };

    for (auto test_vector : kTestVectorList) {
        uint32_t dst = test_vector.dst;
        copyBits(&(test_vector.src), &dst, sizeof(dst),
                 test_vector.src_bit_offset, test_vector.dst_bit_offset,
                 test_vector.bit_count);
        EXPECT_EQ(test_vector.expected_dst, dst);
    }
}

