#ifndef _LINUX_KTHREAD_H
#define _LINUX_KTHREAD_H

#include <linux/sched.h>

#define wake_up_process(t)		vmm_threads_wakeup(t)

#endif /* _LINUX_KTHREAD_H */
