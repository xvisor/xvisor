
#ifndef _LINUX_DMA_MAPPING_H
#define _LINUX_DMA_MAPPING_H

#include <vmm_devdrv.h>
#include <libs/bitops.h>

#define dma_get_mask	vmm_dma_get_mask
#define dma_set_mask	vmm_dma_set_mask


/**
 * an enum dma_attr represents an attribute associated with a DMA
 * mapping. The semantics of each attribute should be defined in
 * Documentation/DMA-attributes.txt.
 */
enum dma_attr {
	DMA_ATTR_WRITE_BARRIER,
	DMA_ATTR_WEAK_ORDERING,
	DMA_ATTR_WRITE_COMBINE,
	DMA_ATTR_NON_CONSISTENT,
	DMA_ATTR_NO_KERNEL_MAPPING,
	DMA_ATTR_SKIP_CPU_SYNC,
	DMA_ATTR_FORCE_CONTIGUOUS,
	DMA_ATTR_MAX,
};

#define __DMA_ATTRS_LONGS BITS_TO_LONGS(DMA_ATTR_MAX)

/**
 * struct dma_attrs - an opaque container for DMA attributes
 * @flags - bitmask representing a collection of enum dma_attr
 */
struct dma_attrs {
	unsigned long flags[__DMA_ATTRS_LONGS];
};

static inline int dma_set_seg_boundary(struct device *dev, unsigned long mask)
{
	return 0;
}
static inline unsigned int dma_set_max_seg_size(struct device *dev,
						unsigned int size)
{
	return 0;
}

#define dma_alloc_coherent(d, s, h, f) dma_alloc_attrs(d, s, h, f, NULL)
#define dma_alloc_attrs(d, s, h, f, a) vmm_dma_zalloc_phy(s, h)
#define dma_free_coherent(d, s, c, h) dma_free_attrs(d, s, c, h, NULL)
#define dma_free_attrs(d, s, c, h, a) vmm_dma_free(c)

#define dma_map_single(d, a, s, r) dma_map_single_attrs(d, a, s, r, NULL)
#define dma_map_single_attrs(d, a, s, r, attrs)		\
	vmm_dma_map((virtual_addr_t *)a, s, r)

#define dma_unmap_single(d, a, s, r) dma_unmap_single_attrs(d, a, s, r, NULL)
#define dma_unmap_single_attrs(d, a, s, r, attrs)	\
	vmm_dma_unmap((virtual_addr_t *)a, s, r)

#define dma_sync_single_for_device(d, a, s, r) vmm_dma_cpu_to_dev(a, a + s, r)
#define dma_sync_single_for_cpu(d, a, s, r) vmm_dma_dev_to_cpu(a, a + s, r)

static inline int dma_mapping_error(struct device *dev, dma_addr_t dma_addr)
{
	return !dma_addr;
}

#endif
