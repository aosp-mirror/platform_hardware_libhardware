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
#include <hardware/power.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>

#define LOG_TAG "power"
#include <utils/Log.h>

#include "qemu.h"
#ifdef QEMU_POWER
#include "power_qemu.h"
#endif

enum {
    ACQUIRE_PARTIAL_WAKE_LOCK = 0,
    RELEASE_WAKE_LOCK,
    REQUEST_STATE,
    OUR_FD_COUNT
};

const char * const OLD_PATHS[] = {
    "/sys/android_power/acquire_partial_wake_lock",
    "/sys/android_power/release_wake_lock",
    "/sys/android_power/request_state"
};

const char * const NEW_PATHS[] = {
    "/sys/power/wake_lock",
    "/sys/power/wake_unlock",
    "/sys/power/state"
};

const char * const AUTO_OFF_TIMEOUT_DEV = "/sys/android_power/auto_off_timeout";

const char * const LCD_BACKLIGHT = "/sys/class/leds/lcd-backlight/brightness";
const char * const BUTTON_BACKLIGHT = "/sys/class/leds/button-backlight/brightness";
const char * const KEYBOARD_BACKLIGHT = "/sys/class/leds/keyboard-backlight/brightness";

//XXX static pthread_once_t g_initialized = THREAD_ONCE_INIT;
static int g_initialized = 0;
static int g_fds[OUR_FD_COUNT];
static int g_error = 1;

static const char *off_state = "mem";
static const char *on_state = "on";

static int64_t systemTime()
{
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec*1000000000LL + t.tv_nsec;
}

static int
open_file_descriptors(const char * const paths[])
{
    int i;
    for (i=0; i<OUR_FD_COUNT; i++) {
        int fd = open(paths[i], O_RDWR);
        if (fd < 0) {
            fprintf(stderr, "fatal error opening \"%s\"\n", paths[i]);
            g_error = errno;
            return -1;
        }
        g_fds[i] = fd;
    }

    g_error = 0;
    return 0;
}

static inline void
initialize_fds(void)
{
    // XXX: should be this:
    //pthread_once(&g_initialized, open_file_descriptors);
    // XXX: not this:
    if (g_initialized == 0) {
        if(open_file_descriptors(NEW_PATHS) < 0) {
            open_file_descriptors(OLD_PATHS);
            on_state = "wake";
            off_state = "standby";
        }
        g_initialized = 1;
    }
}

int
acquire_wake_lock(int lock, const char* id)
{
    initialize_fds();

//    LOGI("acquire_wake_lock lock=%d id='%s'\n", lock, id);

    if (g_error) return g_error;

    int fd;

    if (lock == PARTIAL_WAKE_LOCK) {
        fd = g_fds[ACQUIRE_PARTIAL_WAKE_LOCK];
    }
    else {
        return EINVAL;
    }

    return write(fd, id, strlen(id));
}

int
release_wake_lock(const char* id)
{
    initialize_fds();

//    LOGI("release_wake_lock id='%s'\n", id);

    if (g_error) return g_error;

    ssize_t len = write(g_fds[RELEASE_WAKE_LOCK], id, strlen(id));
    return len >= 0;
}

int
set_last_user_activity_timeout(int64_t delay)
{
//    LOGI("set_last_user_activity_timeout delay=%d\n", ((int)(delay)));

    int fd = open(AUTO_OFF_TIMEOUT_DEV, O_RDWR);
    if (fd >= 0) {
        char buf[32];
        ssize_t len;
        len = sprintf(buf, "%d", ((int)(delay)));
        len = write(fd, buf, len);
        close(fd);
        return 0;
    } else {
        return errno;
    }
}

static void
set_a_light(const char* path, int value)
{
    int fd;
    static int already_warned = 0;

    // LOGI("set_a_light(%s, %d)\n", path, value);

    fd = open(path, O_RDWR);
    if (fd >= 0) {
        char    buffer[20];
        int bytes = sprintf(buffer, "%d\n", value);
        write(fd, buffer, bytes);
        close(fd);
    } else {
        if (already_warned == 0) {
            LOGE("set_a_light failed to open %s\n", path);
            already_warned = 1;
        }
    }
}

int
set_light_brightness(unsigned int mask, unsigned int brightness)
{
    QEMU_FALLBACK(set_light_brightness(mask,brightness));

    initialize_fds();

//    LOGI("set_light_brightness mask=0x%08x brightness=%d now=%lld g_error=%s\n",
//            mask, brightness, systemTime(), strerror(g_error));

    if (mask & KEYBOARD_LIGHT) {
        set_a_light(KEYBOARD_BACKLIGHT, brightness);
    }

    if (mask & SCREEN_LIGHT) {
        set_a_light(LCD_BACKLIGHT, brightness);
    }

    if (mask & BUTTON_LIGHT) {
        set_a_light(BUTTON_BACKLIGHT, brightness);
    }

    return 0;
}


int
set_screen_state(int on)
{
    QEMU_FALLBACK(set_screen_state(on));

    //LOGI("*** set_screen_state %d", on);

    initialize_fds();

    //LOGI("go_to_sleep eventTime=%lld now=%lld g_error=%s\n", eventTime,
      //      systemTime(), strerror(g_error));

    if (g_error) return g_error;

    char buf[32];
    int len;
    if(on)
        len = sprintf(buf, on_state);
    else
        len = sprintf(buf, off_state);
    len = write(g_fds[REQUEST_STATE], buf, len);
    if(len < 0) {
        LOGE("Failed setting last user activity: g_error=%d\n", g_error);
    }
    return 0;
}
