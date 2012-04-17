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

#include <system/camera_metadata.h>
#include <hardware/camera2.h>
#include <gtest/gtest.h>
#include <iostream>

class Camera2Test: public testing::Test {
  public:
    static void SetUpTestCase() {
        int res;

        hw_module_t *module = NULL;
        res = hw_get_module(CAMERA_HARDWARE_MODULE_ID,
                (const hw_module_t **)&module);

        ASSERT_EQ(0, res)
                << "Failure opening camera hardware module: " << res;
        ASSERT_TRUE(NULL != module)
                << "No camera module was set by hw_get_module";

        std::cout << "  Camera module name: " << module->name << std::endl;
        std::cout << "  Camera module author: " << module->author << std::endl;
        std::cout << "  Camera module API version: 0x" << std::hex
                  << module->module_api_version << std::endl;
        std::cout << "  Camera module HAL API version: 0x" << std::hex
                  << module->hal_api_version << std::endl;

        int16_t version2_0 = CAMERA_MODULE_API_VERSION_2_0;
        ASSERT_EQ(version2_0, module->module_api_version)
                << "Camera module version is 0x"
                << std::hex << module->module_api_version
                << ", not 2.0. (0x"
                << std::hex << CAMERA_MODULE_API_VERSION_2_0 << ")";

        sCameraModule = reinterpret_cast<camera_module_t*>(module);

        sNumCameras = sCameraModule->get_number_of_cameras();
        ASSERT_LT(0, sNumCameras) << "No camera devices available!";

        std::cout << "  Camera device count: " << sNumCameras << std::endl;
        sCameraSupportsHal2 = new bool[sNumCameras];

        for (int i = 0; i < sNumCameras; i++) {
            camera_info info;
            res = sCameraModule->get_camera_info(i, &info);
            ASSERT_EQ(0, res)
                    << "Failure getting camera info for camera " << i;
            std::cout << "  Camera device: " << std::dec
                      << i << std::endl;;
            std::cout << "    Facing: " << std::dec
                      << info.facing  << std::endl;
            std::cout << "    Orientation: " << std::dec
                      << info.orientation  << std::endl;
            std::cout << "    Version: 0x" << std::hex <<
                    info.device_version  << std::endl;
            if (info.device_version >= CAMERA_DEVICE_API_VERSION_2_0) {
                sCameraSupportsHal2[i] = true;
                ASSERT_TRUE(NULL != info.static_camera_characteristics);
                std::cout << "    Static camera metadata:"  << std::endl;
                dump_camera_metadata(info.static_camera_characteristics, 0, 1);
            } else {
                sCameraSupportsHal2[i] = false;
            }
        }
    }

    static const camera_module_t *getCameraModule() {
        return sCameraModule;
    }

    static const camera2_device_t *openCameraDevice(int id) {
        if (NULL == sCameraSupportsHal2) return NULL;
        if (id >= sNumCameras) return NULL;
        if (!sCameraSupportsHal2[id]) return NULL;

        hw_device_t *device = NULL;
        const camera_module_t *cam_module = getCameraModule();
        char camId[10];
        int res;

        snprintf(camId, 10, "%d", id);
        res = cam_module->common.methods->open(
            (const hw_module_t*)cam_module,
            camId,
            &device);
        if (res < 0 || cam_module == NULL) {
            return NULL;
        }
        camera2_device_t *cam_device =
                reinterpret_cast<camera2_device_t*>(device);
        return cam_device;
    }

  private:

    static camera_module_t *sCameraModule;
    static int sNumCameras;
    static bool *sCameraSupportsHal2;
};

camera_module_t *Camera2Test::sCameraModule = NULL;
int Camera2Test::sNumCameras = 0;
bool *Camera2Test::sCameraSupportsHal2 = NULL;


TEST_F(Camera2Test, Basic) {
    ASSERT_TRUE(NULL != getCameraModule());
}
