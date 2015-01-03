
#ifndef _LINUX_DMA_MAPPING_H
#define _LINUX_DMA_MAPPING_H

#include <vmm_devdrv.h>

#define dma_get_mask	vmm_dma_get_mask
#define dma_set_mask	vmm_dma_set_mask

static inline int dma_set_seg_boundary(struct device *dev, unsigned long mask)
{
	return 0;
}
static inline unsigned int dma_set_max_seg_size(struct device *dev,
						unsigned int size)
{
	return 0;
}

#endif
