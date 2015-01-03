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

#endif /* _LINUX_IO_H_ */
