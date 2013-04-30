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

struct device;

/* Dummy driver pm ops */
struct dev_pm_ops {
	int (*suspend)(struct device *dev);
	int (*resume)(struct device *dev);
};

/* Dummy driver structure */
struct driver {
	struct dev_pm_ops *pm;
};

/* Dummy device_type structure */
struct device_type {
};

/* Dummy device structure */
struct device {
	struct vmm_device *d;

	struct driver *driver;
	struct device_type *type;
};

/* FIXME: This file is just a place holder in most cases. */

#endif /* _LINUX_DEVICE_H */
