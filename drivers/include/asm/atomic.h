#ifndef _ASM_ATOMIC_H
#define _ASM_ATOMIC_H

#include <vmm_types.h>
#include <arch_atomic.h>

#define ATOMIC_INIT(x)		ARCH_ATOMIC_INITIALIZER(x)

#define atomic_inc_return(x)	({ \
					arch_atomic_inc(x); \
					arch_atomic_read(x); \
				})

#endif /* defined(_ASM_ATOMIC_H) */
