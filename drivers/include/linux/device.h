#ifndef _LINUX_DEVICE_H
#define _LINUX_DEVICE_H

#include <vmm_devtree.h>
#include <vmm_devdrv.h>

struct device {
	struct vmm_device *parent;

	void *priv;
};

#endif /* _LINUX_DEVICE_H */
