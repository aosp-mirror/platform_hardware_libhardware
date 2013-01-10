/*
 * Copyright (C) 2012 The Android Open Source Project
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

#define LOG_NDEBUG 0
#define LOG_TAG "CameraMetadataTestFunctional"
#include "cutils/log.h"
#include "cutils/properties.h"
#include "utils/Errors.h"

#include "gtest/gtest.h"
#include "system/camera_metadata.h"
#include "hardware/hardware.h"
#include "hardware/camera2.h"

#include "Camera2Device.h"
#include "utils/StrongPointer.h"

#include <gui/CpuConsumer.h>
#include <gui/SurfaceTextureClient.h>

#include <string>

#include "CameraStreamFixture.h"
#include "TestExtensions.h"

namespace android {
namespace camera2 {
namespace tests {

//FIXME: dont hardcode
static CameraStreamParams METADATA_STREAM_PARAMETERS = {
    /*mFormat*/     HAL_PIXEL_FORMAT_YCrCb_420_SP,
    /*mHeapCount*/  2
};

class CameraMetadataTest
    : public ::testing::Test,
      public CameraStreamFixture {

public:
    CameraMetadataTest()
    : CameraStreamFixture(METADATA_STREAM_PARAMETERS) {
        TEST_EXTENSION_FORKING_CONSTRUCTOR;
    }

    ~CameraMetadataTest() {
        TEST_EXTENSION_FORKING_DESTRUCTOR;
    }

    int GetTypeFromTag(uint32_t tag) const {
        return get_camera_metadata_tag_type(tag);
    }

    int GetTypeFromStaticTag(uint32_t tag) const {
        const CameraMetadata& staticInfo = mDevice->info();
        camera_metadata_ro_entry entry = staticInfo.find(tag);
        return entry.type;
    }

protected:

};

TEST_F(CameraMetadataTest, types) {

    TEST_EXTENSION_FORKING_INIT;

    //FIXME: set this up in an external file of some sort (xml?)
    {
        char value[PROPERTY_VALUE_MAX];
        property_get("ro.build.id", value, "");
        std::string str_value(value);

        if (str_value == "manta")
        {
            EXPECT_EQ(TYPE_BYTE,
                GetTypeFromStaticTag(ANDROID_QUIRKS_TRIGGER_AF_WITH_AUTO));
            EXPECT_EQ(TYPE_BYTE,
                GetTypeFromStaticTag(ANDROID_QUIRKS_USE_ZSL_FORMAT));
            EXPECT_EQ(TYPE_BYTE,
                GetTypeFromStaticTag(ANDROID_QUIRKS_METERING_CROP_REGION));
        }
    }

    /*
    TODO:
    go through all static metadata and make sure all fields we expect
    that are there, ARE there.

    dont worry about the type as its enforced by the metadata api
    we can probably check the range validity though
    */

    if (0) {
        camera_metadata_ro_entry entry;
        EXPECT_EQ(TYPE_BYTE,     entry.type);
        EXPECT_EQ(TYPE_INT32,    entry.type);
        EXPECT_EQ(TYPE_FLOAT,    entry.type);
        EXPECT_EQ(TYPE_INT64,    entry.type);
        EXPECT_EQ(TYPE_DOUBLE,   entry.type);
        EXPECT_EQ(TYPE_RATIONAL, entry.type);
    }
}

}
}
}
