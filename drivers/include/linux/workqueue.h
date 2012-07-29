#ifndef _LINUX_WORKQUEUE_H_
#define _LINUX_WORKQUEUE_H_

#include <vmm_workqueue.h>
#include <linux/jiffies.h>

#define work_struct			vmm_work
#define delayed_work			vmm_delayed_work
#define workqueue			vmm_workqueue

#define system_long_wq			NULL

#define queue_work(a, b)		vmm_workqueue_schedule_work(a, b)
#define cancel_work_sync(a)		vmm_workqueue_stop_work(a)
#define cancel_delayed_work_sync(a)	vmm_workqueue_stop_delayed_work(a)

static inline int schedule_delayed_work(struct delayed_work *work, 
					unsigned long delay)
{
	u64 nsecs = delay;
	nsecs = nsecs * (1000000000 / HZ);
	return vmm_workqueue_schedule_delayed_work(NULL, work, nsecs);
}

#endif /* _LINUX_WORKQUEUE_H_ */
