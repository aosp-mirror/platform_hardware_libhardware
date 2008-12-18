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
#include <hardware/led.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

const char* const CADENCE_FILE = "/sys/class/leds/left/cadence";
const char* const COLOR_FILE = "/sys/class/leds/left/color";
const char* const BT_WIFI_FILE = "/sys/class/leds/right/brightness";

static int
write_string(const char* file, const char* string, int len)
{
    int fd;
    ssize_t amt;

    fd = open(file, O_RDWR);
    if (fd < 0) {
        return errno;
    }

    amt = write(fd, string, len+1);

    close(fd);
    return amt >= 0 ? 0 : errno;
}

int sardine_set_led_state(unsigned colorARGB, int onMS, int offMS)
{
    int len;
    char buf[30];
    int red, green, color;

    // alpha of 0 or color of 0 (ignoring blue) means off
    if ((colorARGB & 0xff000000) == 0 || (colorARGB & 0x00ffff00) == 0) {
        onMS = 0;
        offMS = 0;
    }

    // blue means green
    if ((colorARGB & 0x00ffff00) == 0 && (colorARGB & 0x000000ff) != 0) {
        colorARGB |= (colorARGB & 0x000000ff) << 8;
    }

    // ===== color =====
    // this color conversion isn't really correct, but it's better to
    // have the light come bright on when asked for low intensity than to
    // not come on at all. we have no blue
    red = (colorARGB & 0x00ff0000) ? 1 : 0;
    green = (colorARGB & 0x0000ff00) ? 2 : 0;
    color = (red | green) - 1;

    len = sprintf(buf, "%d", color);
    write_string(COLOR_FILE, buf, len);

    // ===== color =====
    len = sprintf(buf, "%d,%d", onMS, offMS);
    write_string(CADENCE_FILE, buf, len);

    return 0;
}
