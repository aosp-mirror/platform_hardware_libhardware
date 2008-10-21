#include <hardware/vibrator.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define THE_DEVICE "/sys/class/timed_output/vibrator/enable"

static int sendit(int timeout_ms)
{
    int nwr, ret, fd;
    char value[20];

    fd = open(THE_DEVICE, O_RDWR);
    if(fd < 0)
        return errno;

    nwr = sprintf(value, "%d\n", timeout_ms);
    ret = write(fd, value, nwr);

    close(fd);

    return (ret == nwr) ? 0 : -1;
}

int vibrator_on()
{
    /* constant on, up to maximum allowed time */
    return sendit(-1);
}

int vibrator_off()
{
    return sendit(0);
}
