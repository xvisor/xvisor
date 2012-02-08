/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file vmm_waitqueue.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file for Orphan VCPU (or Thread) wait queue. 
 */

#ifndef __VMM_WAITQUEUE_H__
#define __VMM_WAITQUEUE_H__

#include <vmm_list.h>
#include <vmm_spinlocks.h>
#include <vmm_manager.h>

struct vmm_waitqueue {
	vmm_spinlock_t lock;
	struct dlist vcpu_list;
	u32 vcpu_count;
};

#define INIT_WAITQUEUE(wqptr)	do { \
				INIT_SPIN_LOCK(&((wqptr)->lock)); \
				INIT_LIST_HEAD(&((wqptr)->vcpu_list)); \
				(wqptr)->vcpu_count = 0; \
				} while (0);

/* Waiting VCPU count */
u32 vmm_waitqueue_count(struct vmm_waitqueue * wq);

/* Put current VCPU to sleep on given waitqueue */
int vmm_waitqueue_sleep(struct vmm_waitqueue * wq);

/* Wakeup VCPU from its waitqueue */
int vmm_waitqueue_wake(struct vmm_vcpu * vcpu);

/* Wakeup first VCPU in a given waitqueue */
int vmm_waitqueue_wakefirst(struct vmm_waitqueue * wq);

/* Wakeup all VCPUs in a given waitqueue */
int vmm_waitqueue_wakeall(struct vmm_waitqueue * wq);

#endif /* __VMM_WAITQUEUE_H__ */
