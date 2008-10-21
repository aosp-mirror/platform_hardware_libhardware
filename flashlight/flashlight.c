#include <hardware/flashlight.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define FLASHLIGHT "/sys/class/leds/spotlight/brightness"
#define CAMERA_FLASH "/sys/class/timed_output/flash/enable"


int get_flashlight_enabled()
{
    int fd;
    int ret = 0;
    char value;

    fd = open(FLASHLIGHT, O_RDONLY);
    if(fd && read(fd, &value, 1) == 1) {
        ret = (value == '1');
    }
    close(fd);

    return ret;
}

int set_flashlight_enabled(int on)
{
    int nwr, ret, fd;
    char value[20];

    fd = open(FLASHLIGHT, O_RDWR);
    if(fd < 0)
        return errno;

    nwr = sprintf(value, "%d\n", !!on);
    ret = write(fd, value, nwr);

    close(fd);

    return (ret == nwr) ? 0 : -1;
}

int enable_camera_flash(int milliseconds)
{
    int nwr, ret, fd;
    char value[20];

    fd = open(CAMERA_FLASH, O_RDWR);
    if(fd < 0)
        return errno;

    nwr = sprintf(value, "%d\n", milliseconds);
    ret = write(fd, value, nwr);

    close(fd);

    return (ret == nwr) ? 0 : -1;
}
