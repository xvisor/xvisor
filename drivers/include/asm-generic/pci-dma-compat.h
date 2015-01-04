#ifndef __LINUX_PCI_DMA_COMPAT_H
#define __LINUX_PCI_DMA_COMPAT_H

#include <vmm_types.h>
#include <vmm_heap.h>
#include <asm/io.h>

typedef physical_addr_t dma_addr_t;
struct pci_dev;

static inline void *
pci_alloc_consistent(struct pci_dev *hwdev, size_t size,
                     dma_addr_t *dma_handle)
{
	void *ptr = vmm_dma_malloc(size);
	if (!ptr) return NULL;

	*dma_handle = (dma_addr_t)virt_to_phys((virtual_addr_t)ptr);

	return ptr;
}

static inline void
pci_free_consistent(struct pci_dev *hwdev, size_t size,
                    void *vaddr, dma_addr_t dma_handle)
{
	vmm_dma_free(vaddr);
}

#endif /* __LINUX_PCI_DMA_COMPAT_H */
