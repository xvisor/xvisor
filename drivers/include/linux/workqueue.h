#ifndef _LINUX_WORKQUEUE_H_
#define _LINUX_WORKQUEUE_H_

#include <vmm_macros.h>
#include <vmm_workqueue.h>

#include <linux/timer.h>
#include <linux/jiffies.h>

#define work_struct			vmm_work
#define delayed_work			vmm_delayed_work
#define workqueue_struct		vmm_workqueue

#define system_wq			NULL
#define system_long_wq			NULL
#define system_power_efficient_wq	NULL

#define queue_work(a, b)		vmm_workqueue_schedule_work(a, b)
#define schedule_work(a)		vmm_workqueue_schedule_work(system_wq, a)
#define cancel_work_sync(a)		vmm_workqueue_stop_work(a)
#define cancel_delayed_work_sync(a)	vmm_workqueue_stop_delayed_work(a)

#define create_singlethread_workqueue(name) \
		vmm_workqueue_create(name, VMM_THREAD_DEF_PRIORITY)
#define destroy_workqueue(wq)		vmm_workqueue_destroy(wq);
#define flush_workqueue(wq)		vmm_workqueue_flush(wq)

static inline int schedule_delayed_work(struct delayed_work *work, 
					unsigned long delay)
{
	u64 nsecs = delay;
	nsecs = nsecs * (1000000000 / HZ);
	return vmm_workqueue_schedule_delayed_work(NULL, work, nsecs);
}

static inline int queue_delayed_work(struct workqueue_struct *wq,
					struct delayed_work *work, 
					unsigned long delay)
{
	u64 nsecs = delay;
	nsecs = nsecs * (1000000000 / HZ);
	return vmm_workqueue_schedule_delayed_work(wq, work, nsecs);
}

static inline struct delayed_work *to_delayed_work(struct work_struct *work)
{
	return container_of(work, struct delayed_work, work);
}

#endif /* _LINUX_WORKQUEUE_H_ */
