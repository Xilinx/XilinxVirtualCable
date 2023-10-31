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

#define DMA_DESC_OFFSET   0
#define DMA_DESC_INGRESS(a) HSDP_AXI_ADDR(hsdp->idesc, a)
#define DMA_DESC_EGRESS(a)  HSDP_AXI_ADDR(hsdp->edesc, a)

#define REG_DESC_NEXT     0x00
#define REG_DESC_NEXT_MSB 0x04
#define REG_DESC_BUFF     0x08
#define REG_DESC_BUFF_MSB 0x0C
#define REG_DESC_CNTL     0x18
#define REG_DESC_STS      0x1C
#define REG_DESC_APP0     0x20


#define REG_DMA_INGRESS_CNTL     (*(volatile uint32_t *)((dma_base) + 0x00))
#define REG_DMA_INGRESS_STS      (*(volatile uint32_t *)((dma_base) + 0x04))
#define REG_DMA_INGRESS_CUR      (*(volatile uint32_t *)((dma_base) + 0x08))
#define REG_DMA_INGRESS_CUR_MSB  (*(volatile uint32_t *)((dma_base) + 0x0C))
#define REG_DMA_INGRESS_TAIL     (*(volatile uint32_t *)((dma_base) + 0x10))
#define REG_DMA_INGRESS_TAIL_MSB (*(volatile uint32_t *)((dma_base) + 0x14))


#if ENABLE_DMA_64BIT_ADDR != 0
#define REG_DMA_INGRESS_CUR64    (*(volatile uint64_t *)((dma_base) + 0x08))
#define REG_DMA_INGRESS_TAIL64   (*(volatile uint64_t *)((dma_base) + 0x10))
#else
#define REG_DMA_INGRESS_CUR64    REG_DMA_INGRESS_CUR
#define REG_DMA_INGRESS_TAIL64   REG_DMA_INGRESS_TAIL
#endif

#define REG_DMA_EGRESS_CNTL     (*(volatile uint32_t *)((dma_base) + 0x30))
#define REG_DMA_EGRESS_STS      (*(volatile uint32_t *)((dma_base) + 0x34))
#define REG_DMA_EGRESS_CUR      (*(volatile uint32_t *)((dma_base) + 0x38))
#define REG_DMA_EGRESS_CUR_MSB  (*(volatile uint32_t *)((dma_base) + 0x3C))
#define REG_DMA_EGRESS_TAIL     (*(volatile uint32_t *)((dma_base) + 0x40))
#define REG_DMA_EGRESS_TAIL_MSB (*(volatile uint32_t *)((dma_base) + 0x44))

#if ENABLE_DMA_64BIT_ADDR != 0
#define REG_DMA_EGRESS_CUR64    (*(volatile uint64_t *)((dma_base) + 0x38))
#define REG_DMA_EGRESS_TAIL64   (*(volatile uint64_t *)((dma_base) + 0x40))
#else
#define REG_DMA_EGRESS_CUR64    REG_DMA_EGRESS_CUR
#define REG_DMA_EGRESS_TAIL64   REG_DMA_EGRESS_TAIL
#endif

#ifdef SKIP_MSB_ADDR
static unsigned msb = 0;
#undef REG_DMA_INGRESS_CUR_MSB
#define REG_DMA_INGRESS_CUR_MSB msb
#undef REG_DMA_INGRESS_TAIL_MSB
#define REG_DMA_INGRESS_TAIL_MSB msb
#undef REG_DMA_EGRESS_CUR_MSB
#define REG_DMA_EGRESS_CUR_MSB msb
#undef REG_DMA_EGRESS_TAIL_MSB
#define REG_DMA_EGRESS_TAIL_MSB msb
#endif

#define AXI_INGRESS_DESC(index)        (HSDP_AXI_ADDR(hsdp->idesc, index))
#define AXI_EGRESS_DESC(index)         (HSDP_AXI_ADDR(hsdp->edesc, index))
#define REG_INGRESS_DESC(index, reg)   (HSDP_REG(hsdp->idesc, index, reg))
#define REG_EGRESS_DESC(index, reg)    (HSDP_REG(hsdp->edesc, index, reg))

