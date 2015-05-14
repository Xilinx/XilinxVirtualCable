README for Xilinx Virtual Cable (XVC) v1.0 TCP/IP Protocol

OVERVIEW:

Xilinx Virtual Cable (XVC) is a TCP/IP-based communication protocol that acts 
like a JTAG cable and provides a means to access and debug your FPGA or SoC 
design without using a physical cable. In this document are the general details 
of this XVC 1.0 protocol. For source code examples of this protocol please 
visit:

https://github.com/Xilinx/XilinxVirtualCable

The XVC protocol is used by a TCP/IP client and server to transfer low level 
JTAG vectors from a high level application to a device. The client, for instance
Xilinx's EDA tools, will connect to an XVC server using standard TCP/IP 
client/server connection methods. Once connected, the client will issue an 
XVC message (getinfo:) to the server requesting the server version. After 
the client validates the protocol version, a new message is sent to the 
XVC server to set the JTAG tck rate for future shift operations. 

The shift message is the main command used between the client and the server
to transfer low level JTAG vectors. The client will issue shift operations 
to determine the JTAG chain composition and then perform various JTAG 
instructions for instance driving pins or programming a device.

In summary, XVC 1.0 protocol defines a simple JTAG communication method with 
sufficient capabilities for high level clients, like Xilinx Vivado and SDK 
tools,  to perform complex functions like programming and debug of devices. In 
this document is a basic description of the protocol. The intent of this 
document is to provide a blueprint for users of the XVC 1.0 protocol to create 
their own custom clients and servers.

PROTOCOL:

The XVC 1.0 communication protocol consists of the following three messages:

* getinfo:
* settck:<period in ns>
* shift:<num bits><tms vector><tdi vector>

For each message the client is expected to send the message and wait for a 
response from the server.  The server needs to process each message in the order
recieved and promptly provide a response. Note that for the XVC 1.0 protocol 
only one connection is assumed so as to avoid interleaving locking and 
interleaving issues that may occur with concurrent client communication.


MESSAGE: "getinfo:"

The primary use of "getinfo:" message is to get the XVC server version. The 
server version provides a client a way of determining the protocol capabilites
of the server. 

Syntax:

  Client Sends:
  "getinfo:"

  Server Returns:
    “xvcServer_v1.0:<xvc_vector_len>\n”

  Where:
    <xvc_vector_len> is the max width of the vector that can be shifted 
                     into the server

MESSAGE: "settck:"

The "settck:" message configures the server TCK period. When sending JTAG 
vectors the TCK rate may need to be varied to accomodate cable and board 
signal integrity conditions. This command is used by clients to adjust the TCK
rate in order to slow down or speed up the shifting of JTAG vectors.

Syntax:

  Client Sends:
  "settck:<set period>"

  Server Returns:
    “<current period>”

  Where:
    <set period>      is TCK period specified in ns. This value is a little-endian 
                      integer value.
    <current period>  is the value set on the server by the settck command. If 
                      the server cannot set the value then it will return the 
                      current value.


MESSAGE: "shift:"

The "shift:" message is used to shift JTAG vectors in and out of a device. 
The number of bits to shift is specified as the first shift command parameter 
followed by the TMS and TDI data vectors. The TMS and TDI vectors are 
sized according to the number of bits to shift, rouneded to the nearest byte. 
For instance if shifting in 13 bits the byte vectors will be rounded to 2 
bytes. Upon completion of the JTAG shift operation the server will return a 
byte sized vector containing the sampled target TDO value for each shifted 
TCK clock.

Syntax:

  Client Sends:
  "shift:<num bits><tms vector><tdi vector>"

  Server Returns:
    “<tdo vector>”

  Where:
    <num bits> :  is a integer in little-endian mode. This represents the number 
                  of TCK clk toggles needed to shift the vectors out
    <tms vector>: is a byte sized vector with all the TMS shift in bits Bit 0 in 
                  Byte 0 of this vector is shifted out first. The vector is 
                  num_bits and rounds up to the nearest byte.
    <tdi vector>: is a byte sized vector with all the TDI shift in bits Bit 0 in 
                  Byte 0 of this vector is shifted out first. The vector is 
                  num_bits and rounds up to the nearest byte.
    <tdo vector>: is a byte sized vector with all the TDO shift out bits Bit 0 in 
                  Byte 0 of this vector is shifted out first. The vector is 
                  num_bits and rounds up to the nearest byte.


