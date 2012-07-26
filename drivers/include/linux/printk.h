#ifndef	__KERNEL_PRINTK__
#define	__KERNEL_PRINTK__

#include <vmm_stdio.h>

#undef BUG_ON
#define BUG_ON(x)						\
	do {							\
		if (x) {					\
			vmm_panic("Bug at %s:%d", 		\
					__FILE__, __LINE__);	\
		}						\
	} while(0)

#undef WARN_ON
#define WARN_ON(x)						\
	do {							\
		if (x) {					\
			vmm_printf("Warning at %s:%d", 		\
					__FILE__, __LINE__);	\
		}						\
	} while(0)

#define	KERN_EMERG
#define	KERN_ALERT
#define	KERN_CRIT
#define	KERN_ERR
#define	KERN_WARNING
#define	KERN_NOTICE
#define	KERN_INFO
#define	KERN_DEBUG

#define printk(args...) vmm_printf(args)

#endif