#if ENABLE_DMA_64BIT_ADDR != 0
#define REG_INGRESS_DESC64(index, reg) (HSDP_REG64(hsdp->idesc, index, reg))
#define REG_EGRESS_DESC64(index, reg)  (HSDP_REG64(hsdp->edesc, index, reg))
#else
#define REG_INGRESS_DESC64(index, reg) REG_INGRESS_DESC(index, reg)
#define REG_EGRESS_DESC64(index, reg)  REG_EGRESS_DESC(index, reg)
#endif

#define AXI_INGRESS_PACKET(index)      (HSDP_AXI_ADDR(hsdp->ipkts, index))
#define AXI_EGRESS_PACKET(index)       (HSDP_AXI_ADDR(hsdp->epkts, index))

#define FATAL do { fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", \
  __LINE__, __FILE__, errno, strerror(errno)); exit(1); } while(0)


#define dsb(scope)    asm volatile("dsb " #scope : : : "memory")

//#define VERBOSE 1

static int set_last_error(hsdp_dma *hsdp, char *error) {
    printf("ERROR: Last error - %s\n", error);
    //hsdp->error = set_errno(ERR_OTHER, error);
    return 0;
}

void hsdp_dump_dma(hsdp_dma *hsdp) {
    uint64_t dma_base = hsdp->dma_base;

    printf("REG_DMA_INGRESS_CNTL     0x%08X\n", REG_DMA_INGRESS_CNTL);
    printf("REG_DMA_INGRESS_STS      0x%08X\n", REG_DMA_INGRESS_STS);
    printf("REG_DMA_INGRESS_CUR      0x%08X\n", REG_DMA_INGRESS_CUR);
    printf("REG_DMA_INGRESS_CUR_MSB  0x%08X\n", REG_DMA_INGRESS_CUR_MSB);
    printf("REG_DMA_INGRESS_TAIL     0x%08X\n", REG_DMA_INGRESS_TAIL);
    printf("REG_DMA_INGRESS_TAIL_MSB 0x%08X\n", REG_DMA_INGRESS_TAIL_MSB);

    printf("REG_DMA_EGRESS_CNTL      0x%08X\n", REG_DMA_EGRESS_CNTL);
    printf("REG_DMA_EGRESS_STS       0x%08X\n", REG_DMA_EGRESS_STS);
    printf("REG_DMA_EGRESS_CUR       0x%08X\n", REG_DMA_EGRESS_CUR);
    printf("REG_DMA_EGRESS_CUR_MSB   0x%08X\n", REG_DMA_EGRESS_CUR_MSB);
    printf("REG_DMA_EGRESS_TAIL      0x%08X\n", REG_DMA_EGRESS_TAIL);
    printf("REG_DMA_EGRESS_TAIL_MSB  0x%08X\n", REG_DMA_EGRESS_TAIL_MSB);


    if (1) {
        int i, k;
        int count = 4;
        if (hsdp->idesc.buffer) {
            for (k = 0; k < count; ++k) {
                printf("Ingress desc %d\n", k);
                for (i = 0; i < 8; ++i) {
                    printf("0x%08lX: 0x%08X\n", DMA_DESC_INGRESS(k) + i*4, REG_INGRESS_DESC(k, i * 4));
                }
            }
        }
        if (hsdp->edesc.buffer) {
            printf("\n");
            for (k = 0; k < count; ++k) {
                printf("Egress desc %d\n", k);
                for (i = 0; i < 8; ++i) {
                    printf("0x%08lX: 0x%08X\n", DMA_DESC_EGRESS(k) + i*4, REG_EGRESS_DESC(k, i * 4));
                }
            }
        }
    }
}

void hsdp_dump_desc(hsdp_dma *hsdp, size_t index, int egress) {
    hsdp_region *region = egress ? &hsdp->edesc : &hsdp->idesc;
    size_t i = 0;

    printf("%s desc %ld\n", egress ? "egress" : "ingres", index);
    for (i = 0; i < 8; ++i) {
        printf("0x%08lX: 0x%08X\n", HSDP_AXI_ADDR(*region, index) + i*4, HSDP_REG(*region, index, i * 4));
    }
}


