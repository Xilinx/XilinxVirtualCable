/*
Copyright (C) 2023, Advanced Micro Devices, Inc. All rights reserved.
SPDX-License-Identifier: MIT
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <pthread.h>
#include "hsdp_lib.h"
#include "hsdp.h"

//#define VERBOSE

#ifndef DMA_PHYS_ADDR
// Physical address and size of AXI DMA IP.
#define DMA_PHYS_ADDR   0xA4000000
#endif
#define DMA_SIZE 0x1000

// Physical address and size of memory reserved to be used as buffer by AXI DMA.
#define BUFFER_PHYS_ADDR   0x7FF00000
#define BUFFER_SIZE 0x40000
#define MAX_MMAP_BUFFERS 2

typedef struct loc_buffer {
    unsigned char* buffer;
    uint64_t       physical;
    size_t         size;
} loc_buffer;

static loc_buffer *dma = NULL;
static loc_buffer *data_buffer = NULL;

static loc_buffer mmap_buffers[MAX_MMAP_BUFFERS];
static int mmap_buffer_count = 0;

static size_t dma_phys_addr = DMA_PHYS_ADDR;
static size_t dma_size = DMA_SIZE;

static size_t buffer_phys_addr = BUFFER_PHYS_ADDR;
static size_t buffer_size = BUFFER_SIZE;

static loc_buffer *mmap_buffer(int mem_fd, off_t address, size_t num_bytes) {
    unsigned char *mem = NULL;
    loc_buffer *buffer = NULL;

    assert(mmap_buffer_count < MAX_MMAP_BUFFERS);

    mem = (unsigned char*) mmap(0, num_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, address);
    if (mem == MAP_FAILED) {
        printf("mmap args: %lu bytes, at %lu\n", num_bytes, address);
        perror("Error mmapping the file");
        exit(EXIT_FAILURE);
    }

    buffer = &mmap_buffers[mmap_buffer_count++];

    buffer->buffer = mem;
    buffer->physical = address;
    buffer->size = num_bytes;

    return buffer;
}

static void free_buffers(void) {
    int i = 0;

    for (i = 0; i < mmap_buffer_count; ++i) {
        if (munmap((void *) mmap_buffers[i].buffer, mmap_buffers[i].size)) {
            printf("Failed to unmap 0x%lX\n", (uint64_t) mmap_buffers[i].buffer);
        }
    }
    mmap_buffer_count = 0;
}

static void alloc_buffers() {
    int mem_fd = -1;
    size_t addr = 0;
    size_t size = 0;

    if ((mem_fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
        perror("Failed to open /dev/mem");
        exit(1);
    }

    addr = dma_phys_addr;
    size = dma_size;
    printf("INFO: AXI DMA addr = 0x%lX, size = 0x%lX\n", (uint64_t) addr, (uint64_t) size);
    dma = mmap_buffer(mem_fd, addr, size);

    addr = buffer_phys_addr;
    size = buffer_size;
    printf("INFO: Buffer addr = 0x%lX, size = 0x%lX\n", (uint64_t) addr, (uint64_t) size);
    data_buffer = mmap_buffer(mem_fd, addr, size);

    close(mem_fd);
}

static void hsdp_setup(hsdp_dma *hsdp) {
    const size_t desc_count = MAX_EGRESS_PACKETS;
    size_t packet_buffer_offset = 0;
    loc_buffer *packet_buffer = NULL;

    hsdp->idesc.buffer_size = desc_count * DMA_DESC_SIZE;
    hsdp->idesc.buffer      = data_buffer->buffer;
    hsdp->idesc.axi_base    = data_buffer->physical;
    hsdp->idesc.packet_size = DMA_DESC_SIZE;

    hsdp->edesc.buffer_size = hsdp->idesc.buffer_size;
    hsdp->edesc.buffer      = hsdp->idesc.buffer + hsdp->idesc.buffer_size;
    hsdp->edesc.axi_base    = hsdp->idesc.axi_base + hsdp->idesc.buffer_size;
    hsdp->edesc.packet_size = hsdp->idesc.packet_size;

    packet_buffer = data_buffer;
    packet_buffer_offset = hsdp->edesc.buffer_size * 2;

    hsdp->ipkts.buffer_size = desc_count * DMA_PACKET_BUFF_SIZE_DEFAULT;
    hsdp->ipkts.buffer      = packet_buffer->buffer + packet_buffer_offset;
    hsdp->ipkts.axi_base    = packet_buffer->physical + packet_buffer_offset;
    hsdp->ipkts.packet_size = DMA_PACKET_BUFF_SIZE_DEFAULT;

    hsdp->epkts.buffer_size = hsdp->ipkts.buffer_size;
    hsdp->epkts.buffer      = hsdp->ipkts.buffer + hsdp->ipkts.buffer_size;
    hsdp->epkts.axi_base    = hsdp->ipkts.axi_base + hsdp->ipkts.buffer_size;
    hsdp->epkts.packet_size = hsdp->ipkts.packet_size;

    hsdp_enable(hsdp);
}

void setup_dma_region(size_t addr, size_t size) {
    dma_phys_addr = addr;
    if (size) dma_size = size;
}

void setup_buffer_region(size_t addr, size_t size) {
    buffer_phys_addr = addr;
    if (size) buffer_size = size;
}

uint64_t hsdp_open(void) {
    hsdp_dma *hsdp = NULL;

    if (mmap_buffer_count == 0) {
        alloc_buffers();
    }

    hsdp = (hsdp_dma *) malloc(sizeof *hsdp);
    memset(hsdp, 0, sizeof *hsdp);
    hsdp->dma_base = (uint64_t) dma->buffer;

    hsdp_setup(hsdp);

#ifdef VERBOSE
    printf("%s: Open %p\n", __func__, hsdp);
#endif

    return (uint64_t) hsdp;
}

int hsdp_close(uint64_t handle) {
    hsdp_dma *hsdp = (hsdp_dma *) handle;

#ifdef VERBOSE
    printf("%s: Closing %p\n", __func__, hsdp);
#endif

    if (hsdp) {
        free(hsdp);
    } else {
        free_buffers();
    }

    return 0;
}


