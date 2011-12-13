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
 * @file vmm_semaphore.h
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file of sempahore locks for Orphan VCPU (or Thread).
 */

#ifndef __VMM_SEMAPHORE_H__
#define __VMM_SEMAPHORE_H__

#include <vmm_cpu.h>
#include <vmm_stdio.h>
#include <vmm_waitqueue.h>

struct vmm_semaphore {
	u32 limit;
	atomic_t value;
	vmm_waitqueue_t	wq;
};

typedef struct vmm_semaphore vmm_semaphore_t;

#define INIT_SEMAPHORE(sem, value)	do { \
					(sem)->limit = (value); \
					vmm_cpu_atomic_write(&(sem)->value, value); \
					INIT_WAITQUEUE(&(sem)->wq); \
					} while (0);

/** Check if semaphore is available */
static inline bool vmm_semaphore_avail(vmm_semaphore_t * sem)
{
	BUG_ON(!sem, "%s: NULL poniter to semaphore\n", __func__);

	return vmm_cpu_atomic_read(&(sem)->value) ? TRUE : FALSE;
}

/** Get maximum value (or limit) of semaphore */
static inline u32 vmm_semaphore_limit(vmm_semaphore_t * sem)
{
	BUG_ON(!sem, "%s: NULL poniter to semaphore\n", __func__);

	return sem->limit;
}

/** Release (or increment) semaphore */
int vmm_semaphore_up(vmm_semaphore_t * sem);

/** Acquire (or decrement) semaphore */
int vmm_semaphore_down(vmm_semaphore_t * sem);

#endif /* __VMM_SEMAPHORE_H__ */
