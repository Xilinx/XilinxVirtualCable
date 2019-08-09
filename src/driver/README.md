# Overview
This is the `xilinx_xvc_driver` Linux kernel driver source code.  This driver remaps the memory of the debug_bridge IP (or multiple IPs) specified in the `xvc_user_config.h` header file into the kernel driver's memory.  Specifying multiple debug_bridge definitions in this file will result in the driver creating multiple character files when loaded (e.g. `/dev/xilinx_xvc_driver_0`, `/dev/xilinx_xvc_driver_1`, etc.).  Each character file will target the corresponding debug_bridge IP memory defined in the header file.  `xvc_user_config.h` contains more detailed instructions for configuring debug_bridge IPs from the PL design into the driver.

The kernel driver implements the ioctl() system call to expose debug_bridge memory access to user space.  The implemented ioctl commands are:
  * XDMA_IOCXVC - sends parameters of the shift command to the driver
  * XDMA_RDXVC_PROPS - retrieves properties of the debug_bridge targeted by the character file

See `src/user` for an XVC server implementation with examples of how to interact with this driver.

## Steps Before Compiling
  * Edit `xvc_user_config.h` to specify debug_bridge configuration
  * Make sure any debug_bridge entries in the PL design's device tree have their ".compatible" field matching the value from `xvc_user_config.h`

## How to Compile
**The Makefile has a default cross-compiler of aarch64-linux-gnu-gcc and assumes this cross-compiler is available in the PATH when running make.**
Running
`make KERNEL_SRC=<path>`
or
`make KERNEL_SRC=<path> modules`
will run the default `modules` target, where `<path>` is the path to the kernel source code being built for the user's platform.

If using the Xilinx PetaLinux flow, the KERNEL_SRC directory will be under **\<TMPDIR\>/work/\<MACHINE_NAME\>-xilinx-linux/linux-xlnx/\<KERNEL_VERSION\>-xilinx-v\<PLNX_TOOL_VERSION\>+\<GIT_AUTOINC\>/linux-\<MACHINE_NAME\>-standard-build/**, e.g.:
`<TMPDIR>/work/plnx_zynqmp-xilinx-linux/linux-xlnx/4.19-xilinx-v2019.1+gitAUTOINC+9811303824-r0/linux-plnx_zynqmp-standard-build/`
\<TMPDIR\> is the PetaLinux temp directory and can be found through **petalinux-config-->Yocto-settings-->TMPDIR**.

If compiling on the target platform, run
`make KERNEL_SRC=<path> modules_install`
to run the kernel's module_install target and install the compiled driver into the `/lib/modules/<kernel_version` directory.
