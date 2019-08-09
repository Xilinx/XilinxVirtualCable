export CROSS_COMPILE := aarch64-linux-gnu-
export ARCH := arm64
MYCC := $(CROSS_COMPILE)gcc

DRIVER := xilinx_xvc_driver
XVCSERVER := xvcServer
TEST := verify_$(DRIVER)
IOCTL_HDR_DIR := ../driver

.PHONY: ioctl
ioctl: $(XVCSERVER)_ioctl

.PHONY: mmap
mmap: $(XVCSERVER)_mmap

.PHONY: test
test: $(TEST)

.PHONY: all
all: ioctl mmap test

$(XVCSERVER)_ioctl: $(XVCSERVER).c
	$(MYCC) -Wall -I$(IOCTL_HDR_DIR) -DUSE_IOCTL -o $@ $^

$(XVCSERVER)_mmap: $(XVCSERVER).c
	$(MYCC) -Wall -o $@ $^

$(TEST): $(TEST).c
	$(MYCC) -I$(IOCTL_HDR_DIR) -o $@ $^

.PHONY: clean
clean:
	rm -f $(XVCSERVER)_mmap
	rm -f $(XVCSERVER)_ioctl
	rm -f $(TEST)
