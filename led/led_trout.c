#include <hardware/led.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#define LOG_TAG "LED"
#include <utils/Log.h>

const char* const AMBER_BRIGHTNESS_FILE = "/sys/class/leds/amber/brightness";
const char* const RED_BRIGHTNESS_FILE = "/sys/class/leds/red/brightness";
const char* const GREEN_BRIGHTNESS_FILE = "/sys/class/leds/green/brightness";
const char* const BLUE_BRIGHTNESS_FILE = "/sys/class/leds/blue/brightness";
const char* const BLINK_ENABLE_FILE = "/sys/class/leds/red/device/blink";
const char* const BLINK_FREQ_FILE = "/sys/class/leds/red/device/grpfreq";
const char* const BLINK_PWM_FILE = "/sys/class/leds/red/device/grppwm";

static int
write_string(const char* file, const char* string, int len)
{
    int fd;
    ssize_t amt;

    fd = open(file, O_RDWR);
    if (fd < 0) {
        LOGE("open failed errno: %d\n", errno);
        return errno;
    }

    amt = write(fd, string, len);
    if (amt < 0) {
        LOGE("write failed errno: %d\n", errno);
    }

    close(fd);
    return amt >= 0 ? 0 : errno;
}

int set_led_state(unsigned colorARGB, int onMS, int offMS)
{
    int len;
    char buf[30];
    int alpha, red, green, blue;
    int blink, freq, pwm;
    
    LOGV("set_led_state colorARGB=%08X, onMS=%d, offMS=%d\n", colorARGB, onMS, offMS);
    
    // alpha of 0 or color of 0 means off
    if ((colorARGB & 0xff000000) == 0 || (colorARGB & 0x00ffffff) == 0) {
        onMS = 0;
        offMS = 0;
    }

    red = (colorARGB >> 16) & 0xFF;
    green = (colorARGB >> 8) & 0xFF;
    blue = colorARGB & 0xFF;

    len = sprintf(buf, "%d", red);
    write_string(RED_BRIGHTNESS_FILE, buf, len);
    len = sprintf(buf, "%d", green);
    write_string(GREEN_BRIGHTNESS_FILE, buf, len);
    len = sprintf(buf, "%d", blue);
    write_string(BLUE_BRIGHTNESS_FILE, buf, len);

    if (onMS > 0 && offMS > 0) {        
        int totalMS = onMS + offMS;
        
        // the LED appears to blink about once per second if freq is 20
        // 1000ms / 20 = 50
        freq = totalMS / 50;
        // pwm specifies the ratio of ON versus OFF
        // pwm = 0 -> always off
        // pwm = 255 => always on
        pwm = (onMS * 255) / totalMS;
        
        // the low 4 bits are ignored, so round up if necessary
        if (pwm > 0 && pwm < 16)
            pwm = 16;

        blink = 1;
    } else {
        blink = 0;
        freq = 0;
        pwm = 0;
    }
    
    if (blink) {
        len = sprintf(buf, "%d", freq);
        write_string(BLINK_FREQ_FILE, buf, len);
        len = sprintf(buf, "%d", pwm);
        write_string(BLINK_PWM_FILE, buf, len);
    }

    len = sprintf(buf, "%d", blink);
    write_string(BLINK_ENABLE_FILE, buf, len);

    return 0;
}
