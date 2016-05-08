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
 * @file vmm_mutex.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file of mutext locks for Orphan VCPU (or Thread).
 */

#ifndef __VMM_MUTEX_H__
#define __VMM_MUTEX_H__

#include <vmm_types.h>
#include <vmm_waitqueue.h>

/** Mutex lock structure */
struct vmm_mutex {
	u32 lock;
	struct vmm_vcpu_resource res;
	struct vmm_vcpu *owner;
	struct vmm_waitqueue wq;
};

/** Cleanup callback for mutex when VCPU is destroyed
 *  Note: This function should not be directly called from anywhere.
 */
void __vmm_mutex_cleanup(struct vmm_vcpu *vcpu,
			 struct vmm_vcpu_resource *vcpu_res);

/** Initialize mutex lock */
#define INIT_MUTEX(__mut)	\
do { \
	(__mut)->lock = 0; \
	(__mut)->res.name = "vmm_mutex"; \
	(__mut)->res.cleanup = __vmm_mutex_cleanup; \
	(__mut)->owner = NULL; \
	INIT_WAITQUEUE(&(__mut)->wq, (__mut)); \
} while (0)

#define __MUTEX_INITIALIZER(__mut) \
{ \
	.lock = 0, \
	.res = { .name = "vmm_mutex", .cleanup = __vmm_mutex_cleanup }, \
	.owner = NULL, \
	.wq = __WAITQUEUE_INITIALIZER((__mut).wq, &(__mut)), \
}

#define DEFINE_MUTEX(__mut) \
struct vmm_mutex __mut = __MUTEX_INITIALIZER(__mut)

/** Check if mutex is available */
bool vmm_mutex_avail(struct vmm_mutex *mut);

/** Get mutex owner */
struct vmm_vcpu *vmm_mutex_owner(struct vmm_mutex *mut);

/** Unlock mutex */
int vmm_mutex_unlock(struct vmm_mutex *mut);

/** Try to lock mutex without sleeping 
 *  NOTE: Returns 1 upon success and 0 on failure
 */
int vmm_mutex_trylock(struct vmm_mutex *mut);

/** Lock mutex */
int vmm_mutex_lock(struct vmm_mutex *mut);

/** Lock mutex with timeout */
int vmm_mutex_lock_timeout(struct vmm_mutex *mut, u64 *timeout);

#endif /* __VMM_MUTEX_H__ */
