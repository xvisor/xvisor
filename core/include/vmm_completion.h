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
 * @file vmm_completion.h
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file of completion locks for Orphan VCPU (or Thread).
 */

#ifndef __VMM_COMPLETION_H__
#define __VMM_COMPLETION_H__

#include <vmm_waitqueue.h>

struct vmm_completion {
	vmm_spinlock_t lock;
	u32 done;
	vmm_waitqueue_t wq;
};

typedef struct vmm_completion vmm_completion_t;

#define INIT_COMPLETION(cptr)	do { \
				INIT_SPIN_LOCK(&((cptr)->lock)); \
				(cptr)->done = 0; \
				INIT_WAITQUEUE(&(cptr)->wq); \
				} while (0);

/* Check status of work completion */
bool vmm_completion_done(vmm_completion_t * cmpl);

/* Wait for work completion */
int vmm_completion_wait(vmm_completion_t * cmpl);

/* Wakeup (or Signal) first waiting Orphan VCPU (or Thread) */
int vmm_completion_complete_first(vmm_completion_t * cmpl);

/* Wakeup (or Signal) all waiting Orphan VCPUs (or Threads) */
int vmm_completion_complete_all(vmm_completion_t * cmpl);

#endif /* __VMM_COMPLETION_H__ */
