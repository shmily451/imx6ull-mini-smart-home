#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "mqtt.h"

#define DHT11_DEVICE "/dev/mydht11"
#define LED_BRIGHTNESS "/sys/class/leds/sys-led/brightness"

#define BROKER_HOST "broker.emqx.io"
#define BROKER_PORT 1883
#define CLIENT_ID "imx6ull_dht11"

#define TOPIC_TEMPERATURE "smart-home/dht11/temperature"
#define TOPIC_HUMIDITY "smart-home/dht11/humidity"
#define TOPIC_LED "smart-home/led"

static struct mqtt_client client;
static uint8_t mqtt_sendbuf[2048];
static uint8_t mqtt_recvbuf[2048];

static int dht11_fd = -1;
static char g_humi = 0, g_temp = 0;
static pthread_mutex_t dht11_mutex = PTHREAD_MUTEX_INITIALIZER;
static int thread_running = 1;
static int led_req = -1;

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

static void on_publish(void **state, struct mqtt_response_publish *pub)
{
    char topic[128], payload[64];
    int tl = pub->topic_name_size < 127 ? pub->topic_name_size : 127;
    int pl = pub->application_message_size < 63 ? pub->application_message_size : 63;
    memcpy(topic, pub->topic_name, tl); topic[tl] = '\0';
    memcpy(payload, pub->application_message, pl); payload[pl] = '\0';
    printf("MQTT recv: %s = %s\n", topic, payload);
    if (strcmp(topic, TOPIC_LED) == 0) led_control(payload);
}

static int connect_socket(void)
{
    struct sockaddr_in addr;
    struct hostent *he;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return -1; }
    he = gethostbyname(BROKER_HOST);
    if (!he) { fprintf(stderr, "gethostbyname failed\n"); close(sock); return -1; }
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(BROKER_PORT);
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect"); close(sock); return -1;
    }
    return sock;
}

static void my_reconnect(void)
{
    int sock;
    do {
        sock = connect_socket();
        if (sock < 0) { sleep(3); continue; }
        mqtt_reinit(&client, sock, mqtt_sendbuf, sizeof(mqtt_sendbuf),
                    mqtt_recvbuf, sizeof(mqtt_recvbuf));
        enum MQTTErrors err = mqtt_connect(&client, CLIENT_ID, NULL, NULL, 0,
                                           NULL, NULL, MQTT_CONNECT_CLEAN_SESSION, 60);
        if (err != MQTT_OK) { close(sock); sleep(3); sock = -1; }
    } while (sock < 0);
    printf("MQTT reconnected\n");
    mqtt_subscribe(&client, TOPIC_LED, 0);
}

int main(void)
{
    char ts[8], hs[8];
    time_t last_pub = 0;

    dht11_fd = open(DHT11_DEVICE, O_RDWR | O_NONBLOCK);
    if (dht11_fd < 0) { perror("open dht11"); return -1; }

    pthread_t tid;
    pthread_create(&tid, NULL, dht11_poll_thread, NULL);
    pthread_detach(tid);

    int sock = connect_socket();
    if (sock < 0) return -1;

    mqtt_init(&client, sock, mqtt_sendbuf, sizeof(mqtt_sendbuf),
              mqtt_recvbuf, sizeof(mqtt_recvbuf), on_publish);
    enum MQTTErrors err = mqtt_connect(&client, CLIENT_ID, NULL, NULL, 0,
                                       NULL, NULL, MQTT_CONNECT_CLEAN_SESSION, 60);
    if (err != MQTT_OK) { fprintf(stderr, "mqtt_connect failed: %d\n", err); return -1; }
    printf("MQTT connected as %s\n", CLIENT_ID);

    mqtt_subscribe(&client, TOPIC_LED, 0);
    printf("Subscribed to %s\n", TOPIC_LED);

    for (;;) {
        mqtt_sync(&client);
        if (client.error != MQTT_OK) {
            fprintf(stderr, "MQTT error: %d, reconnecting...\n", client.error);
            my_reconnect();
            continue;
        }

        time_t now = time(NULL);
        if (now - last_pub >= 5) {
            last_pub = now;
            pthread_mutex_lock(&dht11_mutex);
            char h = g_humi, t = g_temp;
            pthread_mutex_unlock(&dht11_mutex);
            snprintf(ts, sizeof(ts), "%d", (int)t);
            snprintf(hs, sizeof(hs), "%d", (int)h);
            if (t != 0 || h != 0) {
                mqtt_publish(&client, TOPIC_TEMPERATURE, ts, strlen(ts), MQTT_PUBLISH_QOS_0);
                mqtt_publish(&client, TOPIC_HUMIDITY, hs, strlen(hs), MQTT_PUBLISH_QOS_0);
                printf("Pub: T=%s H=%s\n", ts, hs);
            }
        }
        usleep(100000);
    }
    close(dht11_fd);
    return 0;
}
