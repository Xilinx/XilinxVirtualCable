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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <asm/io.h>
#include <linux/mod_devicetable.h>

#include "xvc_driver.h"
#include "xvc_user_config.h"

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Max Heimer <maxh@xilinx.com>");
MODULE_DESCRIPTION("XVC Debug Register Access");
MODULE_VERSION("0.1.0");

static dev_t xvc_ioc_dev_region;
static struct class* xvc_dev_class = NULL;
static struct cdev xvc_char_ioc_dev;

#ifndef _XVC_USER_CONFIG_H
#define CONFIG_COUNT 1
#define GET_DB_BY_RES 1
static struct resource *db_res = NULL;
#endif /* _XVC_USER_CONFIG_H */

static void __iomem * db_ptrs[CONFIG_COUNT];

static void xil_xvc_cleanup(void) {
	printk(KERN_INFO LOG_PREFIX "Cleaning up resources...\n");

	if (!IS_ERR(xvc_dev_class)) {
		class_destroy(xvc_dev_class);
		xvc_dev_class = NULL;
		if (xvc_char_ioc_dev.owner != NULL) {
			cdev_del(&xvc_char_ioc_dev);
		}
		unregister_chrdev_region(xvc_ioc_dev_region, CONFIG_COUNT);
	}
}

long char_ctrl_ioctl(struct file *file_p, unsigned int cmd, unsigned long arg) {
	long status = 0;
	unsigned long irqflags = 0;
	int char_index = iminor(file_p->f_path.dentry->d_inode) - MINOR(xvc_ioc_dev_region);

	spin_lock_irqsave(&file_p->f_path.dentry->d_inode->i_lock, irqflags);

	switch (cmd) {
		case XDMA_IOCXVC:
			status = xil_xvc_ioctl((unsigned char*)(db_ptrs[char_index]), (void __user *)arg);
			break;
		case XDMA_RDXVC_PROPS:
			{
#ifndef GET_DB_BY_RES
				struct db_config config_info = db_configs[char_index];
#else
				struct db_config config_info = {
					.name = NULL,
					.base_addr = db_res ? db_res->start : 0,
					.size = db_res ? resource_size(db_res) : 0,
				};
#endif
				status = xil_xvc_readprops(&config_info, (void __user*)arg);
				break;
			}
		default:
			status = -ENOIOCTLCMD;
			break;
	}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(5, 1, 0)
	mmiowb();
#endif
	spin_unlock_irqrestore(&file_p->f_path.dentry->d_inode->i_lock, irqflags);

	return status;
}

static struct file_operations xil_xvc_ioc_ops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = char_ctrl_ioctl
};

