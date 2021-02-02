/* This work, "xvcServer.c", is a derivative of "xvcd.c" (https://github.com/tmbinc/xvcd)
* by tmbinc, used under CC0 1.0 Universal (http://creativecommons.org/publicdomain/zero/1.0/).
* "xvcServer.c" is licensed under CC0 1.0 Universal (http://creativecommons.org/publicdomain/zero/1.0/)
* by Avnet and is used by Xilinx for XAPP1251.
*
*  Description : XAPP1251 Xilinx Virtual Cable Server for Linux
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <pthread.h>

#ifndef USE_IOCTL
#define MAP_SIZE    0x10000
#define UIO_PATH    "/dev/uio0"

typedef struct {
    uint32_t length_offset;
    uint32_t tms_offset;
    uint32_t tdi_offset;
    uint32_t tdo_offset;
    uint32_t ctrl_offset;
} jtag_t;
#else /* USE_IOCTL */
#include <sys/ioctl.h>
#include "xvc_ioctl.h"
#define CHAR_DEV_PATH   "/dev/xilinx_xvc_driver"
#endif /* !USE_IOCTL */

static int verbose = 0;

#define XVC_PORT 2542

static int sread(int fd, void *target, int len) {
    unsigned char *t = target;
    while (len) {
        int r = read(fd, t, len);
        if (r <= 0)
            return r;
        t += r;
        len -= r;
    }
    return 1;
}

