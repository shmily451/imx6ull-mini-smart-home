#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <QDebug>

static int fd;

void dht11_init(void)
{
    fd = open("/dev/mydht11", O_RDWR | O_NONBLOCK);
    if (fd < 0)
    {
        qDebug()<<"open /dev/mydht11 failed";
    }
}

int dht11_read(char *humi, char *temp)
{
    char buf[2];

    if (2 == read(fd, buf, 2))
    {
        *humi = buf[0];
        *temp = buf[1];
        return 0;
    }
    else
    {
        return -1;
    }
}