int probe(struct platform_device* pdev) {
	int status;
	int i;
	unsigned int use_index = CONFIG_COUNT > 1;
	dev_t ioc_device_number;
	char ioc_device_name[32];
	struct device* xvc_ioc_device = NULL;

	if (!xvc_dev_class) {
		xvc_dev_class = class_create(THIS_MODULE, XVC_DRIVER_NAME);
		if (IS_ERR(xvc_dev_class)) {
			xil_xvc_cleanup();
			dev_err(&pdev->dev, "unable to create class\n");
			return PTR_ERR(xvc_dev_class);
		}

		cdev_init(&xvc_char_ioc_dev, &xil_xvc_ioc_ops);
		xvc_char_ioc_dev.owner = THIS_MODULE;
		status = cdev_add(&xvc_char_ioc_dev, xvc_ioc_dev_region, CONFIG_COUNT);
		if (status != 0) {
			xil_xvc_cleanup();
			dev_err(&pdev->dev, "unable to add char device\n");
			return status;
		}
	}

	for (i = 0; i < CONFIG_COUNT; ++i) {
		if (db_ptrs[i] == NULL) {
#ifndef GET_DB_BY_RES
			const char *name = db_configs[i].name;
			unsigned long db_addr = db_configs[i].base_addr;
			unsigned long db_size = db_configs[i].size;
#else
			const char *name = NULL;
			unsigned long db_addr = 0;
			unsigned long db_size = 0;
#endif

			if (name && name[0]) {
				sprintf(ioc_device_name, "%s_%s", XVC_DRIVER_NAME, name);
			} else if (use_index) {
				sprintf(ioc_device_name, "%s_%d", XVC_DRIVER_NAME, i);
			} else {
				sprintf(ioc_device_name, "%s", XVC_DRIVER_NAME);
			}

			ioc_device_number = MKDEV(MAJOR(xvc_ioc_dev_region), MINOR(xvc_ioc_dev_region) + i);

			xvc_ioc_device = device_create(xvc_dev_class, NULL, ioc_device_number, NULL, ioc_device_name);
			if (IS_ERR(xvc_ioc_device)) {
				printk(KERN_WARNING LOG_PREFIX "Failed to create device %s", ioc_device_name);
				xil_xvc_cleanup();
				dev_err(&pdev->dev, "unable to create the device\n");
				return status;
			} else {
				printk(KERN_INFO LOG_PREFIX "Created device %s", ioc_device_name);
			}

#ifndef GET_DB_BY_RES
			db_ptrs[i] = ioremap_nocache(db_addr, db_size);
#else
			db_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
			if (db_res) {
				db_addr = db_res->start;
				db_size = resource_size(db_res);
			}
			db_ptrs[i] = devm_ioremap_resource(&pdev->dev, db_res);
#endif
			if (!db_ptrs[i] || IS_ERR(db_ptrs[i])) {
				printk(KERN_ERR LOG_PREFIX "Failed to remap debug bridge memory at offset 0x%lX, size %lu", db_addr, db_size);
				return -ENOMEM;
			} else {
				printk(KERN_INFO LOG_PREFIX "Mapped debug bridge at offset 0x%lX, size 0x%lX", db_addr, db_size);
			}
		}
	}

	return 0;
}

static int remove(struct platform_device* pdev) {
	int i;
	dev_t ioc_device_number;
	if (pdev) {
		for (i = 0; i < CONFIG_COUNT; ++i) {
			if (db_ptrs[i]) {
#ifndef GET_DB_BY_RES
				unsigned long db_addr = db_configs[i].base_addr;
				unsigned long db_size = db_configs[i].size;
#else
				unsigned long db_addr = 0;
				unsigned long db_size = 0;
				if (db_res) {
					db_addr = db_res->start;
					db_size = resource_size(db_res);
				}
#endif

				printk(KERN_INFO LOG_PREFIX "Unmapping debug bridge at offset 0x%lX, size %lu", db_addr, db_size);
#ifndef GET_DB_BY_RES
				iounmap(db_ptrs[i]);
#else
				// devm_ioremap_resource is managed by the kernel and undone on driver detach.
#endif
				db_ptrs[i] = NULL;

				ioc_device_number = MKDEV(MAJOR(xvc_ioc_dev_region), MINOR(xvc_ioc_dev_region) + i);
				device_destroy(xvc_dev_class, ioc_device_number);
				printk(KERN_INFO LOG_PREFIX "Destroyed device number %u (user config %i)", ioc_device_number, i);
			}
		}
	}

	return 0;
}

static const struct of_device_id xvc_of_ids[] = {
	{ .compatible = DEBUG_BRIDGE_COMPAT_STRING, },
	{}
};

static struct platform_driver xil_xvc_plat_driver = {
	.driver = {
		.name = XVC_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = xvc_of_ids,
	},
	.probe = probe,
	.remove = remove,
};

// --------------------------
// --------------------------
// Driver initialization code
// --------------------------
// --------------------------

static int __init xil_xvc_init(void) {
	int err = 0;

	printk(KERN_INFO LOG_PREFIX "Starting...\n");

	// Register the character packet device major and minor numbers
	err = alloc_chrdev_region(&xvc_ioc_dev_region, 0, CONFIG_COUNT, XVC_DRIVER_NAME);
	if (err != 0) {
		xil_xvc_cleanup();
		printk(KERN_ERR LOG_PREFIX "unable to get char device region\n");
		return err;
	}

	memset(db_ptrs, 0, sizeof(*db_ptrs));

	return platform_driver_register(&xil_xvc_plat_driver);
}

static void __exit xil_xvc_exit(void) {
	platform_driver_unregister(&xil_xvc_plat_driver);
	xil_xvc_cleanup();
}

module_init(xil_xvc_init);
module_exit(xil_xvc_exit);
