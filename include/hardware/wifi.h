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
 * Load the Wi-Fi driver.
 *
 * @return 0 on success, < 0 on failure.
 */
int wifi_load_driver();

/**
 * Unload the Wi-Fi driver.
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
 * wifi_wait_for_event() performs a blocking call to 
 * get a Wi-Fi event and returns a string representing 
 * a Wi-Fi event when it occurs.
 *
 * @param buf is the buffer that receives the event
 * @param len is the maximum length of the buffer
 *
 * @returns number of bytes in buffer, 0 if no
 * event (for instance, no connection), and less than 0
 * if there is an error.
 */
int wifi_wait_for_event(char *buf, size_t len);

/**
 * wifi_command() issues a command to the Wi-Fi driver.
 *
 * Android extends the standard commands listed at 
 * /link http://hostap.epitest.fi/wpa_supplicant/devel/ctrl_iface_page.html 
 * to include support for sending commands to the driver:
 *
 * <table border="2" cellspacing="2" cellpadding="2">
 *   <tr>
 *     <td><strong>Command / Command summary</strong></td>
 *     <td><strong>Form of Response</strong></td>
 *     <td><strong>Processing</strong></td>
 *   </tr>
 *   <tr>
 *     <td>DRIVER START<BR>&nbsp;&nbsp;Turn on Wi-Fi Hardware</td>
 *     <td>OK if successful</td>
 *     <td>OK ? true : false</td>
 *   </tr>
 *   <tr>
 *     <td>DRIVER STOP<BR>&nbsp;&nbsp;Turn off Wi-Fi hardware</td>
 *     <td>OK if successful</td>
 *     <td>OK ? true : false</td>
 *   </tr>
 *   <tr>
 *     <td>DRIVER RSSI<BR>&nbsp;&nbsp;Return received signal strength indicator in -db for current AP</td>
 *     <td>&lt;ssid&gt; Rssi xx</td>
 *     <td>%*s %*s %d", &rssi</td>
 *   </tr>
 *   <tr>
 *     <td>DRIVER LINKSPEED<BR>&nbsp;&nbsp;Return link speed in MBPS</td>
 *     <td>LinkSpeed xx</td>
 *     <td>%*s %d", &linkspd</td>
 *   </tr>
 *   <tr>
 *     <td>DRIVER MACADDR<BR>&nbsp;&nbsp;Return mac address of the station</td>
 *     <td>Macaddr = xx.xx.xx.xx.xx.xx</td>
 *     <td>"%*s = %s", &macadr</td>
 *   </tr>
 *   <tr>
 *     <td>DRIVER SCAN-ACTIVE<BR>&nbsp;&nbsp;Set scan type to active</td>
 *     <td>"OK" if successful</td>
 *     <td>"OK" ? true : false</td>
 *   </tr>
 *   <tr>
 *     <td>DRIVER SCAN-PASSIVE<BR>&nbsp;&nbsp;Set scan type to passive</td>
 *     <td>"OK" if successful</td>
 *     <td>"OK" ? true : false</td>
 *   </tr>
 * </table>
 *
 * See libs/android_runtime/android_net_wifi_Wifi.cpp for more information
 * describing how these and other commands are invoked.
 *
 * @param command is the string command
 * @param reply is a buffer to receive a reply string
 * @param reply_len on entry, this is the maximum length of
 *        the reply buffer. On exit, the number of
 *        bytes in the reply buffer.
 *
 * @return 0 if successful, < 0 if an error.
 */
int wifi_command(const char *command, char *reply, size_t *reply_len);

/**
 * do_dhcp_request() issues a dhcp request and returns the acquired
 * information. 
 * 
 * All IPV4 addresses/mask are in network byte order.
 *
 * @param ipaddr return the assigned IPV4 address
 * @param gateway return the gateway being used
 * @param mask return the IPV4 mask
 * @param dns1 return the IPV4 address of a DNS server
 * @param dns2 return the IPV4 address of a DNS server
 * @param server return the IPV4 address of DHCP server
 * @param lease return the length of lease in seconds.
 *
 * @return 0 if successful, < 0 if error.
 */
int do_dhcp_request(int *ipaddr, int *gateway, int *mask,
                   int *dns1, int *dns2, int *server, int *lease);

/**
 * Return the error string of the last do_dhcp_request().
 */
const char *get_dhcp_error_string();

#if __cplusplus
};  // extern "C"
#endif

#endif  // _WIFI_H
