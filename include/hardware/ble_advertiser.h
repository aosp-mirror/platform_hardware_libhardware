/*
 * Copyright (C) 2016 The Android Open Source Project
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


#ifndef ANDROID_INCLUDE_BLE_ADVERTISER_H
#define ANDROID_INCLUDE_BLE_ADVERTISER_H

#include <stdint.h>
#include <vector>
#include "bt_gatt_types.h"
#include "bt_common_types.h"

using std::vector;

__BEGIN_DECLS

/** Callback invoked in response to register_advertiser */
typedef void (*register_advertiser_callback)(int status, int advertiser_id,
                                             bt_uuid_t *uuid);

/** Callback invoked when multi-adv enable operation has completed */
typedef void (*multi_adv_enable_callback)(int advertiser_id, int status);

/** Callback invoked when multi-adv param update operation has completed */
typedef void (*multi_adv_update_callback)(int advertiser_id, int status);

/** Callback invoked when multi-adv instance data set operation has completed */
typedef void (*multi_adv_data_callback)(int advertiser_id, int status);

/** Callback invoked when multi-adv disable operation has completed */
typedef void (*multi_adv_disable_callback)(int advertiser_id, int status);

typedef struct {
    register_advertiser_callback        register_advertiser_cb;
    multi_adv_enable_callback           multi_adv_enable_cb;
    multi_adv_update_callback           multi_adv_update_cb;
    multi_adv_data_callback             multi_adv_data_cb;
    multi_adv_disable_callback          multi_adv_disable_cb;
} ble_advertiser_callbacks_t;

typedef struct {
    /** Registers an advertiser with the stack */
    bt_status_t (*register_advertiser)(bt_uuid_t *uuid);

    /** Unregister a advertiser from the stack */
    bt_status_t (*unregister_advertiser)(int advertiser_id);

    /** Set the advertising data or scan response data */
    bt_status_t (*set_adv_data)(int advertiser_id, bool set_scan_rsp, bool include_name,
                    bool include_txpower, int min_interval, int max_interval, int appearance,
                    vector<uint8_t> manufacturer_data,
                    vector<uint8_t> service_data,
                    vector<uint8_t> service_uuid);

    /* Set up the parameters as per spec, user manual specified values and enable multi ADV */
    bt_status_t (*multi_adv_enable)(int advertiser_id, int min_interval,int max_interval,int adv_type,
                 int chnl_map, int tx_power, int timeout_s);

    /* Update the parameters as per spec, user manual specified values and restart multi ADV */
    bt_status_t (*multi_adv_update)(int advertiser_id, int min_interval,int max_interval,int adv_type,
                 int chnl_map, int tx_power, int timeout_s);

    /* Setup the data for the specified instance */
    bt_status_t (*multi_adv_set_inst_data)(int advertiser_id, bool set_scan_rsp, bool include_name,
                    bool incl_txpower, int appearance, vector<uint8_t> manufacturer_data,
                    vector<uint8_t> service_data, vector<uint8_t> service_uuid);

    /* Disable the multi adv instance */
    bt_status_t (*multi_adv_disable)(int advertiser_id);

} ble_advertiser_interface_t;

__END_DECLS

#endif /* ANDROID_INCLUDE_BLE_ADVERTISER_H */
