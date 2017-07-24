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
 * @file vmm_shmem.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for shared memory subsystem
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_mutex.h>
#include <vmm_host_ram.h>
#include <vmm_host_aspace.h>
#include <vmm_shmem.h>
#include <libs/stringlib.h>

struct vmm_shmem_ctrl {
	struct vmm_mutex lock;
	struct dlist shmem_list;
};

static struct vmm_shmem_ctrl shmctrl;

u32 vmm_shmem_read(struct vmm_shmem *shm, physical_addr_t off,
		   void *dst, u32 len, bool cacheable)
{
	if (!shm || !dst)
		return 0;
	if (shm->size < (off + len))
		return 0;

	return vmm_host_memory_read(shm->addr + off, dst, len, cacheable);
}

u32 vmm_shmem_write(struct vmm_shmem *shm, physical_addr_t off,
		    void *src, u32 len, bool cacheable)
{
	if (!shm || !src)
		return 0;
	if (shm->size < (off + len))
		return 0;

	return vmm_host_memory_write(shm->addr + off, src, len, cacheable);
}

u32 vmm_shmem_set(struct vmm_shmem *shm, physical_addr_t off,
		  u8 byte, u32 len, bool cacheable)
{
	if (!shm)
		return 0;
	if (shm->size < (off + len))
		return 0;

	return vmm_host_memory_set(shm->addr + off, byte, len, cacheable);
}

int vmm_shmem_iterate(int (*iter)(struct vmm_shmem *, void *),
		      void *priv)
{
	int rc = VMM_OK;
	struct vmm_shmem *shm;

	if (!iter)
		return VMM_EINVALID;

	vmm_mutex_lock(&shmctrl.lock);

	list_for_each_entry(shm, &shmctrl.shmem_list, head) {
		rc = iter(shm, priv);
		if (rc) {
			break;
		}
	}

	vmm_mutex_unlock(&shmctrl.lock);

	return rc;
}

static int shmem_count(struct vmm_shmem *shm, void *priv)
{
	u32 *cntp = priv;

	if (cntp) {
		(*cntp)++;
	}

	return VMM_OK;
}

u32 vmm_shmem_count(void)
{
	u32 count = 0;

	return (!vmm_shmem_iterate(shmem_count, &count)) ? count : 0;
}

struct shmem_find_data {
	const char *name;
	struct vmm_shmem *shm;
};

static int shmem_find_byname(struct vmm_shmem *shm, void *priv)
{
	struct shmem_find_data *data = priv;

	if (!data->shm) {
		if (!strncmp(shm->name, data->name, sizeof(shm->name))) {
			vmm_shmem_ref(shm);
			data->shm = shm;
		}
	}

	return VMM_OK;
}

struct vmm_shmem *vmm_shmem_find_byname(const char *name)
{
	struct shmem_find_data data;

	if (!name) {
		return NULL;
	}

	data.name = name;
	data.shm = NULL;

	return (!vmm_shmem_iterate(shmem_find_byname, &data)) ?
		data.shm : NULL;
}

void vmm_shmem_ref(struct vmm_shmem *shm)
{
	if (!shm) {
		return;
	}

	arch_atomic_inc(&shm->ref_count);
}

void vmm_shmem_dref(struct vmm_shmem *shm)
{
	if (!shm) {
		return;
	}

	if (arch_atomic_sub_return(&shm->ref_count, 1)) {
		return;
	}

	vmm_mutex_lock(&shmctrl.lock);

	list_del(&shm->head);
	vmm_host_ram_free(shm->addr, shm->size);
	vmm_free(shm);

	vmm_mutex_unlock(&shmctrl.lock);
}

struct vmm_shmem *vmm_shmem_create(const char *name,
				   physical_size_t size,
				   u32 align_order, void *priv)
{
	bool found = FALSE;
	struct vmm_shmem *shm;

	if (!name || !size) {
		return VMM_ERR_PTR(VMM_EINVALID);
	}
	size = VMM_ROUNDUP2_PAGE_SIZE(size);

	vmm_mutex_lock(&shmctrl.lock);

	list_for_each_entry(shm, &shmctrl.shmem_list, head) {
		if (!strncmp(shm->name, name, sizeof(shm->name))) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		vmm_mutex_unlock(&shmctrl.lock);
		return VMM_ERR_PTR(VMM_EEXIST);
	};

	shm = vmm_zalloc(sizeof(*shm));
	if (!shm) {
		vmm_mutex_unlock(&shmctrl.lock);
		return VMM_ERR_PTR(VMM_ENOMEM);
	}

	INIT_LIST_HEAD(&shm->head);
	arch_atomic_write(&shm->ref_count, 1);
	strncpy(shm->name, name, sizeof(shm->name));

	shm->size = vmm_host_ram_alloc(&shm->addr, size, align_order);
	if (!shm->size) {
		vmm_free(shm);
		vmm_mutex_unlock(&shmctrl.lock);
		return VMM_ERR_PTR(VMM_ENOMEM);
	}
	shm->align_order = align_order;
	shm->priv = priv;

	list_add_tail(&shm->head, &shmctrl.shmem_list);

	vmm_mutex_unlock(&shmctrl.lock);

	return shm;
}

int __init vmm_shmem_init(void)
{
	memset(&shmctrl, 0, sizeof(shmctrl));

	INIT_MUTEX(&shmctrl.lock);
	INIT_LIST_HEAD(&shmctrl.shmem_list);

	return VMM_OK;
}
