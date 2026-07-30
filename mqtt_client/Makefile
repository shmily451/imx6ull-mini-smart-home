ARCH := arm
CROSS_COMPILE := /opt/fsl-imx-x11/4.1.15-2.1.0/sysroots/x86_64-pokysdk-linux/usr/bin/arm-poky-linux-gnueabi/arm-poky-linux-gnueabi-
SYSROOT := /opt/fsl-imx-x11/4.1.15-2.1.0/sysroots/cortexa7hf-neon-poky-linux-gnueabi
CC := $(CROSS_COMPILE)gcc
CFLAGS := --sysroot=$(SYSROOT) -mfloat-abi=hard -mfpu=vfpv3 -pthread -I.
LDFLAGS := -pthread
OBJS := mqtt.o mqtt_pal.o mqtt_client.o
all: mqtt_client
mqtt_client: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<
clean:
	rm -f mqtt_client *.o
.PHONY: all clean
