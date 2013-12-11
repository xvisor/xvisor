#ifndef _LINUX_DEVICE_H
#define _LINUX_DEVICE_H

#include <vmm_devtree.h>
#include <vmm_devdrv.h>

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/cache.h>
#include <linux/printk.h>
#include <linux/module.h>

#define device		vmm_device
#define driver		vmm_driver
#define device_node	vmm_devtree_node

/* FIXME: This file is just a place holder in most cases. */

#endif /* _LINUX_DEVICE_H */
