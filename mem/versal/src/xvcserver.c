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

#define _CRT_SECURE_NO_WARNINGS 1

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <malloc.h>

#ifdef _WIN32
#undef UNICODE
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#if defined(_MSC_VER)
typedef unsigned __int8 uint8_t;
#else
#include <inttypes.h>
#endif

#pragma comment(lib, "Ws2_32.lib")

#define snprintf _snprintf
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>

#include <sys/time.h>
#endif

#include "xvcserver.h"

#define MAX_PACKET_LEN 10000

#define tostr2(X) #X
#define tostr(X) tostr2(X)

//#define LOG_PACKET
#ifndef XVC_VERSION
#define XVC_VERSION 11
#endif

#ifndef XVC_MEM
#define XVC_MEM 1
#endif

static unsigned max_packet_len = MAX_PACKET_LEN;

struct XvcClient {
    unsigned buf_len;
    unsigned buf_max;
    uint8_t * buf;
    int fd;
    XvcServerHandlers *handlers;
    void *client_data;
    int locked;
    int enable_locking;
    int enable_status;
    char pending_error[1024];
};

static XvcClient xvc_client;

static unsigned char *reply_buf = NULL;
static unsigned reply_max = 0;
static unsigned reply_len;

static void reply_buf_size(unsigned bytes) {
    if (reply_max < bytes) {
        if (reply_max == 0) reply_max = 1;
        while (reply_max < bytes) reply_max *= 2;
        reply_buf = (unsigned char *)realloc(reply_buf, reply_max);
    }
}

static char *get_field(char **sp, int c) {
    char *field = *sp;
    char *s = field;
    while (*s != '\0' && *s != c)
        s++;
    if (*s != '\0')
        *s++ = '\0';
    *sp = s;
    return field;
}

#ifndef _WIN32
static int closesocket(int sock)
{
    return close(sock);
}
#endif

static int open_server(const char * host, const char * port) {
    int err = 0;
    int sock = -1;
    struct addrinfo hints;
    struct addrinfo * reslist = NULL;
    struct addrinfo * res = NULL;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    if (*host == '\0') host = NULL;
    err = getaddrinfo(host, port, &hints, &reslist);
    if (err) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        errno = EINVAL;
        return -1;
    }

    for (res = reslist; res != NULL; res = res->ai_next) {
        const int i = 1;
        sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sock < 0) continue;

        err = 0;
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&i, sizeof(i)) < 0) err = 1;
        if (!err && bind(sock, res->ai_addr, res->ai_addrlen)) err = 1;
        if (!err && listen(sock, 4)) err = 1;
        if (!err) break;

        closesocket(sock);
        sock = -1;
    }
    freeaddrinfo(reslist);
    return sock;
}

static unsigned get_uint_le(void * buf, int len) {
    unsigned char * p = (unsigned char *)buf;
    unsigned value = 0;

    while (len-- > 0) {
        value = (value << 8) | p[len];
    }
    return value;
}

static void set_uint_le(void * buf, int len, unsigned value) {
    unsigned char * p = (unsigned char *)buf;

    while (len-- > 0) {
        *p++ = (unsigned char)value;
        value >>= 8;
    }
}

#if XVC_VERSION >= 11
static uint64_t get_uleb128(unsigned char** buf, void *bufend) {
    unsigned char * p = (unsigned char *)*buf;
    uint64_t value = 0;
    size_t i = 0;
    size_t n;
    do {
        n = p < (unsigned char *)bufend ? *p++ : (p++, 0);
        value |= (n & 0x7fL) << i;
        i += 7;
    } while ((n & 0x80L) != 0);
    *buf = p;
    return value;
}

static void reply_status(XvcClient * c) {
    if (reply_len < max_packet_len)
        reply_buf[reply_len] = (c->pending_error[0] != '\0');
    reply_len++;
}

