#include <fcntl.h>
#include <unistd.h>
#include <QDebug>
#include <errno.h>

void led_control(int on)
{
    int fd = open("/sys/class/leds/sys-led/brightness", O_RDWR);
    if(fd < 0)
    {
        qDebug() << "open sys-led brightness error! errno:";
        return;
    }
    if(on)
        write(fd, "1\n", 2);
    else
        write(fd, "0\n", 2);
    close(fd);
}
