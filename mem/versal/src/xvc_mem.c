/*********************************************************************
 * Copyright (c) 2021 Xilinx, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **********************************************************************/

/*
 * xvcserver
 *
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

/* Default - Enable single word transactions. Use single word (32-bits) reads if getting a "bus error"
 * when using accesses unaligned to 64bits.
 * Comment ENABLE_SINGLE_WORD_RW definition to enable multi-word transactions.
 */
#define ENABLE_SINGLE_WORD_RW
#define DEFAULT_HUB_ADDR 0xA4000000
#define DEFAULT_HUB_SIZE 0x200000
#define BYTE_ALIGN(a) ((a + 7) / 8)
#define BUF_ALIGN(a) ((((a) + 7) / 8) * 8)
#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)

typedef struct mem_region {
    size_t addr;
    size_t size;
    unsigned char *buf;
} mem_region;

typedef struct {
    XvcClient * c;
    mem_region hub;
} xvc_mem_t;

LoggingMode log_mode = LOG_MODE_DEFAULT;

static const char * usage_text[] = {
  "Usage:\n Name      Description",
  "-------------------------------",
  "[--help]    Show help information",
  "[-s]       Socket listening port and protocol.  Default: TCP::10200",
  "[--addr]    Debug hub address.",
  "[--verbose] Show additional messages during execution",
  "[--quiet]   Disable logging all non-error messages during execution",
  "\n",
  NULL
};

static const char * time_stamp = __TIME__;
static const char * date_stamp = __DATE__;

