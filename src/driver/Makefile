DRIVER := xilinx_xvc_driver
export CROSS_COMPILE := aarch64-linux-gnu-
export ARCH := arm64

ccflags-y := -DLOG_PREFIX=\"$(DRIVER):\ \"
$(DRIVER)-y := \
	xvc_driver_base.o \
	xvc_driver.o

obj-m += $(DRIVER).o

SRC := $(shell pwd)

modules:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

.PHONY: clean
clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod.c
	rm -f Module.markers Module.symvers modules.order
	rm -rf .tmp_versions Modules.symvers
