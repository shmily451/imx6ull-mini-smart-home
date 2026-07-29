# i.MX6ULL 智能家居温湿度采集系统

基于 **NXP i.MX6ULL** 嵌入式平台搭建的智能家居温湿度采集系统，打通 Linux 内核驱动 → 用户态进程通信 → 人机交互全链路。

## 功能特性

- **DHT11 内核驱动** — 字符设备驱动，中断方式精确采集时序，支持 CRC 校验
- **用户态 sysfs 读取** — 备选方案，通过 `/sys/class/gpio` 软件模拟时序
- **JSON-RPC 进程通信** — 前后台分离架构，后台负责硬件交互，前台专注界面展示
- **多线程数据采集** — 独立线程周期性读取温湿度，互斥锁保护共享数据
- **动态模块加载** — `insmod` 动态加载驱动，`mknod` 创建设备节点

## 项目架构

```
┌─────────────────────────────────────────────────────┐
│                   Qt 用户界面 (前台)                  │
│                (未包含在本仓库中)                      │
└──────────────────────┬──────────────────────────────┘
                       │ JSON-RPC (TCP :8888)
                       ▼
┌─────────────────────────────────────────────────────┐
│                RPC 服务器 (后台进程)                   │
│   rpc_server   ←→   libjsonrpcc   ←→   libev        │
│      │                    │                              │
│   dht11_read()        led_control()                  │
└──────┬──────────────────────┬────────────────────────┘
       │  /dev/mydht11        │  /sys/class/leds/sys-led
       ▼                      ▼
┌─────────────────────────────────────────────────────┐
│  DHT11 内核驱动 (dht11_drv.ko)     板载 LED          │
│  └─ 中断解析时序  ─  40bit 数据帧                      │
│  └─ 环形缓冲区 + 等待队列                              │
└─────────────────────────────────────────────────────┘
```

## 目录结构

```
imx6ull-mini-smart-home/
├── 02_dht11_drv_imx6ull/     # DHT11 内核驱动模块
│   ├── dht11_drv.c           # 字符设备驱动主文件
│   ├── dht11_sysfs.c         # 用户态 sysfs 读取（备选）
│   ├── dht11_test.c          # 驱动测试程序
│   └── Makefile              # 交叉编译 (arm-poky-linux-gnueabi)
│
├── json-rpc_control-hardware/ # 用户态应用层
│   ├── rpc_server/           # 后台服务（硬件交互）
│   │   ├── rpc_server.c      # RPC 服务器主程序
│   │   ├── dht11.c/h         # DHT11 多线程读取
│   │   ├── led.c/h           # LED 控制
│   │   ├── cJSON.c/h         # JSON 解析
│   │   └── Makefile
│   ├── rpc_client/           # 前台客户端
│   │   ├── rpc_client.c      # RPC 客户端主程序
│   │   ├── dht11.c/h         # DHT11 客户端封装
│   │   ├── led.c/h           # LED 客户端封装
│   │   ├── cJSON.c/h         # JSON 解析
│   │   └── Makefile
│   ├── jsonrpc-c/            # git submodule: JSON-RPC C 库
│   └── libev/                # git submodule: libev 事件循环库
│
├── README.md
├── TROUBLESHOOTING.md         # 常见问题与解决方案
└── .gitignore
```

## 环境依赖

### 硬件

| 组件 | 说明 |
|---|---|
| 主控 | i.MX6ULL (正点原子阿尔法开发板) |
| 传感器 | DHT11 温湿度传感器 (GPIO4) |
| LED | 板载 sys-led |

### 软件

| 工具 | 版本/说明 |
|---|---|
| Linux 内核 | 4.1.15 (自定义编译) |
| 交叉编译器 | arm-poky-linux-gnueabi-gcc (fsl-imx-x11 4.1.15-2.1.0) |
| jsonrpc-c | JSON-RPC 2.0 C 库 |
| libev | 高性能事件循环库 |
| Qt (可选) | Qt5 用于人机界面 |

## 快速开始

### 1. 初始化子模块

```bash
git submodule update --init --recursive
```

### 2. 编译第三方库

```bash
# 编译 jsonrpc-c
cd json-rpc_control-hardware/jsonrpc-c
mkdir -p tmp
./configure --prefix=$(pwd)/tmp --host=arm-poky-linux-gnueabi
make && make install

# 编译 libev
cd ../libev
mkdir -p tmp
./configure --prefix=$(pwd)/tmp --host=arm-poky-linux-gnueabi
make && make install
```

### 3. 编译内核驱动

```bash
cd 02_dht11_drv_imx6ull
make clean && make
```

得到 `dht11_drv.ko`。

### 4. 编译 RPC 服务器

```bash
cd json-rpc_control-hardware/rpc_server
make clean && make
```

### 5. 编译 RPC 客户端

```bash
cd json-rpc_control-hardware/rpc_client
make clean && make
```

## 部署与运行

### 在开发板上

```bash
# 1. 加载驱动
insmod dht11_drv.ko
cat /proc/devices | grep dht11  # 获取主设备号
mknod /dev/mydht11 c <主设备号> 0

# 2. 启动 RPC 服务器（后台运行）
./rpc_server &

# 3. 测试 LED 控制（客户端）
./rpc_client 127.0.0.1 led 1    # 开灯
./rpc_client 127.0.0.1 led 0    # 关灯

# 4. 读取温湿度
./rpc_client 127.0.0.1 dht11

# 5. 直接测试驱动
./dht11_test /dev/mydht11
```

### 设置开机自启

将以下内容添加到 `/etc/init.d/rcS` 或使用 systemd service：

```bash
insmod /root/dht11_drv.ko
mknod /dev/mydht11 c 247 0
/root/rpc_server &
```

## 驱动工作原理

1. `read()` 时 GPIO4 发送 20ms 低电平起始信号
2. 切换 GPIO 为输入，注册双沿中断
3. DHT11 返回 40bit 数据帧（含校验和）
4. 中断服务程序记录 84 个时间戳，解析温度/湿度
5. CRC 校验通过后存入环形缓冲区，唤醒用户进程

## 许可证

GPL v2
