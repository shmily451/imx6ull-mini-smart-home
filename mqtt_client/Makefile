TARGET=mqtt_client
ARCH := arm
CROSS_COMPILE := /opt/fsl-imx-x11/4.1.15-2.1.0/sysroots/x86_64-pokysdk-linux/usr/bin/arm-poky-linux-gnueabi/arm-poky-linux-gnueabi-
SYSROOT := /opt/fsl-imx-x11/4.1.15-2.1.0/sysroots/cortexa7hf-neon-poky-linux-gnueabi
CC := $(CROSS_COMPILE)gcc

PAHO_MQTT_DIR := /home/book/Desktop/paho.mqtt.c/

CFLAGS := --sysroot=$(SYSROOT) -mfloat-abi=hard -mfpu=vfpv3 -I$(PAHO_MQTT_DIR)/src -O
LDFLAGS := -L$(PAHO_MQTT_DIR)/build/output -lpaho-mqtt3c -pthread

all:
	$(CC) $(CFLAGS) -o $(TARGET) mqtt_client.c $(LDFLAGS)

clean:
	rm -f $(TARGET) *.o
