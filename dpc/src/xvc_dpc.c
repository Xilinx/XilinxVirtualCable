/*
Copyright (C) 2023, Advanced Micro Devices, Inc. All rights reserved.
SPDX-License-Identifier: MIT
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <assert.h>
#include "xvcserver.h"
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include <pthread.h>
#include "hsdp_lib.h"
#include "hsdp.h"

#define BYTE_ALIGN(a) ((a + 7) / 8)
#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)

#ifndef XVCDPC_VERSION
  #define XVCDPC_VERSION "2023.2"
#endif

typedef struct mem_region {
    size_t addr;
    size_t size;
} mem_region;

typedef struct {
    XvcClient *c;
    hsdp_dma *hsdp;
    mem_region dma;
    mem_region buf;
} xvc_dpc_t;

LoggingMode log_mode = LOG_MODE_DEFAULT;

static const char * usage_text[] = {
  "Usage:\n Name      Description",
  "-------------------------------",
  "[--help]      Show help information",
  "[-s]          Socket listening port and protocol.  Default: TCP::10200",
  "[--dma_addr]  AXI DMA IP physical address.",
  "[--dma_size]  AXI DMA IP size in bytes.",
  "[--buf_addr]  Buffer physical address.",
  "[--buf_size]  Buffer size in bytes.",
  "[--verbose]   Show additional messages during execution",
  "[--quiet]     Disable logging all non-error messages during execution",
  "\n",
  NULL
};

static const char * time_stamp = __TIME__;
static const char * date_stamp = __DATE__;

static int open_port(void *client_data, XvcClient * c) {
    int ret = 0;
    xvc_dpc_t* xvc_dpc = (xvc_dpc_t*)client_data;

    xvc_dpc->c = c;

    setup_dma_region(xvc_dpc->dma.addr, xvc_dpc->dma.size);
    setup_buffer_region(xvc_dpc->buf.addr, xvc_dpc->buf.size);

    xvc_dpc->hsdp = (hsdp_dma *) hsdp_open();

    if (!xvc_dpc->hsdp) {
        perror("ERROR: hsdp_open failed\n");
        ret = ERROR_HSDP_OPEN_FAILED;
    }

    return ret;
}

static void close_port(void *client_data) {
    int ret = 0;
    xvc_dpc_t* xvc_dpc = (xvc_dpc_t*)client_data;

    ret = hsdp_close((uint64_t) xvc_dpc->hsdp);

    if (ret) {
        fprintf(stderr, "Failed to close HSDP. Return value = %d\n", ret);
    }
}

static void idpc(
        void * client_data,
        unsigned flags,
        size_t num_words,
        unsigned char * buf) {
    int ret = 0;
    xvc_dpc_t* xvc_dpc = (xvc_dpc_t*) client_data;

    if (num_words >= MAX_PACKET_SIZE ) {
        xvcserver_set_error(xvc_dpc->c, "Size of ingress packet (%lu words) greater than "
                            "allowed max packet size of %u words\n",
                             num_words, MAX_PACKET_SIZE / 4);
        return;
    }

    if (log_mode == LOG_MODE_VERBOSE) {
        fprintf(stdout, "idpc: Sending %lu words.\n", (unsigned long) num_words);
    }

    ret = hsdp_send_packet(xvc_dpc->hsdp, (uint32_t *) buf, num_words);

    if (ret) {
        xvcserver_set_error(xvc_dpc->c, "Ingress of DPC packet failed. Return value = %d\n",
            ret);
        return;
    }
}

static void edpc(
        void * client_data,
        unsigned flags,
        size_t *num_words,
        unsigned char ** buf) {
    uint32_t *packet_buf = NULL;

    xvc_dpc_t* xvc_dpc = (xvc_dpc_t*) client_data;

    hsdp_receive_fast_packet(xvc_dpc->hsdp, &packet_buf, num_words, NULL);

    if (*num_words == 0) {
        return;
    }

    *buf = (unsigned char *) packet_buf;

    if (log_mode == LOG_MODE_VERBOSE) {
        fprintf(stdout, "edpc: Received %lu words\n", (unsigned long) *num_words);
    }
}

static void display_banner() {
    fprintf(stdout, "\nDescription:\n");
    fprintf(stdout, "Xilinx xvc_dpc v%s\n", XVCDPC_VERSION);
    fprintf(stdout, "Build date : %s-%s\n", date_stamp, time_stamp);
    fprintf(stdout, "Copyright 1986-2023 Advanced Micro Devices, Inc. All Rights Reserved.\n\n");
}

static inline void print_usage(void) {
    const char ** p = usage_text;
    while (*p != NULL) {
        fprintf(stdout, "%s\n", *p++);
    }
}

static void display_help(void) {
    display_banner();
    fprintf(stdout, "Syntax:\nxvc_dpc [-help] [-s <arg>] [-d <arg>] [-test <arg>] [-verbose] [-quiet]\n\n");
    print_usage();
}

XvcServerHandlers handlers = {
    open_port,
    close_port,
    NULL,
    NULL,
    NULL,
    idpc,
    edpc
};

int main(int argc, char **argv)
{
    const char * url = "tcp::2542";
    xvc_dpc_t xvc_dpc;
    int i = 1;
    int quiet = 0;
    int verbose = 0;

    xvc_dpc.dma.size = 0;
    xvc_dpc.buf.size = 0;

    while (i < argc && argv[i][0] == '-') {
        if (strcmp(argv[i], "-s") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "option -s requires an argument\n");
                return ERROR_INVALID_ARGUMENT;
            }
            url = argv[++i];
        } else if (strcmp(argv[i], "--dma_addr") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "option --dma_addr requires an argument\n");
                return ERROR_INVALID_ARGUMENT;
            }
            xvc_dpc.dma.addr = strtoul(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--dma_size") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "option --dma_size requires an argument\n");
                return ERROR_INVALID_ARGUMENT;
            }
            xvc_dpc.dma.size = strtoul(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--buf_addr") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "option --buf_addr requires an argument\n");
                return ERROR_INVALID_ARGUMENT;
            }
            xvc_dpc.buf.addr = strtoul(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--buf_size") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "option --buf_size requires an argument\n");
                return ERROR_INVALID_ARGUMENT;
            }
            xvc_dpc.buf.size = strtoul(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
            if (quiet) {
              fprintf(stderr, "Using option -verbose along with -quiet is not supported.\n");
              return ERROR_INVALID_ARGUMENT;
            }
            log_mode = LOG_MODE_VERBOSE;
        } else if (strcmp(argv[i], "--quiet") == 0) {
            quiet = 1;
            if (verbose) {
              fprintf(stderr, "Using option -verbose along with -quiet is not supported.\n");
              return ERROR_INVALID_ARGUMENT;
            }
            log_mode = LOG_MODE_QUIET;
        } else if (strcmp(argv[i], "--help") == 0 ) {
            display_help();
            return ERROR_INVALID_ARGUMENT;
        } else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            display_help();
            return ERROR_INVALID_ARGUMENT;
        }
        i++;
    }

    if (log_mode != LOG_MODE_QUIET)
      display_banner();

    if (log_mode != LOG_MODE_QUIET) {
      fprintf(stdout, "\nINFO: xvc_dpc application started\n");
      fprintf(stdout, "INFO: Use Ctrl-C to exit xvc_dpc application\n\n");
    }
    return xvcserver_start(url, &xvc_dpc, &handlers, log_mode);
}
