#ifndef _ASM_X86_E820_H
#define _ASM_X86_E820_H

#ifdef CONFIG_EFI
#include <linux/numa.h>
#define E820_X_MAX (E820MAX + 3 * MAX_NUMNODES)
#else	/* ! CONFIG_EFI */
#define E820_X_MAX E820MAX
#endif
#include <uapi/asm/e820.h>

#endif