int hsdp_poll_fast_packet(hsdp_dma *hsdp, uint32_t **buf, size_t *word_count, hsdp_packet **pkt) {
    uint64_t dma_base = hsdp->dma_base;
    uint32_t status = 0;
    size_t size = 0;
    int ie = 0;
    int iepkt = 0;
#ifdef VERBOSE
    int i = 0;
#endif

    // find a done egress
    ie = HSDP_NEXT(hsdp->edesc);
    status = REG_EGRESS_DESC(ie, REG_DESC_STS);
    //printf("Egress packet error status 0x%08X\n", status);

    if (status & 0x70000000) { // error
        printf("Egress packet error status 0x%08X\n", status);
        REG_EGRESS_DESC(ie, REG_DESC_STS) = 0;
        REG_DMA_EGRESS_TAIL64 = AXI_EGRESS_DESC(ie);
        iepkt = ie;
        REG_EGRESS_DESC64(ie, REG_DESC_BUFF) = AXI_EGRESS_PACKET(iepkt);
        hsdp->epkts.last = iepkt;
        hsdp->edesc.last = ie;
        if (word_count) {
            *word_count = 0;
        }
    } else if ((status >> 31) & 1) { // done
        size_t packet_offset = 0;
        size = (status & 0x3FFFFFF);

#ifdef VERBOSE
        packet_offset = REG_EGRESS_DESC64(ie, REG_DESC_BUFF) - AXI_EGRESS_PACKET(0);
        printf("DPC response packet %d (%u) offset 0x%08X, mem %p, pkt %p:\n", ie, (unsigned) size / 4, packet_offset, EGRESS_PACKET(0) + packet_offset, pkt);
        for (i = 0; i < size / 4; ++i) {
            printf("\t0x%08X\n", ((uint32_t *) (EGRESS_PACKET(0) + packet_offset))[i]);
        }
#endif

        if (buf) {
            packet_offset = REG_EGRESS_DESC64(ie, REG_DESC_BUFF) - AXI_EGRESS_PACKET(0);
            *buf = (uint32_t *) (EGRESS_PACKET(0) + packet_offset);
        }

        if (word_count) {
            *word_count = size >> 2;
        }

        if (pkt) {
            (*pkt)->word_count = size >> 2;
            (*pkt)->buf = EGRESS_PACKET(ie);
        }
        iepkt = ie;

        // resetup the egress desc
        REG_EGRESS_DESC64(ie, REG_DESC_BUFF) = AXI_EGRESS_PACKET(iepkt);
        REG_EGRESS_DESC(ie, REG_DESC_STS) = 0;

        dsb(st);

        // if (REG_EGRESS_DESC(ie, REG_DESC_STS) != 0) {
        //     printf("Reg status not reset: 0x%08X\n", REG_EGRESS_DESC(ie, REG_DESC_STS));
        // }

        REG_DMA_EGRESS_TAIL64 = AXI_EGRESS_DESC(ie);
        hsdp->epkts.last = iepkt;
        hsdp->edesc.last = ie;
    } else if (word_count) {
        *word_count = 0;
    }

    return 0;
}


int hsdp_receive_fast_packet(hsdp_dma *hsdp, uint32_t **buf, size_t *word_count, hsdp_packet **pkt) {
    int max_polls = 1;
    size_t count = 0;

#ifdef VERBOSE
    max_polls *= 10;
#endif

    do {
        hsdp_poll_fast_packet(hsdp, buf, &count, pkt);
        --max_polls;
    } while (max_polls > 0 && count == 0);

    // if (max_polls == 0) {
    //     printf("DPC packet timeout %d\n", HSDP_NEXT(hsdp->edesc));
    // }

    if (word_count) *word_count = count;

    return max_polls != 0;
}

int hsdp_receive_packet(hsdp_dma *hsdp, hsdp_packet **pkt) {
    uint32_t *buf = NULL;
    size_t word_count = 0;
    int rv = 0;

    rv = hsdp_poll_fast_packet(hsdp, &buf, &word_count, pkt);

    return rv;
}

