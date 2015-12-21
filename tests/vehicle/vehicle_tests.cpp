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

#include <gtest/gtest.h>
#include "vehicle_test_fixtures.h"
#include "hardware/vehicle.h"

namespace tests {

// Check if list_properties command exists.
TEST_F(VehicleDevice, isThereListProperties) {
    ASSERT_TRUE(NULL != vehicle_device()->list_properties)
        << "list_properties() function is not implemented";
    std::cout << "Test succeeds.\n";
}

// HAL should provide atleast one property. The output of this command should be
// used to verify the vailidity of the function.
TEST_F(VehicleDevice, listPropertiesMoreThanOne) {
    vehicle_prop_config_t const* config;
    int num_configs = -1;
    config = vehicle_device()->list_properties(vehicle_device(), &num_configs);
    ASSERT_TRUE(num_configs > -1) << "list_properties() call failed.";
    ASSERT_TRUE(num_configs > 0) << "list_properties() returned zero items.";
    std::cout << "Number of properties reported: " << num_configs << "\n";
    for (int i = 0; i < num_configs; i++) {
        // Print each of the properties.
        const vehicle_prop_config_t& config_temp = config[i];
        std::cout << "Property ID: " << config_temp.prop << "\n";
        std::cout << "Property flags: " << config_temp.config_flags << "\n";
        std::cout << "Property change mode: " << config_temp.change_mode << "\n";
        std::cout << "Property min sample rate: " << config_temp.min_sample_rate << "\n";
        std::cout << "Property max sample rate: " << config_temp.max_sample_rate << "\n\n";
    }
}

// Test get() command.
// The fields are hardcoded in the dummy implementation and here.
TEST_F(VehicleDevice, getDriveState) {
    vehicle_prop_value_t data;
    data.prop = VEHICLE_PROPERTY_DRIVING_STATUS;
    // Set drive_state field to EINVAL so that we can check that its valid when
    // it comes back.
    data.value_type = -EINVAL;
    data.value.driving_status = -EINVAL;
    vehicle_device()->get(vehicle_device(), &data);

    // Check that retured values are not invalid.
    ASSERT_NE(data.value_type, -EINVAL) << "Drive state value type should be integer.";
    ASSERT_NE(data.value.driving_status, -EINVAL) << "Driving status should be positive.";

    std::cout << "Driving status value type: " << data.value_type << "\n"
              << "Driving status: " << data.value.driving_status << "\n";
}

// Test the workflows for subscribe and init/release.
// Subscribe will return error before init() is called or after release() is
// called.
TEST_F(VehicleDevice, initTest) {
    // Test that init on a new device works. When getting an instance, we are
    // already calling 'open' on the device.
    int ret_code =
        vehicle_device()->init(vehicle_device(), callback_fn(), error_fn());
    ASSERT_EQ(ret_code, 0) << "ret code: " << ret_code;

    // Trying to init again should return an error.
    ret_code = vehicle_device()->init(vehicle_device(), callback_fn(), error_fn());
    ASSERT_EQ(ret_code, -EEXIST) << "ret code: " << ret_code;

    // Uninit should always return 0.
    ret_code = vehicle_device()->release(vehicle_device());
    ASSERT_EQ(ret_code, 0) << "ret code: " << ret_code;

    // We should be able to init again.
    ret_code = vehicle_device()->init(vehicle_device(), callback_fn(), error_fn());
    ASSERT_EQ(ret_code, 0) << "ret code: " << ret_code;

    // Finally release.
    ret_code = vehicle_device()->release(vehicle_device());
    ASSERT_EQ(ret_code, 0) << "ret_code: " << ret_code;
}

// Test that subscribe works.
// We wait for 10 seconds while which the vehicle.c can post messages from
// within it's own thread.
TEST_F(VehicleDevice, subscribeTest) {
    // If the device is not init subscribe should fail off the bat.
    int ret_code = vehicle_device()->subscribe(vehicle_device(), VEHICLE_PROPERTY_DRIVING_STATUS,
            0, 0);
    ASSERT_EQ(ret_code, -EINVAL) << "Return code is: " << ret_code;

    // Let's init the device.
    ret_code = vehicle_device()->init(vehicle_device(), callback_fn(), error_fn());
    ASSERT_EQ(ret_code, 0) << "Return code is: " << ret_code;

    // Subscribe should now go through.
    ret_code = vehicle_device()->subscribe(vehicle_device(), VEHICLE_PROPERTY_DRIVING_STATUS, 0, 0);
    ASSERT_EQ(ret_code, 0) << "Return code is: " << ret_code;

    // We should start getting some messages thrown from the callback. Let's
    // wait for 20 seconds before unsubscribing.
    std::cout << "Sleeping for 20 seconds.";
    sleep(20);
    std::cout << "Waking from sleep.";

    // This property does not exist, so we should get -EINVAL.
    ret_code = vehicle_device()->unsubscribe(vehicle_device(), VEHICLE_PROPERTY_INFO_VIN);
    ASSERT_EQ(ret_code, -EINVAL) << "Return code is: " << ret_code;

    // This property exists, so we should get a success return code - also this
    // will be a blocking call.
    ret_code = vehicle_device()->unsubscribe(vehicle_device(), VEHICLE_PROPERTY_DRIVING_STATUS);
    ASSERT_EQ(ret_code, 0) << "Return code is: " << ret_code;
}

}  // namespace tests
