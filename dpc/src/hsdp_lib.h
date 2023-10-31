/*
Copyright (C) 2023, Advanced Micro Devices, Inc. All rights reserved.
SPDX-License-Identifier: MIT
*/

#define MAX_PACKET_SIZE 1032

typedef struct hsdp_packet {
  uint8_t *buf;                     // Buffer for packet
  uint32_t word_count;              // Number of words in packet
} hsdp_packet;

/**
  Opens an HSDP interface port with the given file path.
  Returns non-zero handle if successful
  */
uint64_t hsdp_open(void);

/**
  Closes the HSDP interface port.
  */
int hsdp_close(uint64_t handle);

/**
  Setup DMA IP's address and size
 */
void setup_dma_region(size_t addr, size_t size);

/**
  Setup DMA buffer's address and size
 */
void setup_buffer_region(size_t addr, size_t size);
