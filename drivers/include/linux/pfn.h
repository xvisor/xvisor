#ifndef _LINUX_PFN_H_
#define _LINUX_PFN_H_

#include <vmm_host_aspace.h>

#ifndef __ASSEMBLY__
#include <linux/types.h>
#endif

#undef PAGE_SIZE
#define PAGE_SIZE	VMM_PAGE_SIZE

#undef PAGE_SHIFT
#define PAGE_SHIFT	VMM_PAGE_SHIFT

#undef PAGE_MASK
#define PAGE_MASK	VMM_PAGE_MASK

#define PFN_ALIGN(x)	VMM_PFN_ALIGN(x)
#define PFN_UP(x)	VMM_PFN_UP(x)
#define PFN_DOWN(x)	VMM_PFN_DOWN(x)
#define PFN_PHYS(x)	VMM_PFN_PHYS(x)

#endif
