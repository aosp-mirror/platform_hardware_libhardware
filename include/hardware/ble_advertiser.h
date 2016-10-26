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

#include <base/callback_forward.h>
#include <stdint.h>
#include <vector>
#include "bt_common_types.h"
#include "bt_gatt_types.h"

using std::vector;

/** Callback invoked when multi-adv operation has completed */
using BleAdvertiserCb = base::Callback<void(uint8_t /* status */)>;

class BleAdvertiserInterface {
 public:
  virtual ~BleAdvertiserInterface() = default;

  /** Registers an advertiser with the stack */
  virtual void RegisterAdvertiser(
      base::Callback<void(uint8_t /* advertiser_id */, uint8_t /* status */)>) = 0;

  /*  This function disable a Multi-ADV instance */
  virtual void Unregister(uint8_t advertiser_id) = 0;

  /** Set the advertising data or scan response data */
  virtual void SetData(bool set_scan_rsp, vector<uint8_t> data) = 0;

  /* Set the parameters as per spec, user manual specified values */
  virtual void MultiAdvSetParameters(int advertiser_id, int min_interval,
                                     int max_interval, int adv_type,
                                     int chnl_map, int tx_power,
                                     BleAdvertiserCb cb) = 0;

  /* Setup the data for the specified instance */
  virtual void MultiAdvSetInstData(int advertiser_id, bool set_scan_rsp,
                                   vector<uint8_t> data,
                                   BleAdvertiserCb cb) = 0;

  /* Enable the advertising instance as per spec */
  virtual void MultiAdvEnable(uint8_t advertiser_id, bool enable,
                              BleAdvertiserCb cb, int timeout_s,
                              BleAdvertiserCb timeout_cb) = 0;
};

#endif /* ANDROID_INCLUDE_BLE_ADVERTISER_H */
