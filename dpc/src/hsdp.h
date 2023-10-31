/*
Copyright (C) 2023, Advanced Micro Devices, Inc. All rights reserved.
SPDX-License-Identifier: MIT
*/

#define MAX_EGRESS_PACKETS 4

struct hsdp_packet_allocator;
struct hsdp_packet;

typedef struct hsdp_region {
    uint64_t axi_base;
    unsigned char *buffer;
    size_t buffer_size;
    size_t packet_size;
    int last;
} hsdp_region;

typedef struct {
    uint64_t start;
    uint64_t size;
} type_region;

typedef struct hsdp_dma {
    uint64_t dma_base;

    hsdp_region ipkts;
    hsdp_region epkts;

    struct hsdp_packet_allocator *ialloc;
    struct hsdp_packet_allocator *ealloc;

    hsdp_region idesc;
    hsdp_region edesc;

    int error;
    unsigned char seq;

} hsdp_dma;

struct SLDpcPacket;

#define BUF_ALIGN(a) (a)
//#define BUF_ALIGN(a) ((((a) + 7) / 8) * 8)

#define DMA_DESC_SIZE     0x40

#define DMA_PACKET_OFFSET 0x6000
#define DMA_PACKET_BUFF_SIZE_DEFAULT 0x410

#define HSDP_CTL_REG_SIZE 8

#define HSDP_BUFFER_OFFSET(region, index) ((region).packet_size * (index))
#define HSDP_BUFFER(region, index) ((region).buffer + HSDP_BUFFER_OFFSET(region, index))
#define HSDP_COUNT(region) ((region).buffer_size / (region).packet_size)
#define HSDP_BUFFER_SIZE(region) ((region).packet_size * (HSDP_COUNT(region)))
#define HSDP_AXI_ADDR(region, index) ((uint64_t)((region).axi_base + HSDP_BUFFER_OFFSET(region, index)))
#define HSDP_REG(region, index, reg) (*(volatile uint32_t *)(HSDP_BUFFER(region, index) + (reg)))
#define HSDP_REG64(region, index, reg) (*(volatile uint64_t *)(HSDP_BUFFER(region, index) + (reg)))
#define HSDP_NEXT(region) ((((region).last + 1) * (region).packet_size) < (region).buffer_size ? ((region).last + 1) : 0)

#define DMA_PACKET_COUNT 4

#define INGRESS_PACKET(index) (HSDP_BUFFER(hsdp->ipkts, index))
#define EGRESS_PACKET(index) (HSDP_BUFFER(hsdp->epkts, index))

#define MOP_MATTR(addr) (((addr) >> 16) & 0xFFFF)
#define MOP_MADDR_HIGH(addr) (((addr) >> 32) & 0xFFFFFFFF)
#define MOP_MREAD(size, len, addr) ((1 << 28) | ((size) << 24) | (((len)-1) << 16) | ((addr) & 0xFFFF))
#define MOP_MWRITE(size, len, addr) ((2 << 28) | ((size) << 24) | (((len)-1) << 16) | ((addr) & 0xFFFF))
#define MOP_STREAM(size, len, addr) ((3 << 28) | ((size) << 24) | (((len)-1) << 16) | ((addr) & 0xFFFF))
#define MOP_COND(size, cond, addr) ((4 << 28) | ((size) << 24) | ((cond) << 16) | ((addr) & 0xFFFF))
#define MOP_COMPARE(value, mask) (((value) << 16) | ((mask) & 0xFFFF))
#define MOP_SKIPIF(tf, cond, dest) ((0x55 << 24) | ((tf) << 23) | ((cond) << 16) | (dest))
#define MOP_TESTCOND(cond) ((0x53 << 24) | ((cond)<<16))
#define MOP_GETCOND (0x54 << 24)
#define MOP_SYNC (0x52 << 24)
#define MOP_RAGGR (0x50 << 24)

int hsdp_check(hsdp_dma *hsdp);
void hsdp_dump_desc(hsdp_dma *hsdp, size_t index, int egress);
void hsdp_dump_dma(hsdp_dma *hsdp);
void hsdp_dump_cntl(hsdp_dma *hsdp);
void hsdp_setup_dma(hsdp_dma *hsdp);
void hsdp_enable(hsdp_dma *hsdp);
void hsdp_flush_normal(hsdp_dma *hsdp);
void hsdp_enable_loopback(hsdp_dma *hsdp);
void hsdp_disable_loopback(hsdp_dma *hsdp);

int hsdp_send_packet(hsdp_dma *hsdp, uint32_t *buf, size_t word_count);
int hsdp_send_fast_packet(hsdp_dma *hsdp, int buf_index, size_t size);
int hsdp_poll_fast_packet(hsdp_dma *hsdp, uint32_t **buf, size_t *word_count, struct hsdp_packet **pkt);
int hsdp_receive_fast_packet(hsdp_dma *hsdp, uint32_t **buf, size_t *word_count, struct hsdp_packet **pkt);

int hsdp_setup_packets(hsdp_dma *hsdp, int buf_index, int num_packets, size_t word_count);
int hsdp_send_max_packets(hsdp_dma *hsdp);
int hsdp_check_egress_done(hsdp_dma *hsdp, int index);
