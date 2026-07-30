#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include "MQTTClient.h"

#define DHT11_DEVICE  "/dev/mydht11"
#define LED_BRIGHTNESS "/sys/class/leds/sys-led/brightness"

#define ADDRESS     "tcp://192.168.1.200:1883"
#define CLIENTID    "imx6ull_dht11"
#define TOPIC_TEMP  "smart-home/dht11/temperature"
#define TOPIC_HUMI  "smart-home/dht11/humidity"
#define TOPIC_LED   "smart-home/led"
#define QOS         0
#define TIMEOUT     10000L

static MQTTClient client;
static int dht11_fd = -1;
static char g_humi = 0, g_temp = 0;
static pthread_mutex_t dht11_mutex = PTHREAD_MUTEX_INITIALIZER;
static int thread_running = 1;

static void led_control(const char *cmd)
{
    int fd = open(LED_BRIGHTNESS, O_RDWR);
    if (fd < 0) { perror("open led"); return; }
    if (strcmp(cmd, "ON") == 0) write(fd, "1\n", 2);
    else write(fd, "0\n", 2);
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

static int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    printf("MQTT recv: %s = %.*s\n", topicName, message->payloadlen, (char*)message->payload);
    if (strcmp(topicName, TOPIC_LED) == 0) {
        led_control((char*)message->payload);
    }
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

static void connlost(void *context, char *cause)
{
    printf("Connection lost: %s\n", cause ? cause : "unknown");
}

static void delivered(void *context, MQTTClient_deliveryToken dt)
{
}

int main(void)
{
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    char ts[8], hs[8];
    int rc;

    dht11_fd = open(DHT11_DEVICE, O_RDWR | O_NONBLOCK);
    if (dht11_fd < 0) { perror("open dht11"); return -1; }

    pthread_t tid;
    pthread_create(&tid, NULL, dht11_poll_thread, NULL);
    pthread_detach(tid);

    if ((rc = MQTTClient_create(&client, ADDRESS, CLIENTID,
         MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to create client, rc=%d\n", rc); return -1;
    }

    if ((rc = MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered)) != MQTTCLIENT_SUCCESS) {
        printf("Failed set callbacks, rc=%d\n", rc); return -1;
    }

    conn_opts.keepAliveInterval = 30;
    conn_opts.cleansession = 1;

    while (1) {
        if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
            printf("Connect failed (rc=%d), retry in 3s...\n", rc);
            sleep(3); continue;
        }
        printf("MQTT connected\n");

        MQTTClient_subscribe(client, TOPIC_LED, QOS);
        printf("Subscribed to %s\n", TOPIC_LED);

        while (1) {
            pthread_mutex_lock(&dht11_mutex);
            char h = g_humi, t = g_temp;
            pthread_mutex_unlock(&dht11_mutex);

            snprintf(ts, sizeof(ts), "%d", (int)t);
            snprintf(hs, sizeof(hs), "%d", (int)h);

            MQTTClient_message pubmsg = MQTTClient_message_initializer;
            pubmsg.payload = ts;
            pubmsg.payloadlen = strlen(ts);
            pubmsg.qos = QOS;
            if (MQTTClient_publishMessage(client, TOPIC_TEMP, &pubmsg, NULL) != MQTTCLIENT_SUCCESS)
                printf("pub temp fail\n");

            pubmsg.payload = hs;
            pubmsg.payloadlen = strlen(hs);
            if (MQTTClient_publishMessage(client, TOPIC_HUMI, &pubmsg, NULL) != MQTTCLIENT_SUCCESS)
                printf("pub humi fail\n");

            printf("Pub: T=%s H=%s\n", ts, hs);
            sleep(5);
        }

        MQTTClient_disconnect(client, 1000);
        sleep(3);
    }

    MQTTClient_destroy(&client);
    close(dht11_fd);
    return 0;
}
