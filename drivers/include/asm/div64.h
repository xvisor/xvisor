#ifndef _ASM_DIV64_H
#define _ASM_DIV64_H

#include <libs/mathlib.h>

#define do_div(n,base) ({                       \
            u64 __d;                            \
            u64 __rem;                          \
                                                \
            __d = do_udiv64(n, base, &__rem);      \
            (n) = __d;                          \
            __rem;                              \
        })

#endif
