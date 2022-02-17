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
#ifndef HIDUTIL_HIDUTILS_H_
#define HIDUTIL_HIDUTILS_H_

#include <stddef.h>

namespace HidUtil {

void copyBits(const void *src, void *dst, size_t dst_size,
              unsigned int src_bit_offset, unsigned int dst_bit_offset,
              unsigned int bit_count);

} // namespace HidUtil

#endif // HIDUTIL_HIDUTILS_H_
