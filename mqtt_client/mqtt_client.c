#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include "mqtt.h"

#define DHT11_DEVICE "/dev/mydht11"
#define LED_BRIGHTNESS "/sys/class/leds/sys-led/brightness"

#define BROKER_HOST "broker.emqx.io"
#define BROKER_PORT 1883

#define TOPIC_TEMPERATURE "smart-home/dht11/temperature"
#define TOPIC_HUMIDITY "smart-home/dht11/humidity"
#define TOPIC_LED "smart-home/led"

static struct mqtt_client mqtt_client;
static uint8_t mqtt_sendbuf[2048];
static uint8_t mqtt_recvbuf[2048];

static int dht11_fd = -1;
static char g_humi = 0, g_temp = 0;
static pthread_mutex_t dht11_mutex = PTHREAD_MUTEX_INITIALIZER;
static int thread_running = 1;

static void led_control(const char *cmd)
{
    int fd = open(LED_BRIGHTNESS, O_RDWR);
    if (fd < 0) { perror("open led"); return; }
    if (strcmp(cmd, "ON") == 0)
        write(fd, "1\n", 2);
    else
        write(fd, "0\n", 2);
    close(fd);
}

static void *dht11_poll_thread(void *arg)
{
    char buf[2];
    while (thread_running) {
        if (dht11_fd >= 0 && read(dht11_fd, buf, 2) == 2) {
            pthread_mutex_lock(&dht11_mutex);
            g_humi = buf[0]; g_temp = buf[1];
            pthread_mutex_unlock(&dht11_mutex);
        } else { usleep(100000); }
        sleep(1);
    }
    return NULL;
}

static void on_connected(void *context)
{
    printf("Connected to MQTT broker\n");
    mqtt_subscribe(&mqtt_client, TOPIC_LED, 0);
}

static void on_message(void *context, struct mqtt_message *msg)
{
    char topic[128], payload[64];
    int tl = msg->topic_len < 127 ? msg->topic_len : 127;
    int pl = msg->payload_len < 63 ? msg->payload_len : 63;
    memcpy(topic, msg->topic, tl); topic[tl] = '\0';
    memcpy(payload, msg->payload, pl); payload[pl] = '\0';
    printf("MQTT: %s = %s\n", topic, payload);
    if (strcmp(topic, TOPIC_LED) == 0) led_control(payload);
}

static void on_disconnected(void *context)
{
    printf("Disconnected, reconnecting...\n");
}

int main(void)
{
    dht11_fd = open(DHT11_DEVICE, O_RDWR | O_NONBLOCK);
    if (dht11_fd < 0) { perror("open dht11"); return -1; }

    pthread_t tid;
    pthread_create(&tid, NULL, dht11_poll_thread, NULL);
    pthread_detach(tid);

    mqtt_init(&mqtt_client, mqtt_sendbuf, sizeof(mqtt_sendbuf),
              mqtt_recvbuf, sizeof(mqtt_recvbuf),
              on_message, on_connected, on_disconnected, NULL);

    while (1) {
        if (mqtt_connect(&mqtt_client, BROKER_HOST, BROKER_PORT, NULL, NULL, NULL, NULL, NULL) != 0) {
            fprintf(stderr, "Connect failed, retry in 5s...\n");
            sleep(5); continue;
        }
        while (mqtt_client.state == MQTT_CLIENT_STATE_CONNECTED) {
            pthread_mutex_lock(&dht11_mutex);
            char h = g_humi, t = g_temp;
            pthread_mutex_unlock(&dht11_mutex);

            char ts[4], hs[4];
            snprintf(ts, sizeof(ts), "%d", (int)t);
            snprintf(hs, sizeof(hs), "%d", (int)h);

            if (t != 0 || h != 0) {
                mqtt_publish(&mqtt_client, TOPIC_TEMPERATURE, ts, strlen(ts), 0, 0);
                mqtt_publish(&mqtt_client, TOPIC_HUMIDITY, hs, strlen(hs), 0, 0);
                printf("Pub: T=%s H=%s\n", ts, hs);
            }
            for (int i = 0; i < 50; i++) {
                mqtt_sync(&mqtt_client);
                usleep(100000);
            }
        }
        sleep(3);
    }
    close(dht11_fd);
    return 0;
}
