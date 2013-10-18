/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef VENDOR_TAGS_H_
#define VENDOR_TAGS_H_

#include <hardware/camera_common.h>
#include <system/camera_metadata.h>

namespace default_camera_hal {

// VendorTags contains all vendor-specific metadata tag functionality
class VendorTags {
    public:
        VendorTags();
        ~VendorTags();

        // Vendor Tags Operations (see <hardware/camera_common.h>)
        int getTagCount(const vendor_tag_ops_t* ops);
        void getAllTags(const vendor_tag_ops_t* ops, uint32_t* tag_array);
        const char* getSectionName(const vendor_tag_ops_t* ops, uint32_t tag);
        const char* getTagName(const vendor_tag_ops_t* ops, uint32_t tag);
        int getTagType(const vendor_tag_ops_t* ops, uint32_t tag);

    private:
        // Total number of vendor tags
        int mTagCount;
};

// Tag sections start at the beginning of vendor tags (0x8000_0000)
// See <system/camera_metadata.h>
enum {
    DEMO_WIZARDRY,
    DEMO_SORCERY,
    DEMO_MAGIC,
    DEMO_SECTION_COUNT
};

// Each section starts at increments of 0x1_0000
enum {
    DEMO_WIZARDRY_START = (DEMO_WIZARDRY + VENDOR_SECTION) << 16,
    DEMO_SORCERY_START  = (DEMO_SORCERY  + VENDOR_SECTION) << 16,
    DEMO_MAGIC_START    = (DEMO_MAGIC    + VENDOR_SECTION) << 16,
};

// Vendor Tag values, start value begins each section
enum {
    DEMO_WIZARDRY_DIMENSION_SIZE = DEMO_WIZARDRY_START,
    DEMO_WIZARDRY_DIMENSIONS,
    DEMO_WIZARDRY_FAMILIAR,
    DEMO_WIZARDRY_FIRE,
    DEMO_WIZARDRY_END,

    DEMO_SORCERY_DIFFICULTY = DEMO_SORCERY_START,
    DEMO_SORCERY_LIGHT,
    DEMO_SORCERY_END,

    DEMO_MAGIC_CARD_TRICK = DEMO_MAGIC_START,
    DEMO_MAGIC_LEVITATION,
    DEMO_MAGIC_END,
};
} // namespace default_camera_hal

#endif // VENDOR_TAGS_H_
