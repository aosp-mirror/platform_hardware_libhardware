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

#ifndef _HARDWARE_LED_H
#define _HARDWARE_LED_H

#if __cplusplus
extern "C" {
#endif

/**
 * Changes the state of the LED.
 *
 *   -# Turn on LED: Alpha != 0 and RBG != 0, onMS == 0 && offMS == 0.
 *   -# Blink LED: Alpha != 0 and RBG != 0, onMS != 0 && offMS != 0.
 *   -# Turn off LED: Alpha == 0 or RGB == 0.
 *
 * @param colorARGB represents the LED's color: Alpha=31:24, Red=23:16
 * Green=15:8 Blue=7:0
 * @param onMS represents the time the LED is on in milliseconds
 * @param offMS represents the time the LED is off in milliseconds
 *
 * @return 0 if successful
 */
int set_led_state(unsigned colorARGB, int onMS, int offMS);

#if __cplusplus
}  // extern "C"
#endif

#endif  // _HARDWARE_LED_H
