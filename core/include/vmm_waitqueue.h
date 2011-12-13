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
 * @version 0.01
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

typedef struct vmm_waitqueue vmm_waitqueue_t;

#define INIT_WAITQUEUE(wqptr)	do { \
				INIT_SPIN_LOCK(&((wqptr)->lock)); \
				INIT_LIST_HEAD(&((wqptr)->vcpu_list)); \
				(wqptr)->vcpu_count = 0; \
				} while (0);

/* Waiting VCPU count */
u32 vmm_waitqueue_count(vmm_waitqueue_t * wq);

/* Put current VCPU to sleep on given waitqueue */
int vmm_waitqueue_sleep(vmm_waitqueue_t * wq);

/* Wakeup VCPU from its waitqueue */
int vmm_waitqueue_wake(vmm_vcpu_t * vcpu);

/* Wakeup first VCPU in a given waitqueue */
int vmm_waitqueue_wakefirst(vmm_waitqueue_t * wq);

/* Wakeup all VCPUs in a given waitqueue */
int vmm_waitqueue_wakeall(vmm_waitqueue_t * wq);

#endif /* __VMM_WAITQUEUE_H__ */
