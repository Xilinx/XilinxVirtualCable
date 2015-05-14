# XilinxVirtualCable
**Description**:  Xilinx Virtual Cable (XVC) is a TCP/IP-based protocol that 
acts like a JTAG cable and provides a means to access and debug your 
FPGA or SoC design without using a physical cable. 

This capability helps facilitate hardware debug for designs that:
* Have the FPGA in a hard-to-access location, where a "lab-PC" is not close by
* Do not have direct access to the FPGA pins – e.g. the JTAG pins are only accessible via a local processor interface
* Need to efficiently debug Xilinx FGPA or SoC systems deployed in the field to save on costly or impractical travel and reduce the time it takes to debug a remotely located system.

## Key Features & Benefits
* Ability to debug a system over an internal network, or even the internet.
* Debug via Vivado Logic Analyzer IDE exactly as if directly connected to design via standard JTAG or parallel cable
* Zynq®-7000 demonstration with Application Note and Reference Designs available in XAPP1251 - Xilinx Virtual Cable Running on Zynq-7000 Using the PetaLinux Tools
* Extensible to allow for safe, secure connections

For more information on the XVC protocol see 
* XVC 1.0 Protocol
  * [XVC 1.0 Protocol README](README_XVC_v1_0.txt) 


