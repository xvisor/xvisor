#ifndef	__KERNEL_PRINTK__
#define	__KERNEL_PRINTK__

#include <vmm_stdio.h>

#define	KERN_EMERG
#define	KERN_ALERT
#define	KERN_CRIT
#define	KERN_ERR
#define	KERN_WARNING
#define	KERN_NOTICE
#define	KERN_INFO
#define	KERN_DEBUG

#define printk(args...) vmm_printf(args)
#define no_printk(args...)

#if defined(DEV_DEBUG)
#define dev_dbg(dev, args...)						\
	do {								\
		vmm_lnotice((dev)->name, args);				\
	} while (0)
#else
#define dev_dbg(...)
#endif

#define dev_info(dev, args...)		do { \
					vmm_linfo((dev)->name, args); \
					} while (0)

#define dev_warn(dev, args...)		do { \
					vmm_lwarning((dev)->name, args); \
					} while (0)

#define dev_err(dev, args...)		do { \
					vmm_lerror((dev)->name, args); \
					} while (0)

#define dev_crit(dev, args...)		do { \
					vmm_lcritical((dev)->name, args); \
					} while (0)

#define dev_notice(dev, args...)	do { \
					vmm_lnotice((dev)->name, args); \
					} while (0)

#define dev_printk(level, dev, args...)	do { \
					vmm_printf("%s ", (dev)->name); \
					vmm_printf(args); \
					} while (0)

#define printk_ratelimit()			0

#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif

#define pr_emerg(fmt, ...) \
	vmm_lemergency(NULL, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_alert(fmt, ...) \
	vmm_lalert(NULL, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_crit(fmt, ...) \
	vmm_lcritical(NULL, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_err(fmt, ...) \
	vmm_lerror(NULL, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warning(fmt, ...) \
	vmm_lwarning(NULL, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warn pr_warning
#define pr_notice(fmt, ...) \
	vmm_lnotice(NULL, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_info(fmt, ...) \
	vmm_linfo(NULL, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_cont(fmt, ...) \
	vmm_printf(pr_fmt(fmt), ##__VA_ARGS__)

/* pr_devel() should produce zero code unless DEBUG is defined */
#ifdef DEBUG
#define pr_devel(fmt, ...) \
	printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#else
#define pr_devel(fmt, ...) \
	no_printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#endif

/* If you are writing a driver, please use dev_dbg instead */
#if defined(DEBUG)
#define pr_debug(fmt, ...) \
	printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#elif defined(CONFIG_DYNAMIC_DEBUG)
/* dynamic_pr_debug() uses pr_fmt() internally so we don't need it here */
#define pr_debug(fmt, ...) \
	dynamic_pr_debug(fmt, ##__VA_ARGS__)
#else
#define pr_debug(fmt, ...) \
	no_printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#endif

#define pr_err_once(msg...) \
	vmm_lerror_once(NULL, msg)

#endif
