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

#define MAP_SIZE      0x10000

typedef struct {
  uint32_t  length_offset;
  uint32_t  tms_offset;
  uint32_t  tdi_offset;
  uint32_t  tdo_offset;
  uint32_t  ctrl_offset;
} jtag_t;

static int verbose = 0;

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

int handle_data(int fd, volatile jtag_t* ptr) {

	const char xvcInfo[] = "xvcServer_v1.0:2048\n"; 

	do {
		char cmd[16];
		unsigned char buffer[2048], result[1024];
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

			int bytesLeft = nr_bytes;
			int bitsLeft = len;
			int byteIndex = 0;
			int tdi, tms, tdo;

			while (bytesLeft > 0) {
				tms = 0;
				tdi = 0;
				tdo = 0;
				if (bytesLeft >= 4) {
					memcpy(&tms, &buffer[byteIndex], 4);
					memcpy(&tdi, &buffer[byteIndex + nr_bytes], 4);

					ptr->length_offset = 32;        
					ptr->tms_offset = tms;         
					ptr->tdi_offset = tdi;       
					ptr->ctrl_offset = 0x01;

					/* Switch this to interrupt in next revision */
					while (ptr->ctrl_offset)
						{
            }

					tdo = ptr->tdo_offset;
					memcpy(&result[byteIndex], &tdo, 4);

					bytesLeft -= 4;
					bitsLeft -= 32;         
					byteIndex += 4;

					if (verbose) {
						printf("LEN : 0x%08x\n", 32);
						printf("TMS : 0x%08x\n", tms);
						printf("TDI : 0x%08x\n", tdi);
						printf("TDO : 0x%08x\n", tdo);
					}

				} else {
					memcpy(&tms, &buffer[byteIndex], bytesLeft);
					memcpy(&tdi, &buffer[byteIndex + nr_bytes], bytesLeft);
          
					ptr->length_offset = bitsLeft;        
					ptr->tms_offset = tms;         
					ptr->tdi_offset = tdi;       
					ptr->ctrl_offset = 0x01;
					/* Switch this to interrupt in next revision */
					while (ptr->ctrl_offset)
						{
            }

					tdo = ptr->tdo_offset;
          
					memcpy(&result[byteIndex], &tdo, bytesLeft);

					if (verbose) {
						printf("LEN : 0x%08x\n", 32);
						printf("TMS : 0x%08x\n", tms);
						printf("TDI : 0x%08x\n", tdi);
						printf("TDO : 0x%08x\n", tdo);
					}
					break;
				}
			}
		if (write(fd, result, nr_bytes) != nr_bytes) {
			perror("write");
			return 1;
		}

	} while (1);
	/* Note: Need to fix JTAG state updates, until then no exit is allowed */
	return 0;
}

int main(int argc, char **argv) {
   int i;
   int s;
   int c; 
   int fd_uio;
   
   struct sockaddr_in address;
   


   opterr = 0;

   while ((c = getopt(argc, argv, "v")) != -1)
      switch (c) {
      case 'v':
         verbose = 1;
         break;
      case '?':
         fprintf(stderr, "usage: %s [-v]\n", *argv);
         return 1;
      }

   fd_uio = open("/dev/uio0", O_RDWR );
   if (fd_uio < 1) {
      fprintf(stderr,"Failed to Open UIO Device\n");
      return -1;
   }

   s = socket(AF_INET, SOCK_STREAM, 0);
               
   if (s < 0) {
      perror("socket");
      return 1;
   }

   volatile jtag_t* ptr = (volatile jtag_t*) mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
					fd_uio, 0);
   if (ptr == MAP_FAILED)
				fprintf(stderr, "MMAP Failed\n");   
        
   close(fd_uio);
   
   i = 1;
   setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof i);

   address.sin_addr.s_addr = INADDR_ANY;
   address.sin_port = htons(2542);
   address.sin_family = AF_INET;

   if (bind(s, (struct sockaddr*) &address, sizeof(address)) < 0) {
      perror("bind");
      return 1;
   }

   if (listen(s, 0) < 0) {
      perror("listen");
      return 1;
   }

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

//               if (verbose)
                  printf("connection accepted - fd %d\n", newfd);
               if (newfd < 0) {
                  perror("accept");
               } else {
            	   printf("setting TCP_NODELAY to 1\n");
            	  int flag = 1;
            	  int optResult = setsockopt(newfd,
            			  	  	  	  	  	 IPPROTO_TCP,
            			  	  	  	  	  	 TCP_NODELAY,
            			  	  	  	  	  	 (char *)&flag,
            			  	  	  	  	  	 sizeof(int));
            	  if (optResult < 0)
            		  perror("TCP_NODELAY error");
                  if (newfd > maxfd) {
                     maxfd = newfd;
                  }
                  FD_SET(newfd, &conn);
               }
            }
            else if (handle_data(fd,ptr)) {

               if (verbose)
                  printf("connection closed - fd %d\n", fd);
               close(fd);
               FD_CLR(fd, &conn);
            }
         }
         else if (FD_ISSET(fd, &except)) {
            if (verbose)
               printf("connection aborted - fd %d\n", fd);
            close(fd);
            FD_CLR(fd, &conn);
            if (fd == s)
               break;
         }
      }
   }
   munmap((void *) ptr, MAP_SIZE);
   return 0;
}
