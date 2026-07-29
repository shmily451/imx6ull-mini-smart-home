#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

static int fd = -1;
static char g_humi = 0, g_temp = 0;
static pthread_mutex_t dht11_mutex = PTHREAD_MUTEX_INITIALIZER;
static int thread_running = 1;  // 控制线程退出的标志

void *dht11_thread(void *arg)
{
    char buf[2];
    while (thread_running) {
        if (fd >= 0 && read(fd, buf, 2) == 2) {
            pthread_mutex_lock(&dht11_mutex);
            g_humi = buf[0];
            g_temp = buf[1];
            pthread_mutex_unlock(&dht11_mutex);
        } else {
            // 读取失败，可能是设备未就绪，短暂休眠后重试
            usleep(100000);  // 100ms
        }
        sleep(1);  // 每秒读一次
    }
    return NULL;
}

void dht11_init(void)
{
    fd = open("/dev/mydht11", O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        perror("open /dev/mydht11");
        return;
    }
    pthread_t tid;
    if (pthread_create(&tid, NULL, dht11_thread, NULL) != 0) {
        perror("pthread_create");
        close(fd);
        fd = -1;
        return;
    }
    pthread_detach(tid);  // 线程自动回收，无需等待
}

int dht11_read(char *humi, char *temp)
{
    pthread_mutex_lock(&dht11_mutex);
    *humi = g_humi;
    *temp = g_temp;
    pthread_mutex_unlock(&dht11_mutex);
    return 0;
}

// 可选：程序退出时调用此函数停止线程并关闭设备
void dht11_cleanup(void)
{
    thread_running = 0;   // 通知线程退出
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
}
