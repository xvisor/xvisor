#ifndef _LINUX_IO_H_
#define _LINUX_IO_H_

#include <asm/io.h>

#define __devinit

#define ioremap		(void __iomem *)vmm_host_iomap
#define ioremap_nocache (void __iomem *)vmm_host_iomap

#define virt_to_phys(virt)				\
	({						\
		physical_addr_t __pa = 0;		\
		do {					\
			vmm_host_va2pa(virt, &__pa);	\
		}while(0);				\
		(void *)(__pa);				\
	})

static inline void *phys_to_virt(physical_addr_t pa)
{
       int rc = VMM_OK;
       virtual_addr_t va = 0;

       if ((rc = vmm_host_pa2va(pa, &va))) {
               return NULL;
       }

       return (void *)va;
}

#endif /* _LINUX_IO_H_ */
