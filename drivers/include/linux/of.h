#ifndef _LINUX_OF_H
#define _LINUX_OF_H

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <asm/byteorder.h>
#include <asm/errno.h>

#ifndef CONFIG_OF
#define CONFIG_OF
#endif

typedef u32 phandle;

#define	device_node			vmm_devtree_node
#define	property			vmm_devtree_attr
#define	MAX_PHANDLE_ARGS		VMM_MAX_PHANDLE_ARGS
#define of_device_id			vmm_devtree_nodeid
#define	of_phandle_args			vmm_devtree_phandle_args

static inline struct device_node *of_node_get(struct device_node *node)
{
	return node;
}

static inline void of_node_put(struct device_node *node)
{
	return;
}

static inline bool of_node_is_root(const struct device_node *node)
{
        return node && (node->parent == NULL);
}

static inline struct device_node *of_get_parent(const struct device_node *node)
{
        struct device_node *np;

        if (!node)
                return NULL;

        np = of_node_get(node->parent);

        return np;
}

static inline struct device_node *of_get_next_parent(struct device_node *node)
{
        struct device_node *parent;

        if (!node)
                return NULL;

        parent = of_node_get(node->parent);
        of_node_put(node);

        return parent;
}

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
	if (lenp)
		*lenp = vmm_devtree_attrlen(np, name);

	return vmm_devtree_attrval(np, name);
}

static inline struct property *of_find_property(const struct device_node *np,
					  const char *name, int *lenp)
{
	struct vmm_devtree_attr *pp;

	pp = vmm_devtree_getattr(np, name);
	if (pp && lenp)
		*lenp = pp->len;

	return pp;
}

#define of_count_phandle_with_args \
				vmm_devtree_count_phandle_with_args
#define	of_parse_phandle_with_args \
				vmm_devtree_parse_phandle_with_args
#define	of_parse_phandle_with_fixed_args \
				vmm_devtree_parse_phandle_with_fixed_args

#define	of_property_read_u8		vmm_devtree_read_u8
#define	of_property_read_u16		vmm_devtree_read_u16
#define	of_property_read_u32		vmm_devtree_read_u32

#define	of_property_read_u8_array	vmm_devtree_read_u8_array
#define	of_property_read_u16_array	vmm_devtree_read_u16_array
#define	of_property_read_u32_array	vmm_devtree_read_u32_array

#define	of_property_count_strings	vmm_devtree_count_strings
#define	of_property_read_string		vmm_devtree_read_string

#define	for_each_child_of_node(np, child)		\
		devtree_for_each_node(child, np)

#define	for_each_available_child_of_node(np, child)	\
		devtree_for_each_node(child, np)

#define	of_device_is_compatible(device, name)		\
		vmm_devtree_is_compatible(device, name)

#define of_match_node			vmm_devtree_match_node

#define of_prop_next_u32		vmm_devtree_next_u32
#define	of_prop_next_string		vmm_devtree_next_string

#define of_property_for_each_u32(np, propname, prop, p, u)	\
	for (prop = of_find_property(np, propname, NULL),	\
		p = of_prop_next_u32(prop, NULL, &u);		\
		p;						\
		p = of_prop_next_u32(prop, p, &u))

#define of_property_for_each_string(np, propname, prop, s)      \
        for (prop = of_find_property(np, propname, NULL),       \
                s = of_prop_next_string(prop, NULL);            \
                s;                                              \
                s = of_prop_next_string(prop, s))

#define of_property_match_string	vmm_devtree_match_string
#define	of_property_read_string_index	vmm_devtree_string_index
#define	of_find_node_by_phandle		vmm_devtree_find_node_by_phandle

#endif /* _LINUX_OF_H */