int hsdp_setup_packets(hsdp_dma *hsdp, int buf_index, int num_packets, size_t word_count) {
    size_t size = word_count * 4;
    int i = 0, ii = 0;

    //INGRESS_PACKET(packet_buffer, buf_index)[1] = hsdp->seq++;

    for (i = 0; i < num_packets; ++i) {
        ii = HSDP_NEXT(hsdp->idesc);
        REG_INGRESS_DESC64(ii, REG_DESC_BUFF) = AXI_INGRESS_PACKET(buf_index+i);
        REG_INGRESS_DESC(ii, REG_DESC_STS) = 0x00000000;
        REG_INGRESS_DESC(ii, REG_DESC_CNTL) = 0x0C000000 | size;
        hsdp->idesc.last = ii;
    }
    hsdp->ipkts.last = buf_index + i;

    return 0;
}

int hsdp_send_packet(hsdp_dma *hsdp, uint32_t *buf, size_t word_count) {
    uint64_t dma_base = hsdp->dma_base;
    uint32_t status = 0;
    int rv = 0;
    int ii = 0;
    int i = 0;
    int max_polls = 1000;
    size_t size = word_count * 4;

    // if (REG_DMA_EGRESS_STS & 1) {
    //     printf("Last ingress %d\n", hsdp->last_ingress);
    //     set_last_error(hsdp, "DMA Halted.");
    //     hsdp_dump_dma(hsdp);
    //     exit(0);
    // }

    // find available ingress
    ii = HSDP_NEXT(hsdp->idesc);
    for (i = 0; i < max_polls; ++i) {
        status = REG_INGRESS_DESC(ii, REG_DESC_STS);
        if (status & 0x70000000) { // error
            printf("Ingress packet error status 0x%08X\n", status);
            break;
        } else if ((status >> 31) & 1) { // done
            break;
        }
    }

    if (i >= max_polls) {
        set_last_error(hsdp, "No available ingress descriptors");
        // hsdp_dump_dma(hsdp);
        // exit(0);
        return -1;
    }

#ifdef VERBOSE
    printf("DPC packet (%u):\n", (unsigned) size);
    for (i = 0; i < size / 4; ++i) {
        printf("\t0x%08X\n", ((uint32_t *) buf)[i]);
    }
#endif

    memcpy(INGRESS_PACKET(ii), buf, size);

    REG_INGRESS_DESC64(ii, REG_DESC_BUFF) = AXI_INGRESS_PACKET(ii);

    REG_INGRESS_DESC(ii, REG_DESC_CNTL) = 0x0C000000 | size;
    REG_INGRESS_DESC(ii, REG_DESC_STS) = 0;

    dsb(st);

    if (REG_INGRESS_DESC(ii, REG_DESC_STS) != 0) {
        printf("Reg status not reset: 0x%08X\n", REG_INGRESS_DESC(ii, REG_DESC_STS));
    }

    REG_DMA_INGRESS_TAIL64 = AXI_INGRESS_DESC(ii);

    hsdp->idesc.last = ii;

    return rv;
}

int hsdp_send_fast_packet(hsdp_dma *hsdp, int buf_index, size_t word_count) {
    uint64_t dma_base = hsdp->dma_base;
    uint32_t status;
    size_t size = word_count * 4;
    int ii;
    int i;
    int max_polls = 1000;

    // find available ingress
    ii = HSDP_NEXT(hsdp->idesc);
    for (i = 0; i < max_polls; ++i) {
        status = REG_INGRESS_DESC(ii, REG_DESC_STS);
        if (status & 0x70000000) { // error
            printf("Ingress packet error status 0x%08X\n", status);
            break;
        } else if ((status >> 31) & 1) { // done
            break;
        }
    }

    if (i >= max_polls) {
        set_last_error(hsdp, "No available ingress descriptors");
        return -1;
    }

    REG_INGRESS_DESC64(ii, REG_DESC_BUFF) = AXI_INGRESS_PACKET(buf_index);

#ifdef VERBOSE
    printf("DPC packet %d (%u) offset 0x%08X:\n", ii, (unsigned) size / 4, HSDP_AXI_ADDR(hsdp->ipkts, buf_index));
    for (i = 0; i < size / 4; ++i) {
        printf("\t0x%08X\n", ((uint32_t *) (INGRESS_PACKET(buf_index)))[i]);
    }
#endif

    REG_INGRESS_DESC(ii, REG_DESC_STS) = 0;
    REG_INGRESS_DESC(ii, REG_DESC_CNTL) = 0x0C000000 | size;

    dsb(st);

    REG_DMA_INGRESS_TAIL64 = AXI_INGRESS_DESC(ii);

    hsdp->idesc.last = ii;

    return 0;
}

