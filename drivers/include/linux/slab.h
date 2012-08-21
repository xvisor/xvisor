#ifndef _LINUX_SLAB_H_
#define _LINUX_SLAB_H_

#include <vmm_heap.h>

#define GFP_KERNEL		0x00000001
#define GFP_ATOMIC		0x00000002

static inline void *kmalloc(u32 size, u32 flags)
{
	return vmm_malloc(size);
}

static inline void *kzalloc(u32 size, u32 flags)
{
	return vmm_zalloc(size);
}

static inline void kfree(void *ptr)
{
	vmm_free(ptr);
}

#endif /* _LINUX_SLAB_H_ */
