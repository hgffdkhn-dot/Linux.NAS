# 你的内核模块源码
obj-m += kern_chan.o

# 外部传入KDIR，默认为当前路径（便于本地调试）
KDIR ?= /lib/modules/$(shell uname -r)/build

all:
    make -C $(KDIR) M=$(PWD) ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules

clean:
    make -C $(KDIR) M=$(PWD) clean
