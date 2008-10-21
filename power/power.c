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

enum {
    ACQUIRE_PARTIAL_WAKE_LOCK = 0,
    ACQUIRE_FULL_WAKE_LOCK,
    RELEASE_WAKE_LOCK,
    REQUEST_STATE,
    OUR_FD_COUNT
};

const char * const PATHS[] = {
    "/sys/android_power/acquire_partial_wake_lock",
    "/sys/android_power/acquire_full_wake_lock",
    "/sys/android_power/release_wake_lock",
    "/sys/android_power/request_state"
};

const char * const AUTO_OFF_TIMEOUT_DEV = "/sys/android_power/auto_off_timeout";

const char * const LCD_BACKLIGHT = "/sys/class/leds/lcd-backlight/brightness";
const char * const BUTTON_BACKLIGHT = "/sys/class/leds/button-backlight/brightness";
const char * const KEYBOARD_BACKLIGHT = "/sys/class/leds/keyboard-backlight/brightness";

//XXX static pthread_once_t g_initialized = THREAD_ONCE_INIT;
static int g_initialized = 0;
static int g_fds[OUR_FD_COUNT];
static int g_error = 1;


static int64_t systemTime()
{
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec*1000000000LL + t.tv_nsec;
}

static void
open_file_descriptors(void)
{
    int i;
    for (i=0; i<OUR_FD_COUNT; i++) {
        int fd = open(PATHS[i], O_RDWR);
        if (fd < 0) {
            fprintf(stderr, "fatal error opening \"%s\"\n", PATHS[i]);
            g_error = errno;
            return;
        }
        g_fds[i] = fd;
    }

    g_error = 0;
}

static inline void
initialize_fds(void)
{
    // XXX: should be this:
    //pthread_once(&g_initialized, open_file_descriptors);
    // XXX: not this:
    if (g_initialized == 0) {
        open_file_descriptors();
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
    else if (lock == FULL_WAKE_LOCK) {
        fd = g_fds[ACQUIRE_FULL_WAKE_LOCK];
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

    // LOGI("set_a_light(%s, %d)\n", path, value);

    fd = open(path, O_RDWR);
    if (fd) {
        char    buffer[20];
        int bytes = sprintf(buffer, "%d\n", value);
        write(fd, buffer, bytes);
        close(fd);
    } else {
        LOGE("set_a_light failed to open %s\n", path);
    }
}

int
set_light_brightness(unsigned int mask, unsigned int brightness)
{
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
    //LOGI("*** set_screen_state %d", on);

    initialize_fds();

    //LOGI("go_to_sleep eventTime=%lld now=%lld g_error=%s\n", eventTime,
      //      systemTime(), strerror(g_error));

    if (g_error) return g_error;

    char buf[32];
    int len;
    if(on)
        len = sprintf(buf, "wake");
    else
        len = sprintf(buf, "standby");
    len = write(g_fds[REQUEST_STATE], buf, len);
    if(len < 0) {
        LOGE("Failed setting last user activity: g_error=%d\n", g_error);
    }
    return 0;
}