static int open_port(void *client_data, XvcClient * c) {
    xvc_mem_t* xvc_mem = (xvc_mem_t*)client_data;
    int mem_fd = -1;
    unsigned char *mem;

    xvc_mem->c = c;

    // MMap hub address
    if ((mem_fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
        perror("Failed to open /dev/mem");
        exit(1);
    }

    mem = (unsigned char*) mmap(0, xvc_mem->hub.size, PROT_READ | PROT_WRITE,
                                MAP_SHARED | MAP_LOCKED, mem_fd, xvc_mem->hub.addr);
    if (mem == MAP_FAILED) {
        perror("Error mmapping the file");
        exit(EXIT_FAILURE);
    }

    xvc_mem->hub.buf = mem;

    close(mem_fd);
   
    if (log_mode == LOG_MODE_VERBOSE)
        fprintf(stdout, "INFO: Memory mapped 0x%08lX to %p\n", (unsigned long) xvc_mem->hub.addr,
                (void *) xvc_mem->hub.buf);
    return (0);
}

static void close_port(void *client_data) {
    xvc_mem_t* xvc_mem = (xvc_mem_t*)client_data;

    // Unmap hub
    if (xvc_mem->hub.buf && munmap((void *) xvc_mem->hub.buf, xvc_mem->hub.size)) {
        printf("Failed to unmap 0x%08lX\n", (unsigned long) xvc_mem->hub.buf);
    }
}

static void set_tck(void *client_data, unsigned long nsperiod, unsigned long *result) {
    *result = nsperiod;
}

static void shift_tms_tdi(
    void *client_data,
    unsigned long bitcount,
    unsigned char *tms_buf,
    unsigned char *tdi_buf,
    unsigned char *tdo_buf) {
    int ret = 0;

    struct timeval stop, start;

    if (log_mode == LOG_MODE_VERBOSE) {
        gettimeofday(&start, NULL);
    }

    if (log_mode == LOG_MODE_VERBOSE) {
        gettimeofday(&stop, NULL);
        fprintf(stdout, "Shift internal took %lu u-seconds with %lu bits. Return value %d\n",
                stop.tv_usec - start.tv_usec, bitcount, ret);
    }
}

static void mrd(
        void * client_data,
        unsigned flags,
        size_t addr,
        size_t num_bytes,
        unsigned char * buf) {
    xvc_mem_t* xvc_mem = (xvc_mem_t*)client_data;
    struct timeval stop, start;
    int ret = 0;

    if (log_mode == LOG_MODE_VERBOSE) {
        fprintf(stdout, "INFO: Memory read addr 0x%08lX num_bytes %lu\n", 
                (unsigned long) addr, (unsigned long) num_bytes);
    }

    if (addr < xvc_mem->hub.addr || addr + num_bytes > xvc_mem->hub.addr + xvc_mem->hub.size) {
        xvcserver_set_error(xvc_mem->c, "Invalid arguments addr 0x%08llX num_bytes %lu\n", 
                            (unsigned long long) addr, (unsigned long) num_bytes);
        return;
    }

    if (log_mode == LOG_MODE_VERBOSE) {
        gettimeofday(&start, NULL);
    }

#ifdef ENABLE_SINGLE_WORD_RW
    // Use single word reads if getting a "bus error" when using accesses unaligned to 64 bits
    size_t i;
    for (i = 0; i < num_bytes; i += 4) {
        *(uint32_t *)(buf + i) = *(volatile uint32_t *)(xvc_mem->hub.buf +
                                  (addr - xvc_mem->hub.addr + i));
    }
#else 
    memcpy(buf, xvc_mem->hub.buf + (addr - xvc_mem->hub.addr), num_bytes);
#endif

    if (log_mode == LOG_MODE_VERBOSE) {
        gettimeofday(&stop, NULL);
        fprintf(stdout, "Mrd 0x%08lX took %lu u-seconds with %lu bytes. Return value %d\n", 
            (unsigned long) addr, stop.tv_usec - start.tv_usec, (unsigned long) num_bytes, ret);
    }
}

static void mwr(
        void * client_data,
        unsigned flags,
        size_t addr,
        size_t num_bytes,
        unsigned char * buf) {
    xvc_mem_t* xvc_mem = (xvc_mem_t*)client_data;
    struct timeval stop, start;
    int ret = 0;

    if (log_mode == LOG_MODE_VERBOSE) {
        fprintf(stdout, "INFO: Memory write addr 0x%08lX num_bytes %lu\n", 
                (unsigned long) addr, (unsigned long) num_bytes);
    }

    if (addr < xvc_mem->hub.addr || addr + num_bytes > xvc_mem->hub.addr + xvc_mem->hub.size) {
        xvcserver_set_error(xvc_mem->c, "Invalid arguments addr 0x%08lX num_bytes %lu\n", 
            (unsigned long) addr, (unsigned long) num_bytes);
        return;
    }

    if (log_mode == LOG_MODE_VERBOSE) {
        gettimeofday(&start, NULL);
    }

#ifdef ENABLE_SINGLE_WORD_RW
    // Use single word writes if getting a "bus error" when using accesses unaligned to 64 bits
    size_t i;
    for (i = 0; i < num_bytes; i += 4) {
        *(volatile uint32_t *)(xvc_mem->hub.buf + (addr + i - xvc_mem->hub.addr)) = *(uint32_t *)(buf + i);
    }
#else
    memcpy(xvc_mem->hub.buf + (addr - xvc_mem->hub.addr), buf, num_bytes);
#endif

    if (log_mode == LOG_MODE_VERBOSE) {
        gettimeofday(&stop, NULL);
        fprintf(stdout, "Mwr 0x%08lX took %lu u-seconds with %lu bytes. Return value %d\n", 
            (unsigned long) addr, stop.tv_usec - start.tv_usec, (unsigned long) num_bytes, ret);
    }
}


static void display_banner() {
    fprintf(stdout, "\nDescription:\n");
    fprintf(stdout, "Xilinx xvc_mem\n");
    fprintf(stdout, "Build date : %s-%s\n", date_stamp, time_stamp);
    fprintf(stdout, "Copyright 1986-2021 Xilinx, Inc. All Rights Reserved.\n\n");
}

static inline void print_usage(void) {
    const char ** p = usage_text;
    while (*p != NULL) {
        fprintf(stdout, "%s\n", *p++);
    }
}

static void display_help(void) {
    fprintf(stdout, "\nDescription:\n");
    fprintf(stdout, "Xilinx xvc_mem\n");
    fprintf(stdout, "Build date : %s-%s\n", date_stamp, time_stamp);
    fprintf(stdout, "Copyright 1986-2021 Xilinx, Inc. All Rights Reserved.\n\n");
    fprintf(stdout, "Syntax:\nxvc_mem [-help] [-s <arg>] [-d <arg>] [-test <arg>] [-verbose] [-quiet]\n\n");
    print_usage();
}

XvcServerHandlers handlers = {
    open_port,
    close_port,
    set_tck,
    shift_tms_tdi,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    mrd,
    mwr
};

int main(int argc, char **argv) {
    const char * url = "tcp::2542";
    xvc_mem_t xvc_mem;
    int i = 1;
    int quiet = 0;
    int verbose = 0;

    xvc_mem.hub.addr = DEFAULT_HUB_ADDR;
    xvc_mem.hub.size = DEFAULT_HUB_SIZE;

    while (i < argc && argv[i][0] == '-') {
        if (strcmp(argv[i], "-s") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "option -s requires an argument\n");
                return ERROR_INVALID_ARGUMENT;
            }
            url = argv[++i];
        } else if (strcmp(argv[i], "--addr") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "option --addr requires an argument\n");
                return ERROR_INVALID_ARGUMENT;
            }
            xvc_mem.hub.addr = strtoul(argv[++i], NULL, 0);
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
      fprintf(stdout, "\nINFO: xvc_mem application started\n");
      fprintf(stdout, "INFO: Use Ctrl-C to exit xvc_mem application\n\n");
    }
    return xvcserver_start(url, &xvc_mem, &handlers, log_mode);
}