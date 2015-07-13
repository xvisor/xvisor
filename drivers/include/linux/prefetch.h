#ifndef LINUX_PREFETCH_H
#define LINUX_PREFETCH_H

#include <arch_cache.h>

#ifndef ARCH_HAS_PREFETCH
#define prefetch(x) __builtin_prefetch(x)
#else
#define prefetch(x) arch_prefetch(x)
#endif

#endif /* !LINUX_PREFETCH_H */
