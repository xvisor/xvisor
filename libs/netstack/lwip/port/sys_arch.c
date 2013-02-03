/**
 * Copyright (c) 2013 Anup Patel.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file sys_arch.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief OS interface required for lwIP
 */

#include <vmm_heap.h>
#include <vmm_timer.h>
#include <vmm_mutex.h>
#include <vmm_semaphore.h>
#include <vmm_completion.h>
#include <vmm_threads.h>
#include <libs/list.h>
#include <libs/mathlib.h>

#include "lwip/sys.h"

struct sys_sem {
	u8_t count;
	struct vmm_semaphore s;
	struct vmm_completion c;
};

err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
	struct sys_sem *ss;

	ss = vmm_zalloc(sizeof(struct sys_sem));
	if (!ss) {
		return ERR_MEM;
	}

	ss->count = count;
	INIT_SEMAPHORE(&ss->s, count, count);
	INIT_COMPLETION(&ss->c);

	*sem = ss;

	return ERR_OK;
}

void sys_sem_free(sys_sem_t *sem)
{
	if ((sem != NULL) && (*sem != SYS_SEM_NULL)) {
		struct sys_sem *ss = *sem;

		vmm_free(ss);
	}
}

void sys_sem_signal(sys_sem_t *sem)
{
	if ((sem != NULL) && (*sem != SYS_SEM_NULL)) {
		struct sys_sem *ss = *sem;

		if (ss->count) {
			vmm_semaphore_up(&ss->s);
		} else {
			vmm_completion_complete(&ss->c);
		}
	}
}

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
	u64 stimeout;

	stimeout = timeout;
	stimeout = stimeout * 1000000ULL;

	if ((sem != NULL) && (*sem != SYS_SEM_NULL)) {
		struct sys_sem *ss = *sem;

		if (ss->count && timeout) {
			vmm_semaphore_down_timeout(&ss->s, &stimeout);
		} else if (ss->count && !timeout) {
			vmm_semaphore_down(&ss->s);
		} else if (!ss->count && timeout) {
			vmm_completion_wait_timeout(&ss->c, &stimeout);
		} else {
			vmm_completion_wait(&ss->c);
		}
	}

	return udiv64(stimeout, 1000000ULL);
}

int sys_sem_valid(sys_sem_t *sem)
{
	return ((sem != NULL) && (*sem != NULL));
}

void sys_sem_set_invalid(sys_sem_t *sem)
{
	if (sem != NULL) {
		*sem = NULL;
	}
}

#define SYS_MBOX_SIZE			128

struct sys_mbox {
	struct vmm_spinlock lock;
	int first, last, avail, size;
	void *msg[SYS_MBOX_SIZE];
	struct vmm_completion not_empty;
	struct vmm_completion not_full;
};

err_t sys_mbox_new(sys_mbox_t *mb, int size)
{
	struct sys_mbox *mbox;

	size = SYS_MBOX_SIZE; /* Override mbox size */

	mbox = vmm_zalloc(sizeof(struct sys_mbox));
	if (mbox == NULL) {
		return ERR_MEM;
	}

	INIT_SPIN_LOCK(&mbox->lock);

	mbox->first = mbox->last = mbox->avail = 0;
	mbox->size = size;
	INIT_COMPLETION(&mbox->not_empty);
	INIT_COMPLETION(&mbox->not_full);

	*mb = mbox;

	return ERR_OK;
}

void sys_mbox_free(sys_mbox_t *mb)
{
	if ((mb != NULL) && (*mb != SYS_MBOX_NULL)) {
		struct sys_mbox *mbox = *mb;
		vmm_free(mbox);
	}
}

err_t sys_mbox_trypost(sys_mbox_t *mb, void *msg)
{
	irq_flags_t flags;
	struct sys_mbox *mbox;

	LWIP_ASSERT("invalid mbox", (mb != NULL) && (*mb != NULL));

	mbox = *mb;

	vmm_spin_lock_irqsave(&mbox->lock, flags);

	LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_trypost: mbox %p msg %p\n",
                          (void *)mbox, (void *)msg));

	if (mbox->avail == mbox->size) {
		vmm_spin_unlock_irqrestore(&mbox->lock, flags);
		return ERR_MEM;
	}

	mbox->msg[mbox->last] = msg;

	mbox->last++;
	if (mbox->last == mbox->size) {
		mbox->last = 0;
	}
	mbox->avail++;

	vmm_spin_unlock_irqrestore(&mbox->lock, flags);

	vmm_completion_complete(&mbox->not_empty);

	return ERR_OK;
}

