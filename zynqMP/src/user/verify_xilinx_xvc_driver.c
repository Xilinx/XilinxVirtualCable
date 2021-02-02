/*
 * Copyright (c) 2019 Xilinx, Inc.  All rights reserved.
 *
 *                 XILINX CONFIDENTIAL PROPERTY
 * This   document  contains  proprietary information  which   is
 * protected by  copyright. All rights  are reserved. No  part of
 * this  document may be photocopied, reproduced or translated to
 * another  program  language  without  prior written  consent of
 * XILINX Inc., San Jose, CA. 95124
 *
 * Xilinx, Inc.
 * XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS" AS A
 * COURTESY TO YOU.  BY PROVIDING THIS DESIGN, CODE, OR INFORMATION AS
 * ONE POSSIBLE   IMPLEMENTATION OF THIS FEATURE, APPLICATION OR
 * STANDARD, XILINX IS MAKING NO REPRESENTATION THAT THIS IMPLEMENTATION
 * IS FREE FROM ANY CLAIMS OF INFRINGEMENT, AND YOU ARE RESPONSIBLE
 * FOR OBTAINING ANY RIGHTS YOU MAY REQUIRE FOR YOUR IMPLEMENTATION.
 * XILINX EXPRESSLY DISCLAIMS ANY WARRANTY WHATSOEVER WITH RESPECT TO
 * THE ADEQUACY OF THE IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO
 * ANY WARRANTIES OR REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE
 * FROM CLAIMS OF INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include "xvc_ioctl.h"

static const char char_path[] = "/dev/xilinx_xvc_driver";

#define BYTE_ALIGN(a) ((a + 7) / 8)
#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)

static unsigned findMaxBytes(unsigned *nbits_list)
{
    unsigned max_bits = 0;
    while(*nbits_list)
    {
        max_bits = MAX(max_bits, *nbits_list);
        ++nbits_list;
    }

    return BYTE_ALIGN(max_bits);
}

static void rotatePattern(unsigned char *pattern)
{
    unsigned char c;
    unsigned pat_nbytes = strlen(pattern);

    c = pattern[0];
    memmove(pattern, pattern + 1, pat_nbytes - 1);

    pattern[pat_nbytes - 1] = c;
}

static int testXVC(int fd, struct xil_xvc_ioc *xvc_ioc, unsigned cmd_nbits, unsigned char *pattern)
{
    unsigned char *tms_buf = xvc_ioc->tms_buf;
    unsigned char *tdi_buf = xvc_ioc->tdi_buf;
    unsigned char *tdo_buf = xvc_ioc->tdo_buf;
    struct timeval stop, start;
    long delta_us;
    double mbps;
    unsigned cmd_nbytes;
    unsigned pat_nbytes;
    unsigned fill_nbytes;
    unsigned vb;

    cmd_nbytes = BYTE_ALIGN(cmd_nbits);
    pat_nbytes = strlen(pattern);

    // setup tms_buf
    memset(tms_buf, 0xff, cmd_nbytes);

    // copy pattern into tdi_buf
    fill_nbytes = 0;
    while (fill_nbytes < cmd_nbytes)
    {
        unsigned nbytes = MIN(pat_nbytes, cmd_nbytes);
        memcpy(tdi_buf + fill_nbytes, pattern, nbytes);
        fill_nbytes += nbytes;
    }

    // reset tdo_buf
    memset(tdo_buf, 0, cmd_nbytes);

    // set up ioctl codes
    xvc_ioc->opcode = 0x02; // 0x01 for normal, 0x02 for bypass
    xvc_ioc->length = cmd_nbits;

    // start timer
    gettimeofday(&start, NULL);

    // run the test
    int ret = ioctl(fd, XDMA_IOCXVC, xvc_ioc);
    if (ret < 0)
    {
        printf("Could not run the command test bitlen %d\n", cmd_nbits);
        printf("Error: %s\n", strerror(errno));
        return -1;
    }

    // stop timer
    gettimeofday(&stop, NULL);

    if (stop.tv_usec < start.tv_usec)
        delta_us = 1000000 - start.tv_usec + stop.tv_usec;
    else
        delta_us = stop.tv_usec - start.tv_usec;
    delta_us += 1000000 * (stop.tv_sec - start.tv_sec);

    mbps = (double) cmd_nbits / (double)(delta_us);

    // verify tdo
    while (vb < cmd_nbits)
    {
        unsigned nbits = MIN(cmd_nbits - vb, 8);
        unsigned mask = 0xFF;
        unsigned index = vb / 8;

        mask >>= (8 - nbits);
        if ((tdi_buf[index] & mask) != (tdo_buf[index] & mask))
        {
            printf("Test Length: %d, pattern %s FAILURE\n", cmd_nbits, pattern);
            printf("\tByte %d did not match (0x%02X != 0x%02X mask 0x%02X), pattern %s\n", 
                index, tdi_buf[index] & mask, tdo_buf[index] & mask, mask, pattern);
            return -1;
        }
        vb += nbits;
    }

    printf("Test Length: %d, %ld us, %.2f Mbps SUCCESS\n", cmd_nbits, delta_us, mbps);

    return 0;
}

int main(int argc, char **argv)
{
    int fd;
    struct xil_xvc_ioc xvc_ioc;
    unsigned max_nbytes;
    unsigned char pattern[] = "abcdefgHIJKLMOP";

    // unsigned test_lens[] = {1, 4, 6, 12, 24, 32, 64, 89, 144, 233, 
    //  377, 610, 987, 1597, 2584, 4096, 0x2000, 0x800000, 0};
    unsigned test_lens[] = {32, 0};
    unsigned test_index = 0;

    // try opening the driver
    fd = open(char_path, O_RDWR | O_SYNC);
    if (fd <= 0)
    {
        printf("Could not open driver at %s\n", char_path);
        printf("Error: %s\n", strerror(errno));
        return 0;
    }

    max_nbytes = findMaxBytes(test_lens);

    // set up buffers with the maximum size to be tested
    xvc_ioc.tms_buf = (unsigned char *) malloc(max_nbytes);
    xvc_ioc.tdi_buf = (unsigned char *) malloc(max_nbytes);
    xvc_ioc.tdo_buf = (unsigned char *) malloc(max_nbytes);
    if (!xvc_ioc.tms_buf || !xvc_ioc.tdi_buf || !xvc_ioc.tdo_buf)
    {
        printf("Could not allocate %d bytes for buffers\n", max_nbytes);
        printf("Error: %s\n", strerror(errno));
        return 0;
    }

    // run tests
    while (test_lens[test_index])
    {
        if (testXVC(fd, &xvc_ioc, test_lens[test_index], pattern) < 0)
            return 0;
        ++test_index;
        rotatePattern(pattern);
    }

    // if we get this far we must be good
    printf("XVC Driver Verified Successfully!\n");

    return 0;
}
