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
 * @file vmm_host_aspace.c
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
		if (pool_sz > VMM_PAGE_SIZE) {
			pool_sz -= VMM_PAGE_SIZE;
		} else {
			pool_sz = 0;
		}
	}

	found = 0;
	for (bpos = 0; bpos < (hactrl.pool_sz / VMM_PAGE_SIZE); bpos += bcnt) {
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

	*pool_va = hactrl.pool_va + bpos * VMM_PAGE_SIZE;
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
		if (pool_sz > VMM_PAGE_SIZE) {
			pool_sz -= VMM_PAGE_SIZE;
		} else {
			pool_sz = 0;
		}
	}

	bpos = (pool_va - hactrl.pool_va) / VMM_PAGE_SIZE;

	for (i = bpos; i < (bpos + bcnt); i++) {
		hactrl.pool_bmap[i / 32] &= ~(0x1 << (31 - (i % 32)));
	}

	return VMM_OK;
}

virtual_addr_t vmm_host_memmap(physical_addr_t pa, 
			       virtual_size_t sz, 
			       u32 mem_flags)
{
	int rc, ite;
	virtual_addr_t va;
	physical_addr_t tpa;

	sz = VMM_ROUNDUP2_PGSZ(sz);

	rc = vmm_host_aspace_pool_alloc(sz, &va);
	if (rc) {
		/* Don't have space */
		while (1) ;
	}

	tpa = pa & ~(VMM_PAGE_SIZE - 1);
	for (ite = 0; ite < (sz / VMM_PAGE_SIZE); ite++) {
		rc = vmm_cpu_aspace_map(va + ite * VMM_PAGE_SIZE, 
					VMM_PAGE_SIZE, 
					tpa + ite * VMM_PAGE_SIZE, 
					mem_flags);
		if (rc) {
			/* We were not able to map physical address */
			while (1) ;
		}
	}

	return va + (pa & (VMM_PAGE_SIZE - 1));
}

int vmm_host_memunmap(virtual_addr_t va, virtual_size_t sz)
{
	int rc, ite;

	sz = VMM_ROUNDUP2_PGSZ(sz);
	va &= ~(VMM_PAGE_SIZE - 1);

	rc = cpu_host_aspace_pool_free(va, sz);
	if (rc) {
		return rc;
	}

	for (ite = 0; ite < (sz / VMM_PAGE_SIZE); ite++) {
		rc = vmm_cpu_aspace_unmap(va + ite * VMM_PAGE_SIZE, 
					  VMM_PAGE_SIZE);
		if (rc) {
			return rc;
		}
	}

	return VMM_OK;
}

u32 vmm_host_physical_read(physical_addr_t hphys_addr, 
			   void * dst, u32 len)
{
	u32 bytes_read = 0, to_read = 0;
	virtual_addr_t src = 0x0;

	/* FIXME: Added more sanity checkes for 
	 * allowable physical address 
	 */

	while (bytes_read < len) {
		if (hphys_addr & (VMM_PAGE_SIZE - 1)) {
			to_read = hphys_addr & (VMM_PAGE_SIZE - 1);
		} else {
			to_read = VMM_PAGE_SIZE;
		}
		to_read = (to_read < (len - bytes_read)) ? 
			   to_read : (len - bytes_read);

		src = vmm_host_memmap(hphys_addr, 
				      VMM_PAGE_SIZE, 
				      VMM_MEMORY_READABLE);
		vmm_memcpy(dst, (void *)src, to_read);
		vmm_host_memunmap(src, VMM_PAGE_SIZE);

		hphys_addr += to_read;
		bytes_read += to_read;
		dst += to_read;
	}

	return bytes_read;
}

u32 vmm_host_physical_write(physical_addr_t hphys_addr, 
			    void * src, u32 len)
{
	u32 bytes_written = 0, to_write = 0;
	virtual_addr_t dst = 0x0;

	/* FIXME: Added more sanity checkes for 
	 * allowable physical address 
	 */

	while (bytes_written < len) {
		if (hphys_addr & (VMM_PAGE_SIZE - 1)) {
			to_write = hphys_addr & (VMM_PAGE_SIZE - 1);
		} else {
			to_write = VMM_PAGE_SIZE;
		}
		to_write = (to_write < (len - bytes_written)) ? 
			    to_write : (len - bytes_written);

		dst = vmm_host_memmap(hphys_addr, 
				      VMM_PAGE_SIZE, 
				      VMM_MEMORY_WRITEABLE);
		vmm_memcpy((void *)dst, src, to_write);
		vmm_host_memunmap(dst, VMM_PAGE_SIZE);

		hphys_addr += to_write;
		bytes_written += to_write;
		src += to_write;
	}

	return bytes_written;
}

int vmm_host_aspace_init(void)
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

	hactrl.pool_bmap_len = (hactrl.pool_sz / (VMM_PAGE_SIZE * 32) + 1);
	hactrl.pool_bmap = vmm_malloc(sizeof(u32) * hactrl.pool_bmap_len);
	vmm_memset(hactrl.pool_bmap, 0, sizeof(u32) * hactrl.pool_bmap_len);

	return vmm_cpu_aspace_init();
}