#ifndef USE_IOCTL
int handle_data(int fd, volatile jtag_t* ptr) {
#else /* USE_IOCTL */
int handle_data(int fd, int fd_ioctl) {
#endif /* !USE_IOCTL */
    char xvcInfo[32];
    unsigned int bufferSize = 2048;

    sprintf(xvcInfo, "xvcServer_v1.0:%u\n", bufferSize);

    do {
        char cmd[16];
        unsigned char buffer[bufferSize], result[bufferSize / 2];
        memset(cmd, 0, 16);

        if (sread(fd, cmd, 2) != 1)
            return 1;

        if (memcmp(cmd, "ge", 2) == 0) {
            if (sread(fd, cmd, 6) != 1)
                return 1;
            memcpy(result, xvcInfo, strlen(xvcInfo));
            if (write(fd, result, strlen(xvcInfo)) != strlen(xvcInfo)) {
                perror("write");
                return 1;
            }
            if (verbose) {
                printf("%u : Received command: 'getinfo'\n", (int)time(NULL));
                printf("\t Replied with %s\n", xvcInfo);
            }
            break;
        } else if (memcmp(cmd, "se", 2) == 0) {
            if (sread(fd, cmd, 9) != 1)
                return 1;
            memcpy(result, cmd + 5, 4);
            if (write(fd, result, 4) != 4) {
                perror("write");
                return 1;
            }
            if (verbose) {
                printf("%u : Received command: 'settck'\n", (int)time(NULL));
                printf("\t Replied with '%.*s'\n\n", 4, cmd + 5);
            }
            break;
        } else if (memcmp(cmd, "sh", 2) == 0) {
            if (sread(fd, cmd, 4) != 1)
                return 1;
            if (verbose) {
                printf("%u : Received command: 'shift'\n", (int)time(NULL));
            }
        } else {
            fprintf(stderr, "invalid cmd '%s'\n", cmd);
            return 1;
        }

        int len;
        if (sread(fd, &len, 4) != 1) {
            fprintf(stderr, "reading length failed\n");
            return 1;
        }

        int nr_bytes = (len + 7) / 8;
        if (nr_bytes * 2 > sizeof(buffer)) {
            fprintf(stderr, "buffer size exceeded\n");
            return 1;
        }

        if (sread(fd, buffer, nr_bytes * 2) != 1) {
            fprintf(stderr, "reading data failed\n");
            return 1;
        }
        memset(result, 0, nr_bytes);

        if (verbose) {
            printf("\tNumber of Bits  : %d\n", len);
            printf("\tNumber of Bytes : %d \n", nr_bytes);
            printf("\n");
        }

#ifndef USE_IOCTL
        int bytesLeft = nr_bytes;
        int bitsLeft = len;
        int byteIndex = 0;
        int tdi = 0;
        int tms = 0;
        int tdo = 0;

        while (bytesLeft > 0) {
            int shift_num_bytes;
            int shift_num_bits = 32;
            tms = 0;
            tdi = 0;
            tdo = 0;

            if (bytesLeft < 4) {
                shift_num_bits = bitsLeft;
            }
            shift_num_bytes = (shift_num_bits + 7) / 8;

            memcpy(&tms, &buffer[byteIndex], shift_num_bytes);
            memcpy(&tdi, &buffer[byteIndex + nr_bytes], shift_num_bytes);

            ptr->length_offset = shift_num_bits;
            ptr->tms_offset = tms;
            ptr->tdi_offset = tdi;
            ptr->ctrl_offset = 0x01;

            /* Switch this to interrupt in next revision */
            while (ptr->ctrl_offset) {}

            tdo = ptr->tdo_offset;
            memcpy(&result[byteIndex], &tdo, shift_num_bytes);

            bytesLeft -= shift_num_bytes;
            bitsLeft -= shift_num_bits;
            byteIndex += shift_num_bytes;

            if (verbose) {
                printf("LEN : 0x%08x\n", shift_num_bits);
                printf("TMS : 0x%08x\n", tms);
                printf("TDI : 0x%08x\n", tdi);
                printf("TDO : 0x%08x\n", tdo);
            }
        }
#else /* USE_IOCTL */
        struct xil_xvc_ioc xvc_ioc;

        xvc_ioc.opcode = 0x01;
        xvc_ioc.length = len;
        xvc_ioc.tms_buf = buffer;
        xvc_ioc.tdi_buf = buffer + nr_bytes;
        xvc_ioc.tdo_buf = result;

        if (ioctl(fd_ioctl, XDMA_IOCXVC, &xvc_ioc) < 0) {
            int errsv = errno;
            fprintf(stderr, "xvc ioctl error: %s\n", strerror(errsv));
            return errsv;
        }
#endif /* !USE_IOCTL */
        if (write(fd, result, nr_bytes) != nr_bytes) {
            perror("write");
            return 1;
        }
    } while (1);

    /* Note: Need to fix JTAG state updates, until then no exit is allowed */
    return 0;
}

#ifdef USE_IOCTL
void display_driver_properties(int fd_ioctl) {
    int ret = 0;
    struct xil_xvc_properties props;

    ret = ioctl(fd_ioctl, XDMA_RDXVC_PROPS, &props);
    if (ret < 0) {
        perror("failed to read XVC driver properties");
        return;
    }

    printf("INFO: XVC driver character file: %s\n", CHAR_DEV_PATH);
    printf("INFO: debug_bridge base address: 0x%lX\n", props.debug_bridge_base_addr);
    printf("INFO: debug_bridge size: 0x%lX\n", props.debug_bridge_size);
    printf("INFO: debug_bridge device tree compatibility string: %s\n\n", props.debug_bridge_compat_string);
}
#endif /* USE_IOCTL */

int main(int argc, char **argv) {
    int i;
    int s;
    int c;
    struct sockaddr_in address;
    char hostname[256];

#ifndef USE_IOCTL
    int fd_uio;
    volatile jtag_t* ptr = NULL;

    fd_uio = open(UIO_PATH, O_RDWR);
    if (fd_uio < 1) {
        fprintf(stderr, "Failed to open uio: %s\n", UIO_PATH);
        return -1;
    }

    ptr = (volatile jtag_t*) mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_uio, 0);
    if (ptr == MAP_FAILED) {
        fprintf(stderr, "MMAP Failed\n");
        return -1;
    }
    close(fd_uio);
#else /* USE_IOCTL */
    int fd_ioctl;

    fd_ioctl = open(CHAR_DEV_PATH, O_RDWR | O_SYNC);
    if (fd_ioctl < 1) {
        fprintf(stderr, "Failed to open xvc ioctl device driver: %s\n", CHAR_DEV_PATH);
        return -1;
    }

    display_driver_properties(fd_ioctl);
#endif /* !USE_IOCTL */

    opterr = 0;

    while ((c = getopt(argc, argv, "v")) != -1) {
        switch (c) {
            case 'v':
                verbose = 1;
                break;
            case '?':
                fprintf(stderr, "usage: %s [-v]\n", *argv);
                return 1;
        }
    }

    s = socket(AF_INET, SOCK_STREAM, 0);

    if (s < 0) {
        perror("socket");
        return 1;
    }

    i = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof i);

    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(XVC_PORT);
    address.sin_family = AF_INET;

    if (bind(s, (struct sockaddr*) &address, sizeof(address)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(s, 0) < 0) {
        perror("listen");
        return 1;
    }

    if (gethostname(hostname, sizeof(hostname)) != 0) {
        perror("hostname lookup");
        close(s);
        return 1;
    }

    printf("INFO: To connect to this xvcServer instance, use url: TCP:%s:%u\n\n", hostname, XVC_PORT);

    fd_set conn;
    int maxfd = 0;

    FD_ZERO(&conn);
    FD_SET(s, &conn);

    maxfd = s;

    while (1) {
        fd_set read = conn, except = conn;
        int fd;

        if (select(maxfd + 1, &read, 0, &except, 0) < 0) {
            perror("select");
            break;
        }

        for (fd = 0; fd <= maxfd; ++fd) {
            if (FD_ISSET(fd, &read)) {
                if (fd == s) {
                    int newfd;
                    socklen_t nsize = sizeof(address);

                    newfd = accept(s, (struct sockaddr*) &address, &nsize);

                    printf("connection accepted - fd %d\n", newfd);
                    if (newfd < 0) {
                        perror("accept");
                    } else {
                        printf("setting TCP_NODELAY to 1\n");
                        int flag = 1;
                        int optResult = setsockopt(newfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
                        if (optResult < 0)
                            perror("TCP_NODELAY error");
                        if (newfd > maxfd) {
                            maxfd = newfd;
                        }
                        FD_SET(newfd, &conn);
                    }
#ifndef USE_IOCTL
                } else if (handle_data(fd, ptr)) {
#else /* USE_IOCTL */
                } else if (handle_data(fd, fd_ioctl)) {
#endif /* !USE_IOCTL */
                    printf("connection closed - fd %d\n", fd);
                    close(fd);
                    FD_CLR(fd, &conn);
                }
            } else if (FD_ISSET(fd, &except)) {
                printf("connection aborted - fd %d\n", fd);
                close(fd);
                FD_CLR(fd, &conn);
                if (fd == s)
                    break;
            }
        }
    }

#ifndef USE_IOCTL
    munmap((void *) ptr, MAP_SIZE);
#else /* USE_IOCTL */
    close(fd_ioctl);
#endif /* !USE_IOCTL */

    return 0;
}
