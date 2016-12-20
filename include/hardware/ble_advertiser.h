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

struct AdvertiseParameters {
  uint16_t advertising_event_properties;
  uint32_t min_interval;
  uint32_t max_interval;
  uint8_t channel_map;
  int8_t tx_power;
  uint8_t primary_advertising_phy;
  uint8_t secondary_advertising_phy;
  uint8_t scan_request_notification_enable;
};

class BleAdvertiserInterface {
 public:
  virtual ~BleAdvertiserInterface() = default;

  /** Callback invoked when multi-adv operation has completed */
  using Callback = base::Callback<void(uint8_t /* status */)>;

  /** Registers an advertiser with the stack */
  virtual void RegisterAdvertiser(
      base::Callback<void(uint8_t /* advertiser_id */,
                          uint8_t /* status */)>) = 0;

  /* Set the parameters as per spec, user manual specified values */
  virtual void SetParameters(
      uint8_t advertiser_id, uint16_t advertising_event_properties,
      uint32_t min_interval, uint32_t max_interval, int chnl_map, int tx_power,
      uint8_t primary_advertising_phy, uint8_t secondary_advertising_phy,
      uint8_t scan_request_notification_enable, Callback cb) = 0;

  /* Setup the data */
  virtual void SetData(int advertiser_id, bool set_scan_rsp,
                       std::vector<uint8_t> data, Callback cb) = 0;

  /* Enable the advertising instance */
  virtual void Enable(uint8_t advertiser_id, bool enable, Callback cb,
                      int timeout_s, Callback timeout_cb) = 0;

  /*  Unregisters an advertiser */
  virtual void Unregister(uint8_t advertiser_id) = 0;

  virtual void StartAdvertising(uint8_t advertiser_id, Callback cb,
                                AdvertiseParameters params,
                                std::vector<uint8_t> advertise_data,
                                std::vector<uint8_t> scan_response_data,
                                int timeout_s, Callback timeout_cb) = 0;
};

#endif /* ANDROID_INCLUDE_BLE_ADVERTISER_H */
