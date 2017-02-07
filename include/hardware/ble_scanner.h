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

#ifndef ANDROID_INCLUDE_BLE_SCANNER_H
#define ANDROID_INCLUDE_BLE_SCANNER_H

#include <stdint.h>
#include <vector>
#include "bt_common_types.h"
#include "bt_gatt_client.h"
#include "bt_gatt_types.h"

/** Callback invoked when batchscan storage config operation has completed */
typedef void (*batchscan_cfg_storage_callback)(int client_if, int status);

/** Callback invoked when batchscan enable / disable operation has completed */
typedef void (*batchscan_enable_disable_callback)(int action, int client_if,
                                                  int status);

/** Callback invoked when batchscan reports are obtained */
typedef void (*batchscan_reports_callback)(int client_if, int status,
                                           int report_format, int num_records,
                                           std::vector<uint8_t> data);

/** Callback invoked when batchscan storage threshold limit is crossed */
typedef void (*batchscan_threshold_callback)(int client_if);

/** Track ADV VSE callback invoked when tracked device is found or lost */
typedef void (*track_adv_event_callback)(
    btgatt_track_adv_info_t *p_track_adv_info);

/** Callback invoked when scan parameter setup has completed */
typedef void (*scan_parameter_setup_completed_callback)(int client_if,
                                                        btgattc_error_t status);

/** Callback for scan results */
typedef void (*scan_result_callback)(uint16_t event_type, uint8_t addr_type,
                                     bt_bdaddr_t *bda, uint8_t primary_phy,
                                     uint8_t secondary_phy,
                                     uint8_t advertising_sid, int8_t tx_power,
                                     int8_t rssi, uint16_t periodic_adv_int,
                                     std::vector<uint8_t> adv_data);

/** Callback invoked when a scan filter configuration command has completed */
typedef void (*scan_filter_cfg_callback)(int action, int client_if, int status,
                                         int filt_type, int avbl_space);

/** Callback invoked when scan param has been added, cleared, or deleted */
typedef void (*scan_filter_param_callback)(int action, int client_if,
                                           int status, int avbl_space);

/** Callback invoked when a scan filter configuration command has completed */
typedef void (*scan_filter_status_callback)(int enable, int client_if,
                                            int status);

typedef struct {
  scan_result_callback scan_result_cb;
  batchscan_cfg_storage_callback batchscan_cfg_storage_cb;
  batchscan_enable_disable_callback batchscan_enb_disable_cb;
  batchscan_reports_callback batchscan_reports_cb;
  batchscan_threshold_callback batchscan_threshold_cb;
  track_adv_event_callback track_adv_event_cb;
  scan_parameter_setup_completed_callback scan_parameter_setup_completed_cb;
  scan_filter_cfg_callback scan_filter_cfg_cb;
  scan_filter_param_callback scan_filter_param_cb;
  scan_filter_status_callback scan_filter_status_cb;
} btgatt_scanner_callbacks_t;

class BleScannerInterface {
 public:
  virtual ~BleScannerInterface() = default;

  using RegisterCallback =
      base::Callback<void(uint8_t /* scanner_id */, uint8_t /* status */)>;

  /** Registers a scanner with the stack */
  virtual void RegisterScanner(RegisterCallback) = 0;

  /** Unregister a scanner from the stack */
  virtual void Unregister(int scanner_id) = 0;

  /** Start or stop LE device scanning */
  virtual void Scan(bool start) = 0;

  /** Setup scan filter params */
  virtual void ScanFilterParamSetup(
      uint8_t client_if, uint8_t action, uint8_t filt_index,
      std::unique_ptr<btgatt_filt_param_setup_t> filt_param) = 0;

  /** Configure a scan filter condition  */
  virtual void ScanFilterAddRemove(int client_if, int action, int filt_type,
                                   int filt_index, int company_id,
                                   int company_id_mask, const bt_uuid_t *p_uuid,
                                   const bt_uuid_t *p_uuid_mask,
                                   const bt_bdaddr_t *bd_addr, char addr_type,
                                   std::vector<uint8_t> data,
                                   std::vector<uint8_t> p_mask) = 0;

  /** Clear all scan filter conditions for specific filter index*/
  virtual void ScanFilterClear(int client_if, int filt_index) = 0;

  /** Enable / disable scan filter feature*/
  virtual void ScanFilterEnable(int client_if, bool enable) = 0;

  /** Sets the LE scan interval and window in units of N*0.625 msec */
  virtual void SetScanParameters(int client_if, int scan_interval,
                                 int scan_window) = 0;

  /* Configure the batchscan storage */
  virtual void BatchscanConfigStorage(int client_if, int batch_scan_full_max,
                                      int batch_scan_trunc_max,
                                      int batch_scan_notify_threshold) = 0;

  /* Enable batchscan */
  virtual void BatchscanEnable(int client_if, int scan_mode, int scan_interval,
                               int scan_window, int addr_type,
                               int discard_rule) = 0;

  /* Disable batchscan */
  virtual void BatchscanDisable(int client_if) = 0;

  /* Read out batchscan reports */
  virtual void BatchscanReadReports(int client_if, int scan_mode) = 0;
};

#endif /* ANDROID_INCLUDE_BLE_SCANNER_H */