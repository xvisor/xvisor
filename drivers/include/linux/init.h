#ifndef _LINUX_INIT_H_
#define _LINUX_INIT_H_

#include <vmm_compiler.h>
#include <arch_barrier.h>

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/io.h>
#include <linux/kernel.h>

#define snprintf(p1,p2,p3...)		vmm_snprintf(p1,p2,p3)

#define mb()				arch_mb()
#define rmb()				arch_rmb()
#define wmb()				arch_wmb()
#define smp_mb()			arch_smp_mb()
#define smp_rmb()			arch_smp_rmb()
#define smp_wmb()			arch_smp_wmb()

#endif /* _LINUX_INIT_H_ */
