#include "dht11_sysfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/time.h>

static int gpio_export(int num)
{
    int fd = open("/sys/class/gpio/export", O_WRONLY);
    if(fd < 0)
        return -1;
    char buf[16];
    sprintf(buf, "%d", num);
    write(fd, buf, strlen(buf));
    close(fd);
    usleep(50000);
    return 0;
}

static int gpio_unexport(int num)
{
    int fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if(fd < 0)
        return -1;
    char buf[16];
    sprintf(buf, "%d", num);
    write(fd, buf, strlen(buf));
    close(fd);
    return 0;
}

static int gpio_set_dir(int num, int is_out)
{
    char path[128];
    sprintf(path, "/sys/class/gpio/gpio%d/direction", num);
    int fd = open(path, O_WRONLY);
    if(fd < 0)
        return -1;
    if(is_out)
        write(fd, "out", 3);
    else
        write(fd, "in", 2);
    close(fd);
    return 0;
}

static int gpio_write(int num, int val)
{
    char path[128];
    sprintf(path, "/sys/class/gpio/gpio%d/value", num);
    int fd = open(path, O_WRONLY);
    if(fd < 0)
        return -1;
    if(val)
        write(fd, "1", 1);
    else
        write(fd, "0", 1);
    close(fd);
    return 0;
}

static int gpio_read(int num)
{
    char path[128];
    sprintf(path, "/sys/class/gpio/gpio%d/value", num);
    int fd = open(path, O_RDONLY);
    if(fd < 0)
        return -1;
    char ch;
    read(fd, &ch, 1);
    close(fd);
    return (ch == '1') ? 1 : 0;
}

static uint64_t get_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

int dht11_read(char *hum, char *temp)
{
    if(gpio_export(DHT11_GPIO_NUM) < 0)
        return -1;

    // 1. 发送起始信号：拉低18ms
    gpio_set_dir(DHT11_GPIO_NUM, 1);
    gpio_write(DHT11_GPIO_NUM, 0);
    usleep(18000);
    gpio_write(DHT11_GPIO_NUM, 1);
    usleep(30);

    // 切换输入，等待DHT11应答
    gpio_set_dir(DHT11_GPIO_NUM, 0);

    int cnt = 0;
    while(gpio_read(DHT11_GPIO_NUM) == 1) {
        if(cnt++ > 20000) {gpio_unexport(DHT11_GPIO_NUM); return -1;}
    }
    cnt = 0;
    while(gpio_read(DHT11_GPIO_NUM) == 0) {
        if(cnt++ > 20000) {gpio_unexport(DHT11_GPIO_NUM); return -1;}
    }
    cnt = 0;
    while(gpio_read(DHT11_GPIO_NUM) == 1) {
        if(cnt++ > 20000) {gpio_unexport(DHT11_GPIO_NUM); return -1;}
    }

    unsigned char data[5] = {0};
    for(int byte=0;byte<5;byte++)
    {
        unsigned char val = 0;
        for(int bit=0;bit<8;bit++)
        {
            cnt = 0;
            while(gpio_read(DHT11_GPIO_NUM) == 0) {
                if(cnt++>20000) {gpio_unexport(DHT11_GPIO_NUM); return -1;}
            }
            uint64_t start = get_us();
            cnt = 0;
            while(gpio_read(DHT11_GPIO_NUM) == 1) {
                if(cnt++>20000) {gpio_unexport(DHT11_GPIO_NUM); return -1;}
            }
            uint64_t dur = get_us() - start;
            val <<= 1;
            if(dur > 55)
                val |= 1;
        }
        data[byte] = val;
    }
    gpio_unexport(DHT11_GPIO_NUM);

    // CRC校验
    if((data[0]+data[1]+data[2]+data[3]) != data[4])
        return -1;

    *hum = data[0];
    *temp = data[2];
    return 0;
}