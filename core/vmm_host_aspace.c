/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file vmm_host_addr.c
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Source file for host virtual address space management.
 */

#include <vmm_error.h>
#include <vmm_list.h>
#include <vmm_cpu.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_devtree.h>
#include <vmm_host_aspace.h>

vmm_host_aspace_ctrl_t hactrl;

int vmm_host_aspace_pool_alloc(virtual_size_t pool_sz, virtual_addr_t * pool_va)
{
	u32 i, found, bcnt, bpos, bfree;

	bcnt = 0;
	while (pool_sz > 0) {
		bcnt++;
		if (pool_sz > PAGE_SIZE) {
			pool_sz -= PAGE_SIZE;
		} else {
			pool_sz = 0;
		}
	}

	found = 0;
	for (bpos = 0; bpos < (hactrl.pool_sz / PAGE_SIZE); bpos += bcnt) {
		bfree = 0;
		for (i = bpos; i < (bpos + bcnt); i++) {
			if (hactrl.pool_bmap[i / 32] & (0x1 << (31 - (i % 32)))) {
				break;
			}
			bfree++;
		}
		if (bfree == bcnt) {
			found = 1;
			break;
		}
	}
	if (!found) {
		return VMM_EFAIL;
	}

	*pool_va = hactrl.pool_va + bpos * PAGE_SIZE;
	for (i = bpos; i < (bpos + bcnt); i++) {
		hactrl.pool_bmap[i / 32] |= (0x1 << (31 - (i % 32)));
	}

	return VMM_OK;
}

int cpu_host_aspace_pool_free(virtual_addr_t pool_va, virtual_addr_t pool_sz)
{
	u32 i, bcnt, bpos;

	if (pool_va < hactrl.pool_va ||
	    (hactrl.pool_va + hactrl.pool_sz) <= pool_va) {
		return VMM_EFAIL;
	}

	bcnt = 0;
	while (pool_sz > 0) {
		bcnt++;
		if (pool_sz > PAGE_SIZE) {
			pool_sz -= PAGE_SIZE;
		} else {
			pool_sz = 0;
		}
	}

	bpos = (pool_va - hactrl.pool_va) / PAGE_SIZE;

	for (i = bpos; i < (bpos + bcnt); i++) {
		hactrl.pool_bmap[i / 32] &= ~(0x1 << (31 - (i % 32)));
	}

	return VMM_OK;
}

s32 vmm_host_aspace_init(void)
{
	vmm_devtree_node_t *node;
	const char *attrval;

	vmm_memset(&hactrl, 0, sizeof(hactrl));

	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPRATOR_STRING
				   VMM_DEVTREE_HOSTINFO_NODE_NAME);
	if (!node) {
		return VMM_EFAIL;
	}

	attrval = vmm_devtree_attrval(node,
				      VMM_DEVTREE_HOST_VIRT_START_ATTR_NAME);
	if (!attrval) {
		return VMM_EFAIL;
	}

	hactrl.pool_va = *((virtual_addr_t *) attrval);

	attrval = vmm_devtree_attrval(node,
				      VMM_DEVTREE_HOST_VIRT_SIZE_ATTR_NAME);

	if (!attrval) {
		return VMM_EFAIL;
	}

	hactrl.pool_sz = *((virtual_size_t *) attrval);

	hactrl.pool_bmap_len = (hactrl.pool_sz / (PAGE_SIZE * 32) + 1);
	hactrl.pool_bmap = vmm_malloc(sizeof(u32) * hactrl.pool_bmap_len);
	vmm_memset(hactrl.pool_bmap, 0, sizeof(u32) * hactrl.pool_bmap_len);

	return vmm_cpu_aspace_init();
}

virtual_addr_t vmm_host_iomap(physical_addr_t pa, virtual_size_t sz)
{
	int rc;
	virtual_addr_t va;

	sz = ROUNDUP2_PGSZ(sz);

	rc = vmm_host_aspace_pool_alloc(sz, &va);
	if (rc) {
		/* Don't have space */
		while (1) ;
	}

	rc = vmm_cpu_iomap(va, sz, pa);
	if (rc) {
		/* We were not able to map physical address */
		while (1) ;
	}

	return va;
}

int vmm_host_iounmap(virtual_addr_t va, virtual_size_t sz)
{
	int rc;

	rc = cpu_host_aspace_pool_free(va, sz);
	if (rc) {
		return rc;
	}

	return vmm_cpu_iounmap(va, sz);
}
