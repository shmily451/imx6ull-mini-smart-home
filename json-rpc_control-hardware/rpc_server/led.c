#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include "led.h"

void led_control(int on)
{
    int fd = open("/sys/class/leds/sys-led/brightness", O_RDWR);
    if (fd < 0) {
        perror("open led");
        return;
    }
    if (on)
        write(fd, "1\n", 2);
    else
        write(fd, "0\n", 2);
    close(fd);
}
