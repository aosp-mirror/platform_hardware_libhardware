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

#ifndef __ANDROID_HAL_VEHICLE_TEST_
#define __ANDROID_HAL_VEHICLE_TEST_

#include <gtest/gtest.h>
#include <hardware/hardware.h>
#include <hardware/vehicle.h>

namespace tests {

static const uint64_t kVersion = HARDWARE_DEVICE_API_VERSION_2(1, 0, 1);

class VehicleModule : public testing::Test {
public:
    VehicleModule() :
        vehicle_module_(NULL) {}
    ~VehicleModule() {}
protected:
    virtual void SetUp() {
        const hw_module_t *hw_module = NULL;
        ASSERT_EQ(0, hw_get_module(VEHICLE_HARDWARE_MODULE_ID, &hw_module))
                    << "Can't get vehicle module";
        ASSERT_TRUE(NULL != hw_module)
                    << "hw_get_module didn't return a valid hardware module";

        vehicle_module_ = reinterpret_cast<const vehicle_module_t*>(hw_module);
    }
    const vehicle_module_t* vehicle_module() { return vehicle_module_; }
private:
    const vehicle_module_t* vehicle_module_;
};


int VehicleEventCallback(const vehicle_prop_value_t* event_data) {
    // Print what we got.
    std::cout << "got some value from callback: "
              << event_data->prop
              << " uint32 value: "
              << event_data->value.int32_value << "\n";
    return 0;
}

 int VehicleErrorCallback(int32_t /*error_code*/, int32_t /*property*/, int32_t /*operation*/) {
    // Do nothing.
    return 0;
}

class VehicleDevice : public VehicleModule {
public:
    VehicleDevice() :
        vehicle_device_(NULL) {}
    ~VehicleDevice() {}
protected:
    virtual void SetUp() {
        VehicleModule::SetUp();
        hw_device_t *device = NULL;
        ASSERT_TRUE(NULL != vehicle_module()->common.methods->open)
                    << "Vehicle open() is unimplemented";
        ASSERT_EQ(0, vehicle_module()->common.methods->open(
            (const hw_module_t*)vehicle_module(), NULL, &device))
                << "Can't open vehicle device";
        ASSERT_TRUE(NULL != device)
                    << "Vehicle open() returned a NULL device";
        ASSERT_EQ(kVersion, device->version)
                    << "Unsupported version";
        vehicle_device_ = reinterpret_cast<vehicle_hw_device_t*>(device);
    }
    vehicle_hw_device_t* vehicle_device() { return vehicle_device_; }
    vehicle_event_callback_fn callback_fn() {
      return VehicleEventCallback;
    }
    vehicle_error_callback_fn error_fn() {
      return VehicleErrorCallback;
    }

 private:
    vehicle_hw_device_t* vehicle_device_;
};

}  // namespace tests

#endif  // __ANDROID_HAL_VEHICLE_TEST_
