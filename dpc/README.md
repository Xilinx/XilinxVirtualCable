# Xilinx Virtual Cable for Debug Packet Controller (DPC) access

**Description**:  Xilinx Virtual Cable (XVC) is a TCP/IP-based protocol that 
acts like a JTAG or High-Speed Debug Port (HSDP) cable and provides a means to access and debug your 
FPGA or SoC design without using a physical cable. 

This capability helps facilitate hardware debug for designs that:
* Have the FPGA in a hard-to-access location, where a "lab-PC" is not close by
* Do not have direct access to the FPGA pins – e.g. the JTAG pins are only accessible via a local processor interface
* Need to efficiently debug Xilinx FGPA or SoC systems deployed in the field to save on costly or impractical travel and reduce the time it takes to debug a remotely located system.

## Key Features & Benefits
* Ability to debug a system over an internal network, or even the internet.
* Debug via Vivado Logic Analyzer IDE exactly as if directly connected to design via standard JTAG, HSDP or parallel cable
* Extensible to allow for safe, secure connections

# XVC 1.1 Protocol
```
XVC 1.1 Protocol 
Copyright (C) 2015-2021 Xilinx, Inc.  All rights reserved.
Copyright (C) 2023, Advanced Micro Devices, Inc. All rights reserved.

SPDX-License-Identifier: MIT
```

## Overview

Xilinx Virtual Cable (XVC) is a TCP/IP-based communication protocol that acts like a JTAG or HSDP cable and provides a means to access and debug your FPGA or SoC design without using a physical cable. In this document are the general details of this XVC 1.1 protocol.

The XVC protocol is used by a TCP/IP client and server to transfer low level JTAG vectors from a high level application to a device. The client, for instance Xilinx's EDA tools, will connect to an XVC server using standard TCP/IP client/server connection methods. Once connected, the client will issue an XVC message (getinfo:) to the server requesting the server version. After the client validates the protocol version, a new message is sent to the XVC server to set the JTAG tck rate for future shift operations. 

The shift message is the main command used between the client and the server to transfer low level JTAG vectors. The client will issue shift operations to determine the JTAG chain composition and then perform various JTAG instructions for instance driving pins or programming a device. The mrd and mwr messages can be used to read/write debug addresses. The idpc and edpc messages can be used to send/receive packets to/from the Debug Packet Controller (DPC).

In summary, XVC 1.1 protocol defines a simple JTAG/HSDP communication method with sufficient capabilities for high level clients, like Xilinx Vivado and Vitis tools, to perform complex functions like programming and debug of devices. In this document is a basic description of the protocol. The intent of this document is to provide a blueprint for users of the XVC 1.1 protocol to create their own custom clients and servers.

## Protocol

The XVC 1.1 communication protocol consists of the following messages:

```
getinfo:
capabilities:
configure:<length><configuration strings>
error:
idpc:<flags><num words><data>
edpc:<flags>
```

For each message the client is expected to send the message and wait for a response from the server.  The server needs to process each message in the order received and promptly provide a response. Note that for the XVC 1.1 protocol only one connection is assumed so as to avoid interleaving locking and interleaving issues that may occur with concurrent client communication.

### MESSAGE: "getinfo:"

The primary use of "getinfo:" message is to get the XVC server version. The server version provides a client a way of determining the protocol capabilities of the server.

**Syntax**

Client Sends:
```
"getinfo:"
```

Server Returns:
```
“xvcServer_v1.1:<xvc_vector_len>\n”
```

Where:
```
<xvc_vector_len> is the max width of the vector that can be shifted
                 into the server
```

### MESSAGE: "capabilities:"

The primary use of "capabilities:" message is to get capabilities of the XVC server.

**Syntax**

Client Sends:
```
"capabilities:"
```

Server Returns:
```
“<capability strings>"
```

### MESSAGE: "configure:"

The primary use of "configure:" message is to set configuration of the XVC server.

**Syntax**

Client Sends:
```
"configure:<length><configuration strings>"
```

Server Returns:
```
“<status>"
```

Where:
```
<length>                ULEB128 indicating length of <configuration strings> in bytes
<configuration strings> Comma separated list of strings
```

### MESSAGE: "error:"

The primary use of "error:" message is to return pending error and clear error flag.

**Syntax**

Client Sends:
```
"error:"
```

Server Returns:
```
“<length><message>"
```

Where:
```
<length>  ULEB128 length of <message> in bytes
<message> UTF-8 encoded error message
```

### MESSAGE: "idpc:"

The primary use of "idpc:" message is to send a packet to the Debug Packet Controller (DPC).

**Syntax**

Client Sends:
```
"idpc:<flags><num words><data>"
```

Server Returns:
```
“<status>"
```

Where:
```
<flags>     ULEB128 bit field for future flag use
<num words> ULEB128 number of 32-bit words in data
<data>      Binary DPC packet payload including header and CRC
```

### MESSAGE: "edpc:"

The primary use of "edpc:" message is to receive a packet from the Debug Packet Controller (DPC).

**Syntax**

Client Sends:
```
"edpc:<flags>"
```

Server Returns:
```
“<num words><data><status>"
```

Where:
```
<flags>     ULEB128 bit field for future flag use
<num words> ULEB128 number of 32-bit words being returned
<data>      Binary DPC payload data, skipped if <num words> is 0.
```

# Build Instructions

The `src` directory contains the source code for XVC for DPC access. To build XVC-DPC server:

```bash
$ source <vitis-install-directory>/.settings64-Vitis.sh # For aarch64 cross-compiler tools
$ make xvc_dpc
```

Optional arguments to `make xvc_dpc` command:

```
ENABLE_DMA_64BIT_ADDR: <1 or 0> If AXI DMA IP's address width is greater than 32-bits this
                       should be 1 else zero.
DMA_PHYS_ADDR:         <64-bit address> of AXI DMA IP in Vivado design.
```

Example with optional arguments:
```bash
$ make xvc_dpc ENABLE_DMA_64BIT_ADDR=0 DMA_PHYS_ADDR=0xA4000000
```