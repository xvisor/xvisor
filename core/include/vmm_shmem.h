/**
 * Copyright (c) 2017 Anup Patel.
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
 * @file vmm_shmem.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for shared memory subsystem
 */
#ifndef __VMM_SHMEM_H__
#define __VMM_SHMEM_H__

#include <vmm_types.h>
#include <vmm_limits.h>
#include <arch_atomic.h>
#include <libs/list.h>

struct vmm_shmem {
	struct dlist head;
	atomic_t ref_count;
	char name[VMM_FIELD_NAME_SIZE];
	physical_addr_t addr;
	physical_size_t size;
	u32 align_order;
	void *priv;
};

/** Read from shared memory instance */
u32 vmm_shmem_read(struct vmm_shmem *shm, physical_addr_t off,
		   void *dst, u32 len, bool cacheable);

/** Write to shared memory instance */
u32 vmm_shmem_write(struct vmm_shmem *shm, physical_addr_t off,
		    void *src, u32 len, bool cacheable);

/** Write a byte pattern to shared memory instance */
u32 vmm_shmem_set(struct vmm_shmem *shm, physical_addr_t off,
		  u8 byte, u32 len, bool cacheable);

/** Iterate over each shared memory instance */
int vmm_shmem_iterate(int (*iter)(struct vmm_shmem *, void *),
		      void *priv);

/** Count shared memory instances */
u32 vmm_shmem_count(void);

/** Find shared memory instance by name */
struct vmm_shmem *vmm_shmem_find_byname(const char *name);

/** Increment shared memory instance reference count */
void vmm_shmem_ref(struct vmm_shmem *shm);

/** Decrement shared memory instance reference count */
void vmm_shmem_dref(struct vmm_shmem *shm);

/** Create shared memory instance */
struct vmm_shmem *vmm_shmem_create(const char *name,
				   physical_size_t size,
				   u32 align_order, void *priv);

/** Destroy shared memory instance */
static inline void vmm_shmem_destroy(struct vmm_shmem *shm)
{
	if (shm) {
		vmm_shmem_dref(shm);
	}
}

/** Get name of shared memory instance */
static inline const char *vmm_shmem_get_name(struct vmm_shmem *shm)
{
	return (shm) ? shm->name : NULL;
}

/** Get address of shared memory instance */
static inline physical_addr_t vmm_shmem_get_addr(struct vmm_shmem *shm)
{
	return (shm) ? shm->addr : 0x0;
}

/** Get size of shared memory instance */
static inline physical_size_t vmm_shmem_get_size(struct vmm_shmem *shm)
{
	return (shm) ? shm->size : 0x0;
}

/** Get align order of shared memory */
static inline u32 vmm_shmem_get_align_order(struct vmm_shmem *shm)
{
	return (shm) ? shm->align_order : 0;
}

/** Get reference count of shared memory */
static inline u32 vmm_shmem_get_ref_count(struct vmm_shmem *shm)
{
	return (shm) ? arch_atomic_read(&shm->ref_count) : 0;
}

/** Get private pointer of shared memory instance */
static inline void *vmm_shmem_get_priv(struct vmm_shmem *shm)
{
	return (shm) ? shm->priv : NULL;
}

/** Set private pointer of shared memory instance */
static inline void vmm_shmem_set_priv(struct vmm_shmem *shm, void *priv)
{
	if (shm) {
		shm->priv = priv;
	}
}

/** Initialize shared memory subsystem */
int vmm_shmem_init(void);

#endif
