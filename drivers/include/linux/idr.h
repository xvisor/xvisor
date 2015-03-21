#ifndef _LINUX_IDR_H
#define _LINUX_IDR_H

#include <libs/radix-tree.h>

#define idr			radix_tree_root

#define DEFINE_IDR(radix)	RADIX_TREE(radix, GFP_KERNEL)

static inline int idr_alloc(struct radix_tree_root *radix, void *ptr,
			    int start, int end, gfp_t gfp_mask)
{
	unsigned long id = 0;

	/* We do not support other allocation than "GFP_KERNEL" */
	BUG_ON(GFP_KERNEL != gfp_mask);

	if (end <= 0) {
		end = INT_MAX;
	} else {
		end -= start;
	}

	id = radix_tree_next_hole(radix, start, end);
	if ((id - start >= 1) || (id > INT_MAX)) {
		return VMM_ENOSPC;
	}
	if (0 != radix_tree_insert(radix, id, ptr)) {
		return VMM_ENOMEM;
	}

	return id;
}

static inline void *idr_find(struct radix_tree_root *radix, int id)
{
	if (id < 0) {
		return NULL;
	}
	return radix_tree_lookup(radix, id);
}

static inline void idr_remove(struct radix_tree_root *radix, int id)
{
	radix_tree_delete(radix, id);
}

#endif /* _LINUX_IDR_H */
