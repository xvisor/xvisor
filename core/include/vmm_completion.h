/**
 * Copyright (c) 2012 Anup Patel.
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
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file of completion events for Orphan VCPU (or Thread).
 */

#ifndef __VMM_COMPLETION_H__
#define __VMM_COMPLETION_H__

#include <vmm_waitqueue.h>

/** Completion event structure */
struct vmm_completion {
	u32 done;
	struct vmm_waitqueue wq;
};

/** Initialize completion event */
#define INIT_COMPLETION(cptr)			do { \
						(cptr)->done = 0; \
						INIT_WAITQUEUE(&(cptr)->wq); \
						} while (0)

/** Re-initialize completion event. 
 *
 * This macro should be used to reinitialize a completion structure so it can
 * be reused. This is especially important after complete_all() is used.
 */
#define REINIT_COMPLETION(cptr)			do { \
						(cptr)->done = 0; \
						} while (0)
/** Check if completion is done */
bool vmm_completion_done(struct vmm_completion *cmpl);

/** Wait for completion */
int vmm_completion_wait(struct vmm_completion *cmpl);

/** Wait for completion for given timeout */
int vmm_completion_wait_timeout(struct vmm_completion *cmpl, u64 *timeout);

/** Signal completion and wake first sleeping Orphan VCPU */
int vmm_completion_complete(struct vmm_completion *cmpl);

/** Signal completion and wake all sleeping Orphan VCPUs */
int vmm_completion_complete_all(struct vmm_completion *cmpl);

#endif /* __VMM_COMPLETION_H__ */
