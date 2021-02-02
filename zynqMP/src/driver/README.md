# Overview
This is the `xilinx_xvc_driver` Linux kernel driver source code.  This driver remaps the memory of one or more debug_bridge IPs specified in the device tree into the kernel driver's memory.  Access to this debug_bridge memory is exposed through the driver's ioctl system call implementation.

By default, the driver is set up to map only the memory of the first debug_bridge IP from the device tree, i.e. `debug_bridge_0`.  The `xvc_user_config.h` header file is empty in this case and causes the driver to only handle `debug_bridge_0`.

The driver can also handle multiple debug_bridge IPs in a single design, but in this case the user must supply information about these debug_bridge configurations in `xvc_user_config.h`.  The user can copy `xvc_user_config_example.h` into `xvc_user_config.h` and modify the example debug_bridge configurations to tell the driver about multiple debug_bridges contained in the design.  Specifying multiple debug_bridge definitions in this file will result in the driver creating multiple character files when loaded (e.g. `/dev/xilinx_xvc_driver_0`, `/dev/xilinx_xvc_driver_1`, etc.).  Each character file will target the corresponding debug_bridge IP memory defined in the header file.  `xvc_user_config_example.h` contains more detailed instructions for configuring debug_bridge IPs from the PL design into the driver.

The kernel driver implements the ioctl() system call to expose debug_bridge memory access to user space.  The implemented ioctl commands are:
  * XDMA_IOCXVC - sends parameters of the shift command to the driver
  * XDMA_RDXVC_PROPS - retrieves properties of the debug_bridge targeted by the character file

See `src/user` for an XVC server implementation with examples of how to interact with this driver.

## Steps Before Compiling
  * Make sure all debug_bridge entries in the PL design's device tree have their "compatible" field matching the default value of "xlnx,xvc", or the value from `xvc_user_config.h` if the user is overriding this "compatible" field value.
    * The user can append `xvc_fragment.dts` to the end of their device tree to change the "compatible" field for `debug_bridge_0` to "xlnx,xvc".
    * In the Xilinx PetaLinux flow, the device tree source file is typically located in `project-spec/meta-user/recipes-bsp/device-tree/files/system-user.dtsi`.
  * If targeting multiple debug_bridges, create `xvc_user_config.h` from `xvc_user_config_example.h` to specify debug_bridge configurations for all debug_bridges in the design.

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
