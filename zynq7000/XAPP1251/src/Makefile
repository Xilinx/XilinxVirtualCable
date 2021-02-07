export CROSS_COMPILE := aarch64-linux-gnu-
export ARCH := arm64
MYCC := $(CROSS_COMPILE)gcc

DRIVER := xilinx_xvc_driver
XVCSERVER := xvcServer

.PHONY: mmap
mmap: $(XVCSERVER)_mmap

.PHONY: all
all: mmap 

$(XVCSERVER)_mmap: $(XVCSERVER).c
	$(MYCC) -Wall -o $@ $^

.PHONY: clean
clean:
	rm -f $(XVCSERVER)_mmap
