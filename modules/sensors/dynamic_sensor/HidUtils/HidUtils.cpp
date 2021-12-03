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
#include <stdint.h>
#include <algorithm>

namespace HidUtil {

void copyBits(const void *src, void *dst, size_t dst_size,
              unsigned int src_bit_offset, unsigned int dst_bit_offset,
              unsigned int bit_count) {
    const uint8_t *p_src;
    uint8_t       *p_dst;
    uint8_t        dst_mask;
    unsigned int   bits_rem;
    unsigned int   bit_block_count;

    // Do nothing if copying past the end of the destination buffer.
    if ((static_cast<size_t>(dst_bit_offset) > (8 * dst_size)) ||
        (static_cast<size_t>(bit_count) > (8 * dst_size)) ||
        (static_cast<size_t>(dst_bit_offset + bit_count) > (8 * dst_size))) {
        return;
    }

    // Copy bits from source to destination buffer.
    p_src = static_cast<const uint8_t*>(src) + (src_bit_offset / 8);
    src_bit_offset = src_bit_offset % 8;
    p_dst = static_cast<uint8_t*>(dst) + (dst_bit_offset / 8);
    dst_bit_offset = dst_bit_offset % 8;
    bits_rem = bit_count;
    while (bits_rem > 0) {
        // Determine the size of the next block of bits to copy. The block must
        // not cross a source or desintation byte boundary.
        bit_block_count = std::min(bits_rem, 8 - src_bit_offset);
        bit_block_count = std::min(bit_block_count, 8 - dst_bit_offset);

        // Determine the destination bit block mask.
        dst_mask = ((1 << bit_block_count) - 1) << dst_bit_offset;

        // Copy the block of bits.
        *p_dst = (*p_dst & ~dst_mask) |
                 (((*p_src >> src_bit_offset) << dst_bit_offset) & dst_mask);

        // Advance past the block of copied bits in the source.
        src_bit_offset += bit_block_count;
        p_src += src_bit_offset / 8;
        src_bit_offset = src_bit_offset % 8;

        // Advance past the block of copied bits in the destination.
        dst_bit_offset += bit_block_count;
        p_dst += dst_bit_offset / 8;
        dst_bit_offset = dst_bit_offset % 8;

        // Decrement the number of bits remaining.
        bits_rem -= bit_block_count;
    }
}

} // namespace HidUtil
