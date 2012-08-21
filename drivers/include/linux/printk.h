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

#undef BUG
#define BUG() BUG_ON(1)

#define	KERN_EMERG
#define	KERN_ALERT
#define	KERN_CRIT
#define	KERN_ERR
#define	KERN_WARNING
#define	KERN_NOTICE
#define	KERN_INFO
#define	KERN_DEBUG

#define printk(args...) vmm_printf(args)

#if defined(DEV_DEBUG)
#define dev_dbg(args...) vmm_printf(args)
#else
#define dev_dbg(...)
#endif

#define dev_info(dev, args...)	do { \
				vmm_printf("%s: ", dev->node->name); \
				vmm_printf(args); \
				} while (0)
#define dev_warn(dev, args...)	do { \
				vmm_printf("WARNING: %s: ", dev->node->name); \
				vmm_printf(args); \
				} while (0)
#define dev_err(dev, args...)	do { \
				vmm_printf("ERROR: %s: ", dev->node->name); \
				vmm_printf(args); \
				} while (0)

#define printk_ratelimit()			0

#endif