static void reply_uleb128(uint64_t value) {
    unsigned pos = 0;
    do {
        if (reply_len + pos < max_packet_len) {
            if (value >= 0x80) {
                reply_buf[reply_len + pos] = (value & 0x7f) | 0x80;
            } else {
                reply_buf[reply_len + pos] = value & 0x7f;
            }
        }
        value >>= 7;
        pos++;
    } while (value);
    reply_len += pos;
}

void xvcserver_set_error(XvcClient * c, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(c->pending_error, sizeof c->pending_error, fmt, ap);
    c->pending_error[sizeof c->pending_error - 1] = '\0';
    va_end(ap);
}
#endif

static int send_packet(XvcClient * c, const void * buf, unsigned len) {
    int rval = send(c->fd, buf, len, 0);
    return rval;
}

static void consume_packet(XvcClient * c, unsigned len) {
    assert(len <= c->buf_len);
    c->buf_len -= len;
    memmove(c->buf, c->buf + len, c->buf_len);
}

#ifdef LOG_PACKET
static void dumphex(
    void *buf, size_t len)
{
    unsigned char *p = (unsigned char *)buf;
    size_t i;

    if (len == 0)
        return;
    for (i = 0; i < len; i++) {
        int c = p[i];
        if (c >= 32 && c <= 127) {
            printf("%c", c);
        } else if (c == 0) {
            printf("\\0");
        } else {
            printf("\\x%02x", c);
        }
    }
}
#endif

