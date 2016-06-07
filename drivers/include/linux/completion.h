#ifndef __LINUX_COMPLETION_H
#define __LINUX_COMPLETION_H

/*
 * (C) Copyright 2001 Linus Torvalds
 *
 * Atomic wait-for-completion handler data structures.
 * See kernel/sched.c for details.
 */

#include <vmm_completion.h>

#include <linux/wait.h>
#include <linux/jiffies.h>

#define completion				vmm_completion

#define init_completion(cmpl)			INIT_COMPLETION(cmpl)

#define reinit_completion(cmpl)			REINIT_COMPLETION(cmpl)

static inline void complete(struct completion *x)
{
	vmm_completion_complete(x);
}

static inline int wait_for_completion(struct completion *x)
{
	return vmm_completion_wait(x);
}

/**
 * wait_for_completion_timeout: - waits for completion of a task (w/timeout)
 * @x:  holds the state of this particular completion
 * @timeout:  timeout value in jiffies
 *
 * This waits for either a completion of a specific task to be signaled or for a
 * specified timeout to expire. The timeout is in jiffies. It is not
 * interruptible.
 *
 * The return value is 0 if timed out, and positive (at least 1, or number of
 * jiffies left till timeout) if completed.
 */
static inline unsigned long wait_for_completion_timeout(struct completion *x,
							unsigned long timeout)
{
	u64 __r = timeout;
	__r = __r * (1000000000 / HZ);
	vmm_completion_wait_timeout(x, &__r);
	__r = udiv64(__r, (1000000000 / HZ));
	return (unsigned long)__r;
}

#endif
