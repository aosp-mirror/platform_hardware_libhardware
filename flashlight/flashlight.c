#include <hardware/flashlight.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "qemu.h"

#define FLASHLIGHT "/sys/class/leds/spotlight/brightness"
#define CAMERA_FLASH "/sys/class/timed_output/flash/enable"

#ifdef QEMU_HARDWARE
int  qemu_get_flashlight_enabled()
{
    char  question[256];
    char  answer[256];
    int   len;

    len = qemu_command_format( question, sizeof question,
                               "get_flashlight_enabled" );

    len = qemu_control_query( question, len, answer, sizeof answer );
    if (len <= 0) return 0;

    /* we expect an answer of 0 or 1 */
    return (answer[0] == '1');
}

int qemu_set_flashlight_enabled(int  on)
{
    return qemu_control_command( "set_flashlight_enabled:%d", on );
}

int qemu_enable_camera_flash(int  milliseconds)
{
    return qemu_control_command( "enable_camera_flash:%d", milliseconds );
}
#endif

int get_flashlight_enabled()
{
    int fd;
    int ret = 0;
    char value;

    QEMU_FALLBACK(get_flashlight_enabled());

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

    QEMU_FALLBACK(set_flashlight_enabled(on));

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

    QEMU_FALLBACK(enable_camera_flash(milliseconds));

    fd = open(CAMERA_FLASH, O_RDWR);
    if(fd < 0)
        return errno;

    nwr = sprintf(value, "%d\n", milliseconds);
    ret = write(fd, value, nwr);

    close(fd);

    return (ret == nwr) ? 0 : -1;
}
