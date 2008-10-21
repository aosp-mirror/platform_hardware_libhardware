/*
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef _WIFI_H
#define _WIFI_H

#if __cplusplus
extern "C" {
#endif

/**
 * Load the wifi driver.
 *
 * @return 0 on success, < 0 on failure.
 */
int wifi_load_driver();

/**
 * Unload the wifi driver.
 *
 * @return 0 on success, < 0 on failure.
 */
int wifi_unload_driver();

/**
 * Start supplicant.
 *
 * @return 0 on success, < 0 on failure.
 */
int wifi_start_supplicant();

/**
 * Stop supplicant.
 *
 * @return 0 on success, < 0 on failure.
 */
int wifi_stop_supplicant();

/**
 * Open a connection to supplicant.
 *
 * @return 0 on success, < 0 on failure.
 */
int wifi_connect_to_supplicant();

/**
 * Close connection supplicant.
 *
 * @return 0 on success, < 0 on failure.
 */
void wifi_close_supplicant_connection();

/**
 * Do a blocking call to get a wifi event and
 * return a string representing a wifi event
 * when it occurs.
 *
 * @param buf is the buffer that receives the event
 * @param len is the maximum length of the buffer
 *
 * @returns number of bytes in buffer, 0 if no
 * event, for instance no connection, < 0 if an
 * error.
 */
int wifi_wait_for_event(char *buf, size_t len);

/**
 * Issue a command to the wifi driver.
 *
 * see \link http://hostap.epitest.fi/wpa_supplicant/devel/ctrl_iface_page.html
 * for the list of the standard commands. Android has extended these to include
 * support for sending commands to the driver:
 *
 *------------------------------------------------------------------------------
 *   command                 form of response                processing
 *      Summary of the command
 *------------------------------------------------------------------------------
 * "DRIVER START"        -> "OK" if successful            -> "OK" ? true : false
 *      Turn on WiFi Hardware
 *
 * "DRIVER STOP"         -> "OK" if successful            -> "OK" ? true : false
 *      Turn off WiFi Hardware
 *
 * "DRIVER RSSI"         -> "<ssid> Rssi xx"              -> "%*s %*s %d", &rssi
 *      Return received signal strength indicator in -db for current AP
 *
 * "DRIVER LINKSPEED"    -> "LinkSpeed xx"                -> "%*s %d", &linkspd
 *      Return link speed in MBPS
 *
 * "DRIVER MACADDR"      -> "Macaddr = xx.xx.xx.xx.xx.xx" -> "%*s = %s", &macadr
 *      Return mac address of the station
 *
 * "DRIVER SCAN-ACTIVE"  -> "OK" if successful            -> "OK" ? true : false
 *      Set scan type to active
 *
 * "DRIVER SCAN-PASSIVE" -> "OK" if successful            -> "OK" ? true : false
 *      Set scan type to passive
 *------------------------------------------------------------------------------
 *
 * See libs/android_runtime/android_net_wifi_Wifi.cpp for more information
 * on how these and other commands invoked.
 *
 * @param command is the string command
 * @param reply is a buffer to receive a reply string
 * @param reply_len on entry is the maximum length of
 *        reply buffer and on exit the number of
 *        bytes in the reply buffer.
 *
 * @return 0 if successful, < 0 if an error.
 */
int wifi_command(const char *command, char *reply, size_t *reply_len);

/**
 * Issues a dhcp request returning the acquired
 * information. All IPV4 addresses/mask are in
 * network byte order.
 *
 * @param ipaddr return the assigned IPV4 address
 * @param gateway return the gateway being used
 * @param mask return the IPV4 mask
 * @param dns1 return the IPV4 address of a dns server
 * @param dns2 return the IPV4 address of a dns server
 * @param serverAddress return the IPV4 address of dhcp server
 * @param lease return the length of lease in seconds.
 *
 * @return 0 if successful, < 0 if error.
 */
int do_dhcp_request(int *ipaddr, int *gateway, int *mask,
                   int *dns1, int *dns2, int *server, int *lease);

/**
 * Return the error string of the last do_dhcp_request.
 */
const char *get_dhcp_error_string();

#if __cplusplus
};  // extern "C"
#endif

#endif  // _WIFI_H
