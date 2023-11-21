# Copyright (C) 2023, Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: MIT

ifndef ENABLE_DMA_64BIT_ADDR
	ENABLE_DMA_64BIT_ADDR := 0
endif

ifndef DMA_PHYS_ADDR
	DMA_PHYS_ADDR := 0xA4000000
endif

ARCH := arm64
CROSS_COMPILE := aarch64-linux-gnu-
CFLAGS=-Wall \
 -DENABLE_DMA_64BIT_ADDR=$(ENABLE_DMA_64BIT_ADDR)
CDEBUG=-g
CC=aarch64-linux-gnu-gcc
CFLAGS_HSDP= -Wall -I./src/ -lm \
 -DDMA_PHYS_ADDR=$(DMA_PHYS_ADDR)

XVC_CFILE := \
 $(wildcard src/xvc_dpc.c) \
 $(wildcard src/xvcserver*.c)

HSDP_CFILES := \
 $(wildcard src/hsdp*.c) \
 $(wildcard src/dpc*.c) \

OBJDIR := obj

XVC_OFILE := $(addprefix $(OBJDIR)/,$(addsuffix .o,$(basename $(sort $(XVC_CFILE)))))
HSDP_OFILES := $(addprefix $(OBJDIR)/,$(addsuffix .o,$(basename $(sort $(HSDP_CFILES)))))

debug: DEBUG = -ggdb
debug: all

all: xvc_dpc

.PHONY: clean
clean: clean_hsdplib

xvc_dpc: $(HSDP_OFILES) $(XVC_OFILE)
	$(CC) $(CFLAGS_HSDP) -o $@ $(HSDP_OFILES) $(XVC_OFILE)

$(OBJDIR)/src/%.o: src/%.c | $(OBJDIR)
	$(CC) $(DEBUG) $(CCWARN) $(CCARGS) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/src/hsdp_lib.o: src/hsdp_lib.c | $(OBJDIR)
	$(CC) $(DEBUG) $(CCWARN) $(CCARGS) $(CFLAGS_HSDP) -c -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)/src

clean_hsdplib:
	rm -rf $(OBJDIR)
	rm -rf xvc_dpc
