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

#ifndef _XVC_DRIVER_H
#define _XVC_DRIVER_H

#include "xvc_ioctl.h"
#include "xvc_user_config.h"

#ifndef _XVC_USER_CONFIG_H
#define XVC_DRIVER_NAME "xilinx_xvc_driver"
#define DEBUG_BRIDGE_COMPAT_STRING "xlnx,xvc"
// debug bridge configuration
struct db_config {
	const char* name;
	unsigned long base_addr;
	unsigned long size;
};
#endif

long xil_xvc_ioctl(unsigned char* db_ptr, const char __user* arg);
long xil_xvc_readprops(const struct db_config* db_config, const char __user* arg);

#endif /* _XVC_DRIVER_H */
