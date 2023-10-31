/*
 * Xilinx XVC Driver
 * Copyright (C) 2019 Xilinx Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _XVC_USER_CONFIG_H
#define _XVC_USER_CONFIG_H

// debug bridge configuration
struct db_config {
    const char* name;
    unsigned long base_addr;
    unsigned long size;
};

/*
 * Modify the macros and structure below with customizations
 * for the driver, if desired.
 *
 * XVC_DRIVER_NAME            - name of the driver and character files
 * DEBUG_BRIDGE_COMPAT_STRING - debug_bridge ".compatible" entry in device tree
 *
 * name      - alias to append to character file name
 * base_addr - debug_bridge base address from device tree
 * size      - debug_bridge size from device tree
 */
#define XVC_DRIVER_NAME "xilinx_xvc_driver"
#define DEBUG_BRIDGE_COMPAT_STRING "xlnx,xvc"

static const struct db_config db_configs[] = {
    /////////////////////////////////////////////////////////
    //  The single debug tree entry below with an empty 
    //  name modifier will create a character file called:
    //
    //   /dev/xilinx_xvc_driver
    //
    /////////////////////////////////////////////////////////
    // {
    //     .name = "",
    //     .base_addr = 0x80010000,
    //     .size = 0x10000,
    // },
    /////////////////////////////////////////////////////////
    //  For two debug trees in the same driver, you can 
    //  uncomment and modify the entries below.  If names are
    //  empty, only the index will be appended to the
    //  character file:
    //   /dev/xilinx_xvc_driver_0
    //   /dev/xilinx_xvc_driver_1
    //
    //  In this example, since the names are not empty, the
    //  names "tree0" and "tree1" are appended to the
    //  character files as follows:
    //
    //   /dev/xilinx_xvc_driver_tree0
    //   /dev/xilinx_xvc_driver_tree1
    //
    /////////////////////////////////////////////////////////
    //
    {
        .name = "tree0",
        .base_addr = 0x90020000,
        .size = 0x10000,
    },
    {
        .name = "tree1",
        .base_addr = 0x90030000,
        .size = 0x10000,
    },
};

#define CONFIG_COUNT (sizeof(db_configs) / sizeof(*db_configs))

#endif /* _XVC_USER_CONFIG_H */
