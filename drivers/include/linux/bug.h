#ifndef _LINUX_BUG_H
#define _LINUX_BUG_H

#include <vmm_stdio.h>

#define WARN_ON_ONCE(x)		WARN_ON(x)
#define WARN_ONCE		WARN

#define BUILD_BUG_ON(condition)	((void)sizeof(char[1 - 2*!!(condition)]))

#ifdef CONFIG_SMP
#define WARN_ON_SMP(x)                 WARN_ON(x)
#else
/*
 * Use of ({0;}) because WARN_ON_SMP(x) may be used either as
 * a stand alone line statement or as a condition in an if ()
 * statement.
 *
 * A simple "0" would cause gcc to give a "statement has no effect"
 * warning.
 */
# define WARN_ON_SMP(x)                 ({0;})
#endif

#endif /* _LINUX_BUG_H */
