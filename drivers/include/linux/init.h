#ifndef _LINUX_INIT_H_
#define _LINUX_INIT_H_

#include <arch_barrier.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/printk.h>
#include <linux/sched.h>

#define ARRAY_SIZE	array_size

#define mb()		arch_mb()
#define rmb()		arch_rmb()
#define wmb()		arch_wmb()
#define smp_mb()	arch_smp_mb()
#define smp_rmb()	arch_smp_rmb()
#define smp_wmb()	arch_smp_wmb()

#endif /* _LINUX_INIT_H_ */
