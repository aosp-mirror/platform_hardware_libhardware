/*
 * Copyright 2008, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <hardware/sensors.h>
#include <unistd.h>


uint32_t sensors_control_init() {
    return 0;
}

/* returns a fd where to read the events from. the caller takes
 * ownership of this fd */
int sensors_control_open() {
    return -1;
}

/* returns a bitmask indicating which sensors are enabled */
uint32_t sensors_control_activate(uint32_t sensors, uint32_t mask) {
    return 0;
}

/* set the delay between sensor events in ms */
int sensors_control_delay(int32_t ms) {
    return -1;
}

/* initialize the sensor data read. doesn't take ownership of the fd */
int sensors_data_open(int fd) {
    return 0;
}

/* done with sensor data */
int sensors_data_close() {
    return -1;
}

/* returns a bitmask indicating which sensors have changed */
int sensors_data_poll(sensors_data_t* data, uint32_t sensors_of_interest) {
    usleep(60000000);
    return 0;
}

/* returns available sensors */
uint32_t sensors_data_get_sensors() {
    return 0;
}