static void read_packet(XvcClient * c) {
    unsigned char * cbuf;
    unsigned char * cend;
    unsigned fill;

    reply_buf_size(max_packet_len);

    struct timeval stop, start;

read_more:
#ifdef LOG_PACKET
    printf("read_packet ");
    dumphex(c->buf, c->buf_len);
    printf("\n");
#endif
    cbuf = c->buf;
    cend = cbuf + c->buf_len;
    fill = 0;
    reply_len = 0;
    for (;;) {
        unsigned char * p = cbuf;
        unsigned char * e = p + 30 < cend ? p + 30 : cend;
        unsigned len;

        while (p < e && *p != ':') {
            p++;
        }
        if (p >= e) {
            if (p - cbuf >= 30) {
                fprintf(stderr, "protocol error: received %.30s\n", cbuf);
                goto error;
            }
            fill = 1;
            break;
        }
        p++;
        len = p - cbuf;

        if (len == 8 && memcmp(cbuf, "getinfo:", len) == 0) {
            snprintf((char *)reply_buf + reply_len, 100, "xvcServer_v%u.%u:%u\n",
                     XVC_VERSION / 10, XVC_VERSION % 10, c->buf_max);
            reply_len += strlen((char *)reply_buf + reply_len);
            goto reply;
        }

#if XVC_VERSION >= 11
        if (len == 13 && memcmp(cbuf, "capabilities:", len) == 0) {
            unsigned bytes;
            char capabilities[100];
            capabilities[0] = '\0';
            if (c->handlers->lock && c->handlers->unlock)
                strcat(capabilities, "locking,");
            if (c->handlers->register_shift && c->handlers->state)
                strcat(capabilities, "state-aware,");
#if XVC_MEM
            strcat(capabilities, "memory,");
            // idcode to identify versal_debug_bridge
            strcat(capabilities, "idcode=2315268243,");
#endif
            strcat(capabilities, "status");
            bytes = strlen(capabilities);
            reply_uleb128(bytes);
            memcpy(reply_buf + reply_len, capabilities, bytes);
            reply_len += bytes;
            goto reply;
        }

        if (len == 10 && memcmp(cbuf, "configure:", len) == 0) {
            unsigned bytes = get_uleb128(&p, cend);
            unsigned char * pktend = p + bytes;
            char * s = (char *)p;
            int oldc;

            if (cend < pktend) {
                assert(pktend - cbuf < c->buf_max);
                fill = 1;
                break;
            }

            oldc = *pktend;
            *pktend = '\0';
            while (*s != '\0' && !c->pending_error[0]) {
                char * config = get_field(&s, ',');
                char * assign = strchr(config, '=');
                int enable = -1;
                if (assign) {
                    *assign++ = '\0';
                } else {
                    unsigned config_len = strlen(config);
                    if (config_len > 1) {
                        if (config[config_len - 1] == '+') {
                            enable = 1;
                        } else if (config[config_len - 1] == '-') {
                            enable = 0;
                        }
                        config[config_len - 1] = '\0';
                    }
                }
                if (strcmp(config, "locking") == 0 && c->handlers->lock && c->handlers->unlock) {
                    if (enable < 0) {
                        xvcserver_set_error(c, "configuration \"locking\" requires boolean + or -");
                        break;
                    }
                    c->enable_locking = enable;
                } else if (strcmp(config, "status") == 0) {
                    if (enable < 0) {
                        xvcserver_set_error(c, "configuration \"status\" requires boolean + or -");
                        break;
                    }
                    c->enable_status = enable;
                } else {
                    xvcserver_set_error(c, "unexpected configuration: %s", config);
                    break;
                }
            }
            *pktend = oldc;
            p = pktend;
            goto reply_with_status;
        }

        if (len == 6 && memcmp(cbuf, "error:", len) == 0) {
            unsigned bytes = strlen(c->pending_error);
            if (bytes > c->buf_max - (bytes + 127)/128)
                bytes = c->buf_max - (bytes + 127)/128;
            reply_uleb128(bytes);
            memcpy(reply_buf + reply_len, c->pending_error, bytes);
            reply_len += bytes;
            c->pending_error[0] = '\0';
            goto reply;
        }

        if (len == 5 && memcmp(cbuf, "lock:", len) == 0) {
            unsigned timeout = get_uleb128(&p, cend);
            if (cend < p) {
                assert(p - cbuf <= c->buf_max);
                fill = 1;
                break;
            }
            if (reply_len > 0) break;
            if (!c->pending_error[0]) {
                if (!c->enable_locking) {
                    xvcserver_set_error(c, "locking is disabled");
                } else if (c->locked) {
                    xvcserver_set_error(c, "already locked");
                } else {
                    c->handlers->lock(c->client_data, timeout);
                    if (!c->pending_error[0])
                        c->locked = 1;
                }
            }
            goto reply_with_status;
        }

        if (len == 7 && memcmp(cbuf, "unlock:", len) == 0) {
            if (!c->pending_error[0]) {
                if (!c->enable_locking) {
                    xvcserver_set_error(c, "locking is disabled");
                } else if (!c->locked) {
                    xvcserver_set_error(c, "already unlocked");
                } else {
                    c->handlers->unlock(c->client_data);
                    if (!c->pending_error[0])
                        c->locked = 0;
                }
            }
            goto reply_with_status;
        }
#endif // XVC_VERSION


        if (len == 6 && memcmp(cbuf, "shift:", len) == 0) {

            gettimeofday(&start, NULL);

            unsigned bits;
            unsigned bytes;

            if (cend < p + 4) {
                fill = 1;
                break;
            }
            bits = get_uint_le(p, 4);
            bytes = (bits + 7) / 8;
            if (cend < p + 4 + bytes * 2) {
                assert(p + 4 + bytes * 2 - cbuf <= c->buf_max);
                fill = 1;
                break;
            }
            p += 4;

            if (!c->pending_error[0]) {
                c->handlers->shift_tms_tdi(c->client_data, bits, p, p + bytes, reply_buf + reply_len);
            }
            if (c->pending_error[0]) {
                memset(reply_buf + reply_len, 0, bytes);
            }
            reply_len += bytes;
            p += bytes * 2;

            gettimeofday(&stop, NULL);

            goto reply_with_optional_status;
        }

        if (len == 7 && memcmp(cbuf, "settck:", len) == 0) {
            unsigned long nsperiod;
            unsigned long resnsperiod;

            if (cend < p + 4) {
                fill = 1;
                break;
            }
            nsperiod = get_uint_le(p, 4);
            p += 4;

            if (!c->pending_error[0])
                c->handlers->set_tck(c->client_data, nsperiod, &resnsperiod);
            if (c->pending_error[0])
                resnsperiod = nsperiod;

            set_uint_le(reply_buf + reply_len, 4, resnsperiod);
            reply_len += 4;
            goto reply_with_optional_status;
        }

#if XVC_VERSION >= 11
        if (len == 8 &&
                 (cbuf[0] == 'i' || cbuf[0] == 'd') &&
                 memcmp(cbuf + 1, "rshift:", len - 1) == 0 &&
                 c->handlers->register_shift) {
            unsigned int flags = get_uleb128(&p, cend);
            unsigned int state = get_uleb128(&p, cend);
            unsigned long count = get_uleb128(&p, cend);
            unsigned long tdibytes = (flags & 3) == 0 ? (count + 7) / 8 : 0;
            unsigned long tdobytes = (flags & 4) != 0 ? (count + 7) / 8 : 0;
            if (cend < p + tdibytes) {
                assert(p + tdibytes - cbuf <= c->buf_max);
                fill = 1;
                break;
            }
            if (!c->pending_error[0])
                c->handlers->register_shift(
                    c->client_data, (cbuf[0] == 'i'), flags, state,
                    count, tdibytes ? p : NULL, tdobytes ? reply_buf + reply_len : NULL);
            if (c->pending_error[0])
                memset(reply_buf + reply_len, 0, tdobytes);
            reply_len += tdobytes;
            p += tdibytes;
            goto reply_with_status;
        }

        if (len == 6 && memcmp(cbuf, "state:", len) == 0 && c->handlers->state) {
            unsigned int flags = get_uleb128(&p, cend);
            unsigned int state = get_uleb128(&p, cend);
            unsigned long count = get_uleb128(&p, cend);
            if (cend < p) {
                assert(p - cbuf <= c->buf_max);
                fill = 1;
                break;
            }
            if (!c->pending_error[0])
                c->handlers->state(c->client_data, flags, state, count);
            goto reply_with_status;
        }

#if XVC_MEM
        if (len == 4 && memcmp(cbuf, "mrd:", len) == 0 && c->handlers->mrd) {
            unsigned int flags = get_uleb128(&p, cend);
            size_t        addr = get_uleb128(&p, cend);
            size_t   num_bytes = get_uleb128(&p, cend);
            if (cend < p) {
                assert(p - cbuf <= c->buf_max);
                fill = 1;
                break;
            }

            if (!c->pending_error[0])
                c->handlers->mrd(c->client_data, flags, addr, num_bytes, reply_buf + reply_len);

            if (c->pending_error[0])
                memset(reply_buf + reply_len, 0, num_bytes);
            reply_len += num_bytes;
            goto reply_with_status;
        }

        if (len == 4 && memcmp(cbuf, "mwr:", len) == 0 && c->handlers->mwr) {
            unsigned int flags = get_uleb128(&p, cend);
            size_t        addr = get_uleb128(&p, cend);
            size_t   num_bytes = get_uleb128(&p, cend);
            if (cend < p + num_bytes) {
                assert(p + num_bytes - cbuf <= c->buf_max);
                fill = 1;
                break;
            }

            if (!c->pending_error[0])
                c->handlers->mwr(c->client_data, flags, addr, num_bytes, p);

            p += num_bytes;
            goto reply_with_status;
        }
#endif // XVC_MEM
#endif

        fprintf(stderr, "protocol error: received %.*s\n", (int)len, cbuf);
        goto error;

    reply_with_optional_status:
        if (!c->enable_status) goto reply;
#if XVC_VERSION >= 11
    reply_with_status:
        reply_status(c);
#endif
    reply:
        cbuf = p;
    }

    if (c->buf < cbuf) {
        if (c->handlers->flush)
            if (c->handlers->flush(c->client_data) < 0) goto error;
#ifdef LOG_PACKET
        printf("send_packet ");
        dumphex(reply_buf, reply_len);
        printf("\n");
#endif
        if (send_packet(c, reply_buf, reply_len) < 0) goto error;
        consume_packet(c, cbuf - c->buf);
        
        gettimeofday(&stop, NULL);
        
        if (c->buf_len && !fill) goto read_more;
    }

    {
        int len = recv(c->fd, c->buf + c->buf_len, c->buf_max - c->buf_len, 0);
        if (len > 0) {
            c->buf_len += len;
            goto read_more;
        }
        if (len < 0) goto error;
    }
    return;

error:
    fprintf(stderr, "XVC connection terminated: error %d\n", errno);
}

