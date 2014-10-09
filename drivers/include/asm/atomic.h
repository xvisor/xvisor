#ifndef _ASM_ATOMIC_H
#define _ASM_ATOMIC_H

#include <vmm_types.h>
#include <arch_atomic.h>

#define ATOMIC_INIT(x)		ARCH_ATOMIC_INITIALIZER(x)

#define atomic_inc(x)		arch_atomic_inc(x)
#define atomic_dec(x)		arch_atomic_dec(x)
#define atomic_set(x, y)	arch_atomic_write(x, y)
#define atomic_inc_return(x)	arch_atomic_add_return(x, 1)
#define atomic_dec_return(x)	arch_atomic_sub_return(x, 1)

#define atomic_inc_and_test(x)  (arch_atomic_add_return(x, 1) == 0)
#define atomic_dec_and_test(x)  (arch_atomic_sub_return(x, 1) == 0)
#define atomic_sub_and_test(i, x) (arch_atomic_sub_return(x, i) == 0)

#define atomic_add_negative(i, x) (arch_atomic_add_return(x, i) < 0)

#endif /* defined(_ASM_ATOMIC_H) */
