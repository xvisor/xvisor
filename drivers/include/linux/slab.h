#ifndef _LINUX_SLAB_H_
#define _LINUX_SLAB_H_

#include <vmm_heap.h>

#include <linux/bug.h>

#define GFP_KERNEL		0x00000001
#define GFP_ATOMIC		0x00000002

#define kmalloc_track_caller	kmalloc

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

static inline size_t ksize(const void *ptr)
{
	return vmm_alloc_size(ptr);
}

static inline void *krealloc(const void *p, size_t new_size, u32 flags)
{
	size_t	ks;
	void *ret;

	if (!new_size) {
		kfree((void *) p);
		return NULL;
	}

	ks = ksize(p);

	if (ks >= new_size) {
		return (void *) p;
	}

	ret = vmm_malloc(new_size);

	if (ret && p)
		memcpy(ret, p, ks);

	return ret;
}

#endif /* _LINUX_SLAB_H_ */
