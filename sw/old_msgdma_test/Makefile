ifndef CROSS_COMPILE
$(error CROSS_COMPILE is not set. Example: export CROSS_COMPILE=arm-none-linux-gnueabihf-)
endif

KDIR  := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))../../repos/linux-socfpga)
ARCH  := arm

obj-m := msgdma_test.o

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