void hsdp_setup_dma(hsdp_dma *hsdp) {
    uint64_t dma_base = hsdp->dma_base;
    unsigned status = 0;
    size_t count = 0;
    size_t i = 0;

    hsdp->ipkts.last = -1;
    hsdp->epkts.last = -1;
    hsdp->idesc.last = -1;
    hsdp->edesc.last = -1;

    // reset
    REG_DMA_INGRESS_CNTL = 0x00010004;
    while ((status = REG_DMA_INGRESS_CNTL) & 4) {
        //printf("Resetting DMA status 0x%08X\n", status);
    }

#ifdef VERBOSE
    printf("Done reset DMA Ingress 0x%08X, Egress 0x%08X\n", REG_DMA_INGRESS_STS, REG_DMA_EGRESS_STS);
#endif

    // setup ingress
    memset(hsdp->idesc.buffer, 0, BUF_ALIGN(hsdp->idesc.buffer_size));
    count = HSDP_COUNT(hsdp->idesc);
    for (i = 0; i < count; ++i) {
        if (i < count - 1) {
            REG_INGRESS_DESC64(i, REG_DESC_NEXT) = AXI_INGRESS_DESC(i + 1);
        } else {
            REG_INGRESS_DESC64(i, REG_DESC_NEXT) = AXI_INGRESS_DESC(0);
        }
        REG_INGRESS_DESC64(i, REG_DESC_BUFF) = AXI_INGRESS_PACKET(i);
        REG_INGRESS_DESC(i, REG_DESC_STS) = 0x80000000;
    }
    REG_DMA_INGRESS_CUR64 = AXI_INGRESS_DESC(0);
    dsb(st);

    REG_DMA_INGRESS_CNTL = 0x00014003;

    // setup egress
    count = HSDP_COUNT(hsdp->edesc);
    for (i = 0; i < count; ++i) {
        memset(HSDP_BUFFER(hsdp->edesc, i), 0, BUF_ALIGN(hsdp->edesc.packet_size));
        if (i < count - 1) {
            REG_EGRESS_DESC64(i, REG_DESC_NEXT) = AXI_EGRESS_DESC(i + 1);
        } else {
            REG_EGRESS_DESC64(i, REG_DESC_NEXT) = AXI_EGRESS_DESC(0);
        }
        REG_EGRESS_DESC64(i, REG_DESC_BUFF) = AXI_EGRESS_PACKET(i);
        REG_EGRESS_DESC(i, REG_DESC_CNTL) = 0x0C000000 | hsdp->epkts.packet_size;
        hsdp->edesc.last++;
    }
    REG_DMA_EGRESS_CUR64 = AXI_EGRESS_DESC(0);
    dsb(st);

    REG_DMA_EGRESS_CNTL = 0x00010003;

    while ((status = REG_DMA_EGRESS_STS) & 1) {
        printf("Egress starting 0x%08X\n", status);
    }
    REG_DMA_EGRESS_TAIL64 = AXI_EGRESS_DESC(count - 1);

#ifdef VERBOSE
    printf("%s: REG_DMA_EGRESS_CUR64 = 0x08X", __func__, REG_DMA_EGRESS_CUR64);
    hsdp_dump_dma(hsdp);
#endif
}

void hsdp_enable(hsdp_dma *hsdp) {
    // Enable dma
    hsdp_setup_dma(hsdp);
}

int hsdp_check(hsdp_dma *hsdp) {
    uint64_t dma_base = hsdp->dma_base;
    int err = 0;

    if ((REG_DMA_INGRESS_STS & 0x4771) != 0) {
        printf("Error REG DMA Ingress Status 0x%08X\n", REG_DMA_INGRESS_STS);
        err = -1;
    }

    if ((REG_DMA_EGRESS_STS & 0x4771) != 0) {
        printf("Error REG DMA Egress Status 0x%08X\n", REG_DMA_EGRESS_STS);
        err = -1;
    }

    if (!err) printf("HSDP Internals OK\n");

    return err;
}
