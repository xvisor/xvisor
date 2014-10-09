#ifndef __OF_ADDRESS_H
#define __OF_ADDRESS_H

#include <linux/of.h>

/* This is just a placeholder */

static inline void __iomem *of_iomap(struct device_node *device, int index)
{
	virtual_addr_t  reg_addr;
	int ret = 0;

	if ((ret = vmm_devtree_regmap(device, &reg_addr, index)))
		return NULL;

	return (void *) reg_addr;
}

#endif