void sys_mbox_post(sys_mbox_t *mb, void *msg)
{
	irq_flags_t flags;
	struct sys_mbox *mbox;

	LWIP_ASSERT("invalid mbox", (mb != NULL) && (*mb != NULL));

	mbox = *mb;

	vmm_spin_lock_irqsave(&mbox->lock, flags);

	LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_post: mbox %p msg %p\n", 
				(void *)mbox, (void *)msg));

	while (mbox->avail == mbox->size) {
		vmm_spin_unlock_irqrestore(&mbox->lock, flags);
		vmm_completion_wait(&mbox->not_full);
		vmm_spin_lock_irqsave(&mbox->lock, flags);
	}

	mbox->msg[mbox->last] = msg;

	mbox->last++;
	if (mbox->last >= mbox->size) {
		mbox->last = 0;
	}
	mbox->avail++;

	vmm_spin_unlock_irqrestore(&mbox->lock, flags);

	vmm_completion_complete(&mbox->not_empty);
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mb, void **msg)
{
	irq_flags_t flags;
	struct sys_mbox *mbox;

	LWIP_ASSERT("invalid mbox", (mb != NULL) && (*mb != NULL));

	mbox = *mb;

	vmm_spin_lock_irqsave(&mbox->lock, flags);

	if (!mbox->avail) {
		vmm_spin_unlock_irqrestore(&mbox->lock, flags);
		return SYS_MBOX_EMPTY;
	}

	if (msg != NULL) {
		LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_tryfetch: mbox %p msg %p\n", 
					(void *)mbox, *msg));
		*msg = mbox->msg[mbox->first];
	} else {
		LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_tryfetch: mbox %p, null\n", 
					(void *)mbox));
	}

	mbox->first++;
	if (mbox->first == mbox->size) {
		mbox->first = 0;
	}
	mbox->avail--;

	vmm_spin_unlock_irqrestore(&mbox->lock, flags);

	vmm_completion_complete(&mbox->not_full);

	return 0;
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mb, void **msg, u32_t timeout)
{
	int rc;
	u64 time_avail;
	irq_flags_t flags;
	struct sys_mbox *mbox;

	LWIP_ASSERT("invalid mbox", (mb != NULL) && (*mb != NULL));

	time_avail = timeout * 1000000ULL;
	mbox = *mb;

	vmm_spin_lock_irqsave(&mbox->lock, flags);

	while (!mbox->avail) {
		vmm_spin_unlock_irqrestore(&mbox->lock, flags);
		if (timeout) {
			rc = vmm_completion_wait_timeout(&mbox->not_empty, &time_avail);
		} else {
			rc = vmm_completion_wait(&mbox->not_empty);
		}
		if (rc) {
			return SYS_ARCH_TIMEOUT;
		}
		vmm_spin_lock_irqsave(&mbox->lock, flags);
	}

	if (msg != NULL) {
		LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_fetch: mbox %p msg %p\n", 
					(void *)mbox, *msg));
		*msg = mbox->msg[mbox->first];
	} else {
		LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_fetch: mbox %p, null msg\n", 
					(void *)mbox));
	}

	mbox->first++;
	if (mbox->first == mbox->size) {
		mbox->first = 0;
	}
	mbox->avail--;

	vmm_spin_unlock_irqrestore(&mbox->lock, flags);

	vmm_completion_complete(&mbox->not_full);

	return ((u64)timeout * 1000000ULL) - time_avail;
}

int sys_mbox_valid(sys_mbox_t *mb)
{
	return ((mb != NULL) && (*mb != NULL));
}

void sys_mbox_set_invalid(sys_mbox_t *mb)
{
	if (mb != NULL) {
		*mb = NULL;
	}
}

struct sys_thread {
	struct dlist head;
	const char *name;
	lwip_thread_fn function;
	void *arg;
	int stacksize;
	int prio;
	struct vmm_thread *thread;
};

static struct dlist st_list;
static struct vmm_spinlock st_list_lock;

static int sys_thread_main(void *data)
{
	struct sys_thread *t = data;

	t->function(t->arg);

	return VMM_OK;
}

sys_thread_t sys_thread_new(const char *name, 
			    lwip_thread_fn function, 
			    void *arg, int stacksize, int prio)
{
	irq_flags_t flags;
	struct sys_thread *st = NULL;

	st = vmm_zalloc(sizeof(struct sys_thread));
	if (!st) {
		vmm_panic("Failed to alloc sys_thread\n");
		return NULL;
	}

	INIT_LIST_HEAD(&st->head);
	st->name = name;
	st->function = function;
	st->arg = arg;
	st->stacksize = stacksize;
	st->prio = prio;

	st->thread = vmm_threads_create(st->name, &sys_thread_main, st, 
					VMM_THREAD_DEF_PRIORITY,
					VMM_THREAD_DEF_TIME_SLICE);
	if (!st->thread) {
		vmm_panic("Failed to create thread.\n");
	}

	vmm_spin_lock_irqsave(&st_list_lock, flags);
	list_add_tail(&st->head, &st_list);
	vmm_spin_unlock_irqrestore(&st_list_lock, flags);

	vmm_threads_start(st->thread);

	return st;
}

u32_t sys_jiffies(void)
{
	return udiv64(vmm_timer_timestamp(), 1000000ULL);
}

u32_t sys_now(void)
{
	return udiv64(vmm_timer_timestamp(), 1000000ULL);
}

void sys_init(void)
{
	INIT_LIST_HEAD(&st_list);
	INIT_SPIN_LOCK(&st_list_lock);
}