int xvcserver_start(
    const char * url,
    void * client_data,
    XvcServerHandlers * handlers,
    LoggingMode log_mode)
{
    XvcClient * c = &xvc_client;
    int sock;
    int fd;
    char * url_copy = strdup(url);
    char * p = url_copy;
    const char * transport;
    const char * host;
    const char * port;
    struct sockaddr_in client_addr;
    socklen_t addr_len = 0 ;
    char tmpname[1024];
    int ret = 0;

    transport = get_field(&p, ':');
    if ((transport[0] == 'T' || transport[0] == 't') &&
        (transport[1] == 'C' || transport[1] == 'c') &&
        (transport[2] == 'P' || transport[2] == 'p') &&
        transport[3] == '\0') {
        host = get_field(&p, ':');
    } else if (strchr(p, ':') == NULL) {
        host = transport;
        transport = "tcp";
    } else {
        fprintf(stderr, "ERROR: Invalid transport type: %s\n", transport);
        ret = ERROR_INVALID_URL_TRANSPORT_TYPE;
        goto cleanup;
    }
    port = get_field(&p, ':');
    if (*p != '\0') {
        fprintf(stderr, "ERROR: Unexpected url field: %s\n", p);
        ret = ERROR_INVALID_URL_FIELD;
        goto cleanup;
    }

#ifdef _WIN32
    {
        WSADATA wsaData;
        int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (err != 0) {
            printf("WSAStartup failed: %d\n", err);
            ret = 1;
            goto cleanup;
        }
    }
#endif

    sock = open_server(host, port);
    if (sock < 0) {
        perror("ERROR: Failed to create socket");
        ret = ERROR_SOCKET_CREATION;
        goto cleanup;
    } else {
        if (host[0] == '\0') {
            if (gethostname(tmpname, sizeof(tmpname)) != 0) {
                ret = ERROR_GETHOSTNAME_FAILED;
                closesocket(sock);
                goto cleanup;
            }
            host = tmpname;
        }
        if (log_mode != LOG_MODE_QUIET)
            fprintf(stdout, "INFO: To connect to this xvc_mem instance use url: %s:%s:%s\n\n",
                    transport, host, port);
    }

    while ((fd = accept(sock, NULL, NULL)) >= 0) {
        int opt = 1;
        int client_port;
        char *client_ip;

        if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&opt, sizeof(opt)) < 0)
            fprintf(stderr, "setsockopt TCP_NODELAY failed\n");

        // Get client address
        addr_len = sizeof(client_addr);
        if (getpeername(fd, (struct sockaddr *)&client_addr, &addr_len) < 0) {
            fprintf(stderr, "ERROR: getpeername failed. Returned error - %s\n", strerror(errno));
            continue;
        }
        client_ip = inet_ntoa(client_addr.sin_addr);
        client_port = htons(client_addr.sin_port);

        if (log_mode != LOG_MODE_QUIET)
            fprintf(stdout, "INFO: xvcserver accepted connection from client %s:%d \n",
                    client_ip, client_port);

        memset(c, 0, sizeof *c);
        c->fd = fd;
        c->handlers = handlers;
        c->client_data = client_data;
        c->buf_max = max_packet_len;
        c->buf = (uint8_t *)malloc(c->buf_max);

        if (handlers->open_port(client_data, c) < 0) {
            fprintf(stderr, "Opening JTAG port failed\n");
            closesocket(fd);
            free(c->buf);
            continue;
        }

        read_packet(c);
        handlers->close_port(client_data);
        closesocket(fd);
        free(c->buf);
    }
    closesocket(sock);
 
cleanup:
    free(url_copy);
    return ret;
}