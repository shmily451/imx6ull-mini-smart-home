# --------------------------配置区（按需修改路径）-------------------------
ARCH := arm
# 交叉编译器前缀（which查询路径去掉末尾gcc）
CROSS_COMPILE := /opt/fsl-imx-x11/4.1.15-2.1.0/sysroots/x86_64-pokysdk-linux/usr/bin/arm-poky-linux-gnueabi/arm-poky-linux-gnueabi-
# Poky目标系统根文件系统sysroot
SYSROOT := /opt/fsl-imx-x11/4.1.15-2.1.0/sysroots/cortexa7hf-neon-poky-linux-gnueabi
# 内核源码绝对路径
KERN_DIR := /home/book/Desktop/zhengdian
# ------------------------------------------------------------------------
PWD := $(shell pwd)
obj-m += dht11_drv.o

all:
	# 编译内核驱动模块
	make -C $(KERN_DIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
	# 编译用户态测试程序（增加硬浮点ABI与sysroot参数）
	$(CROSS_COMPILE)gcc --sysroot=$(SYSROOT) -mfloat-abi=hard -mfpu=vfpv3 -o dht11_test dht11_test.c

clean:
	make -C $(KERN_DIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules clean
	rm -rf modules.order dht11_test

