#ifndef _LINUX_KTHREAD_H
#define _LINUX_KTHREAD_H

#include <vmm_threads.h>

#define task_struct 			vmm_thread

#define wake_up_process(t)		vmm_threads_wakeup(t)

#endif /* _LINUX_KTHREAD_H */
