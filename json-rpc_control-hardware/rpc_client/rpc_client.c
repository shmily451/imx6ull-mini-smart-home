#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "cJSON.h"

#define SERVER_PORT 8888

int connect_server(const char *ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return -1; }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_aton(ip, &addr.sin_addr) == 0) { fprintf(stderr, "Invalid IP\n"); close(sock); return -1; }
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("connect"); close(sock); return -1; }
    return sock;
}

char* rpc_call(int sock, const char *method, const char *params_json) {
    char request[512];
    snprintf(request, sizeof(request),
             "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s,\"id\":1}",
             method, params_json);
    if (send(sock, request, strlen(request), 0) < 0) { perror("send"); return NULL; }
    char buffer[1024];
    int len = recv(sock, buffer, sizeof(buffer)-1, 0);
    if (len <= 0) { perror("recv"); return NULL; }
    buffer[len] = '\0';
    return strdup(buffer);
}

int rpc_led_control(int sock, int on) {
    char params[16];
    snprintf(params, sizeof(params), "[%d]", on);
    char *resp = rpc_call(sock, "led_control", params);
    if (!resp) return -1;
    cJSON *root = cJSON_Parse(resp);
    int ret = -1;
    if (root) {
        cJSON *result = cJSON_GetObjectItem(root, "result");
        if (result && result->type == cJSON_Number) ret = result->valueint;
        cJSON_Delete(root);
    }
    free(resp);
    return ret;
}

int rpc_dht11_read(int sock, char *humi, char *temp) {
    char *resp = rpc_call(sock, "dht11_read", "[]");
    if (!resp) return -1;
    cJSON *root = cJSON_Parse(resp);
    int ret = -1;
    if (root) {
        cJSON *result = cJSON_GetObjectItem(root, "result");
        if (result && result->type == cJSON_Array && cJSON_GetArraySize(result) == 2) {
            *humi = (char)cJSON_GetArrayItem(result, 0)->valueint;
            *temp = (char)cJSON_GetArrayItem(result, 1)->valueint;
            ret = 0;
        }
        cJSON_Delete(root);
    }
    free(resp);
    return ret;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage:\n");
        printf("  %s <server_ip> led <0|1>\n", argv[0]);
        printf("  %s <server_ip> dht11\n", argv[0]);
        return -1;
    }
    const char *ip = argv[1];
    int sock = connect_server(ip, SERVER_PORT);
    if (sock < 0) return -1;

    if (strcmp(argv[2], "led") == 0 && argc == 4) {
        int on = atoi(argv[3]);
        int ret = rpc_led_control(sock, on);
        printf("led_control result: %d\n", ret);
    } else if (strcmp(argv[2], "dht11") == 0) {
        char humi, temp;
        if (rpc_dht11_read(sock, &humi, &temp) == 0) {
            printf("Humidity: %d%%, Temperature: %d°C\n", (int)humi, (int)temp);
        } else {
            printf("dht11 read failed\n");
        }
    } else {
        printf("Invalid command\n");
    }
    close(sock);
    return 0;
}


