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

#include <linux/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#include "xvc_driver.h"

#define LENGTH_REG_OFFSET   (0)
#define TMS_REG_OFFSET      (4)
#define TDI_REG_OFFSET      (8)
#define TDO_REG_OFFSET      (12)
#define CONTROL_REG_OFFSET  (16)

static int xil_xvc_shift_bits(unsigned char* db_ptr, u32 tms_bits, u32 tdi_bits, u32 *tdo_bits) {
	int status = 0;
	u32 control_reg_data;
	u32 write_reg_data;
	int count = 100;

	// Set tms bits
	iowrite32(tms_bits, db_ptr + TMS_REG_OFFSET);

	// Set tdi bits and shift data out
	iowrite32(tdi_bits, db_ptr + TDI_REG_OFFSET);

	// Read control register
	control_reg_data = ioread32(db_ptr + CONTROL_REG_OFFSET);

	// Enable shift operation in control register
	write_reg_data = control_reg_data | 0x01;

	// Write control register
	iowrite32(write_reg_data, db_ptr + CONTROL_REG_OFFSET);

	while (count) {
		// Read control reg to check shift operation completion
		control_reg_data = ioread32(db_ptr + CONTROL_REG_OFFSET);
		if ((control_reg_data & 0x01) == 0)	{
			break;
		}
		count--;
	}
	if (count == 0)	{
		printk(KERN_ERR LOG_PREFIX "XVC transaction timed out (%0X)\n", control_reg_data);
		return -ETIMEDOUT;
	}

	// Read tdo bits back out
	*tdo_bits = ioread32(db_ptr + TDO_REG_OFFSET);

	return status;
}

long xil_xvc_ioctl(unsigned char* db_ptr, const char __user *arg) {
	struct xil_xvc_ioc xvc_obj;
	u32 operation_code;
	u32 num_bits;
	int num_bytes;
	char *tms_buf_temp = NULL;
	char *tdi_buf_temp = NULL;
	char *tdo_buf_temp = NULL;
	int current_bit;
	u32 bypass_status;
	long status = 0;

	if ((status = copy_from_user((void *)&xvc_obj, arg, sizeof(struct xil_xvc_ioc)))) {
		goto cleanup;
	}

	operation_code = xvc_obj.opcode;

	// Invalid operation type, no operation performed
	if (operation_code != 0x01 && operation_code != 0x02) {
		return 0;
	}

	num_bits = xvc_obj.length;
	num_bytes = (num_bits + 7) / 8;

	// Allocate and copy data into temporary buffers
	tms_buf_temp = (char*) kmalloc(num_bytes, GFP_KERNEL);
	if (tms_buf_temp == NULL) {
		status = -ENOMEM;
		goto cleanup;
	}
	if ((status = copy_from_user((void *)tms_buf_temp, xvc_obj.tms_buf, num_bytes))) {
		goto cleanup;
	}

	tdi_buf_temp = (char*) kmalloc(num_bytes, GFP_KERNEL);
	if (tdi_buf_temp == NULL) {
		status = -ENOMEM;
		goto cleanup;
	}
	if ((status = copy_from_user((void *)tdi_buf_temp, xvc_obj.tdi_buf, num_bytes))) {
		goto cleanup;
	}

	// Allocate TDO buffer
	tdo_buf_temp = (char*) kmalloc(num_bytes, GFP_KERNEL);
	if (tdo_buf_temp == NULL) {
		status = -ENOMEM;
		goto cleanup;
	}

	if (operation_code == 0x2) {
		bypass_status = 0x2;
	} else {
		bypass_status = 0x0;
	}

	iowrite32(bypass_status, db_ptr + CONTROL_REG_OFFSET);

	// Set length register to 32 initially if more than one word-transaction is to be done
	if (num_bits >= 32) {
		iowrite32(0x20, db_ptr + LENGTH_REG_OFFSET);
	}

	current_bit = 0;
	while (current_bit < num_bits) {
		int shift_num_bytes;
		int shift_num_bits = 32;

		u32 tms_store = 0;
		u32 tdi_store = 0;
		u32 tdo_store = 0;

		if (num_bits - current_bit < shift_num_bits) {
			shift_num_bits = num_bits - current_bit;
			// do LENGTH_REG_OFFSET here
			// Set number of bits to shift out
			iowrite32(shift_num_bits, db_ptr + LENGTH_REG_OFFSET);
		}

		// Copy only the remaining number of bytes out of user-space
		shift_num_bytes = (shift_num_bits + 7) / 8;

		memcpy(&tms_store, tms_buf_temp + (current_bit / 8), shift_num_bytes);
		memcpy(&tdi_store, tdi_buf_temp + (current_bit / 8), shift_num_bytes);

		// Shift data out and copy to output buffer
		status = xil_xvc_shift_bits(db_ptr, tms_store, tdi_store, &tdo_store);
		if (status) {
			goto cleanup;
		}

		memcpy(tdo_buf_temp + (current_bit / 8), &tdo_store, shift_num_bytes);

		current_bit += shift_num_bits;
	}

	if (copy_to_user((void *)xvc_obj.tdo_buf, tdo_buf_temp, num_bytes)) {
		status = -EFAULT;
		goto cleanup;
	}

cleanup:
	if (tms_buf_temp) kfree(tms_buf_temp);
	if (tdi_buf_temp) kfree(tdi_buf_temp);
	if (tdo_buf_temp) kfree(tdo_buf_temp);
	return status;
}

long xil_xvc_readprops(const struct db_config* db_config, const char __user* arg) {
	struct xil_xvc_properties xvc_props_obj;

	if (!db_config) {
		return -EINVAL;
	}

	xvc_props_obj.debug_bridge_base_addr = db_config->base_addr;
	xvc_props_obj.debug_bridge_size = db_config->size;
	strcpy(xvc_props_obj.debug_bridge_compat_string, DEBUG_BRIDGE_COMPAT_STRING);

	if (copy_to_user((void *)arg, &xvc_props_obj, sizeof(xvc_props_obj))) {
		return -ENOMEM;
	}

	return 0;
}
