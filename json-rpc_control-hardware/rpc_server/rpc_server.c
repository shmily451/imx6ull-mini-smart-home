#include <jsonrpc-c.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include "led.h"
#include "dht11.h"
#include "rpc.h"

cJSON* rpc_led_control(jrpc_context *ctx, cJSON *params, cJSON *id)
{
    cJSON *status = cJSON_GetArrayItem(params, 0);
    if (!status) {
        return cJSON_CreateString("Missing parameter");
    }
    int on = status->valueint;
    led_control(on);
    return cJSON_CreateNumber(0);   // 返回 0 表示成功
}

cJSON* rpc_dht11_read(jrpc_context *ctx, cJSON *params, cJSON *id)
{
    int array[2] = {0, 0};
    // 一直读到成功为止（若硬件一直失败则会死循环，建议增加超时）
    while (dht11_read((char*)&array[0], (char*)&array[1]) != 0) {
        // 可加入短暂延时，避免占用CPU过猛
        usleep(100000);  // 100ms
    }
    return cJSON_CreateIntArray(array, 2);
}
 


int main(int argc, char **argv)
{
    struct jrpc_server server;
    int port = 8888;   // 可改为其他端口

    // 初始化硬件
    dht11_init();

    // 初始化 RPC 服务器
    if (jrpc_server_init(&server, port) != 0) {
        fprintf(stderr, "Failed to init RPC server\n");
        return -1;
    }

    // 注册方法
    jrpc_register_procedure(&server, rpc_led_control, "led_control", NULL);
    jrpc_register_procedure(&server, rpc_dht11_read,   "dht11_read",   NULL);

    printf("RPC Server running on port %d\n", port);
    jrpc_server_run(&server);   // 进入事件循环（阻塞）

    jrpc_server_destroy(&server);
    return 0;
}
