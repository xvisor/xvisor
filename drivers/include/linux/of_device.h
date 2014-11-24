#ifndef _LINUX_OF_DEVICE_H
#define _LINUX_OF_DEVICE_H

#include <linux/platform_device.h>
#include <linux/of.h>

static inline int of_driver_match_device(struct device *dev,
					 struct device_driver *drv)
{
	const struct vmm_devtree_nodeid *match;

	if (!dev || !dev->node || !drv || !drv->match_table)
		return 0;

	match = vmm_devtree_match_node(drv->match_table, dev->node);

	return (match) ? 1 : 0;
}

#endif
