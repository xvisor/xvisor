#ifndef _LINUX_OF_H
#define _LINUX_OF_H

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <asm/byteorder.h>
#include <asm/errno.h>

#include <linux/device.h>

/* This is just a placeholder */

#define	of_node_get(node)	do { (void) (node); } while (0);
#define	of_node_put(node)	do { (void) (node); } while (0);

#define	for_each_available_child_of_node(np, child)	\
		devtree_for_each_node(child, np)

#define	of_device_is_compatible(device, name)		\
		vmm_devtree_is_compatible(device, name)

static inline unsigned int irq_of_parse_and_map(struct device_node *dev,
						int index)
{
	u32 irq;

	if (vmm_devtree_irq_get(dev, &irq, index))
		return 0;

	return irq;
}

static inline const void *of_get_property(const struct device_node *np,
					  const char *name, int *lenp)
{
	*lenp = vmm_devtree_attrlen(np, name);

	return vmm_devtree_attrval(np, name);
}

#endif /* _LINUX_OF_H */
