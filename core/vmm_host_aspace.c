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
#include <vmm_board.h>
#include <vmm_sections.h>
#include <vmm_string.h>
#include <vmm_host_aspace.h>

struct vmm_host_aspace_ctrl {
	u32 *vapool_bmap;
	u32 vapool_bmap_len;
	virtual_addr_t vapool_start;
	virtual_size_t vapool_size;
	u32 *ram_bmap;
	u32 ram_bmap_len;
	physical_addr_t ram_start;
	physical_size_t ram_size;
};

typedef struct vmm_host_aspace_ctrl vmm_host_aspace_ctrl_t;

vmm_host_aspace_ctrl_t hactrl;

int vmm_host_vapool_alloc(virtual_addr_t * va, virtual_size_t sz, bool aligned)
{
	u32 i, found, binc, bcnt, bpos, bfree;

	bcnt = 0;
	while (sz > 0) {
		bcnt++;
		if (sz > VMM_PAGE_SIZE) {
			sz -= VMM_PAGE_SIZE;
		} else {
			sz = 0;
		}
	}

	found = 0;
	if (aligned & (sz > VMM_PAGE_SIZE)) {
		bpos = (hactrl.vapool_start % sz);
		if (bpos) {
			bpos = VMM_ROUNDUP2_PGSZ(sz) / VMM_PAGE_SIZE;
		}
		binc = bcnt;
	} else {
		bpos = 0;
		binc = 1;
	}
	for ( ; bpos < (hactrl.vapool_size / VMM_PAGE_SIZE); bpos += binc) {
		bfree = 0;
		for (i = bpos; i < (bpos + bcnt); i++) {
			if (hactrl.vapool_bmap[i / 32] & 
			    (0x1 << (31 - (i % 32)))) {
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

	*va = hactrl.vapool_start + bpos * VMM_PAGE_SIZE;
	for (i = bpos; i < (bpos + bcnt); i++) {
		hactrl.vapool_bmap[i / 32] |= (0x1 << (31 - (i % 32)));
	}

	return VMM_OK;
}

int vmm_host_vapool_free(virtual_addr_t va, virtual_size_t sz)
{
	u32 i, bcnt, bpos;

	if (va < hactrl.vapool_start ||
	    (hactrl.vapool_start + hactrl.vapool_size) <= va) {
		return VMM_EFAIL;
	}

	bcnt = 0;
	while (sz > 0) {
		bcnt++;
		if (sz > VMM_PAGE_SIZE) {
			sz -= VMM_PAGE_SIZE;
		} else {
			sz = 0;
		}
	}

	bpos = (va - hactrl.vapool_start) / VMM_PAGE_SIZE;

	for (i = bpos; i < (bpos + bcnt); i++) {
		hactrl.vapool_bmap[i / 32] &= ~(0x1 << (31 - (i % 32)));
	}

	return VMM_OK;
}

int vmm_host_ram_alloc(physical_addr_t * pa, physical_size_t sz, bool aligned)
{
	u32 i, found, binc, bcnt, bpos, bfree;

	bcnt = 0;
	while (sz > 0) {
		bcnt++;
		if (sz > VMM_PAGE_SIZE) {
			sz -= VMM_PAGE_SIZE;
		} else {
			sz = 0;
		}
	}

	found = 0;
	if (aligned & (sz > VMM_PAGE_SIZE)) {
		bpos = (hactrl.ram_start % sz);
		if (bpos) {
			bpos = VMM_ROUNDUP2_PGSZ(sz) / VMM_PAGE_SIZE;
		}
		binc = bcnt;
	} else {
		bpos = 0;
		binc = 1;
	}
	for ( ; bpos < (hactrl.ram_size / VMM_PAGE_SIZE); bpos += binc) {
		bfree = 0;
		for (i = bpos; i < (bpos + bcnt); i++) {
			if (hactrl.ram_bmap[i / 32] & 
			    (0x1 << (31 - (i % 32)))) {
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

	*pa = hactrl.ram_start + bpos * VMM_PAGE_SIZE;
	for (i = bpos; i < (bpos + bcnt); i++) {
		hactrl.ram_bmap[i / 32] |= (0x1 << (31 - (i % 32)));
	}

	return VMM_OK;
}

int vmm_host_ram_free(physical_addr_t pa, physical_size_t sz)
{
	u32 i, bcnt, bpos;

	if (pa < hactrl.ram_start ||
	    (hactrl.ram_start + hactrl.ram_size) <= pa) {
		return VMM_EFAIL;
	}

	bcnt = 0;
	while (sz > 0) {
		bcnt++;
		if (sz > VMM_PAGE_SIZE) {
			sz -= VMM_PAGE_SIZE;
		} else {
			sz = 0;
		}
	}

	bpos = (pa - hactrl.ram_start) / VMM_PAGE_SIZE;

	for (i = bpos; i < (bpos + bcnt); i++) {
		hactrl.ram_bmap[i / 32] &= ~(0x1 << (31 - (i % 32)));
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

	if ((rc = vmm_host_vapool_alloc(&va, sz, FALSE))) {
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

	if ((rc = vmm_host_vapool_free(va, sz))) {
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

virtual_addr_t vmm_host_alloc_pages(u32 page_count, u32 mem_flags)
{
	int rc = VMM_OK;
	physical_addr_t pa = 0x0;

	rc = vmm_host_ram_alloc(&pa, page_count * VMM_PAGE_SIZE, FALSE);
	if (rc) {
		return 0x0;
	}

	return vmm_host_memmap(pa, page_count * VMM_PAGE_SIZE, mem_flags);
}

int vmm_host_free_pages(virtual_addr_t page_va, u32 page_count)
{
	/* FIXME: */
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
	int ite, rc;
	u32 resv_size = 0x0, bmap_total_size = 0x0;

	vmm_memset(&hactrl, 0, sizeof(hactrl));

	if ((rc = vmm_board_ram_start(&hactrl.ram_start))) {
		return rc;
	}
	if ((rc = vmm_board_ram_size(&hactrl.ram_size))) {
		return rc;
	}
	if (hactrl.ram_start & VMM_PAGE_MASK) {
		hactrl.ram_size -= VMM_PAGE_SIZE;
		hactrl.ram_size += hactrl.ram_start & VMM_PAGE_MASK;
		hactrl.ram_start += VMM_PAGE_SIZE;
		hactrl.ram_start -= hactrl.ram_start & VMM_PAGE_MASK;
	}
	if (hactrl.ram_size & VMM_PAGE_MASK) {
		hactrl.ram_size -= hactrl.ram_size & VMM_PAGE_MASK;
	}
	hactrl.ram_bmap_len = hactrl.ram_size / (VMM_PAGE_SIZE * 32);
	hactrl.ram_bmap_len += 1;

	hactrl.vapool_start = vmm_vapool_start();
	hactrl.vapool_size = vmm_vapool_size();
	if (hactrl.vapool_start & VMM_PAGE_MASK) {
		hactrl.vapool_size -= VMM_PAGE_SIZE;
		hactrl.vapool_size += hactrl.vapool_start & VMM_PAGE_MASK;
		hactrl.vapool_start += VMM_PAGE_SIZE;
		hactrl.vapool_start -= hactrl.vapool_start & VMM_PAGE_MASK;
	}
	if (hactrl.vapool_size & VMM_PAGE_MASK) {
		hactrl.vapool_size -= hactrl.vapool_size & VMM_PAGE_MASK;
	}
	hactrl.vapool_bmap_len = hactrl.vapool_size / (VMM_PAGE_SIZE * 32);
	hactrl.vapool_bmap_len += 1;

	bmap_total_size = hactrl.ram_bmap_len + hactrl.vapool_bmap_len;
	bmap_total_size *= sizeof(u32);
	bmap_total_size = VMM_ROUNDUP2_PGSZ(bmap_total_size);
	resv_size = bmap_total_size;
	resv_size = vmm_cpu_aspace_init(hactrl.ram_start, 
					hactrl.vapool_start,
					bmap_total_size);
	if (resv_size < bmap_total_size) {
		return VMM_EFAIL;
	}
	if ((hactrl.vapool_size <= resv_size) || 
	    (hactrl.ram_size <= resv_size)) {
		return VMM_EFAIL;
	}

	hactrl.vapool_bmap = (u32 *)hactrl.vapool_start;
	hactrl.ram_bmap = &hactrl.vapool_bmap[hactrl.vapool_bmap_len];
	vmm_memset(hactrl.vapool_bmap, 0, sizeof(u32) * hactrl.vapool_bmap_len);
	vmm_memset(hactrl.ram_bmap, 0, sizeof(u32) * hactrl.ram_bmap_len);

	for (ite = 0; ite < (resv_size / VMM_PAGE_SIZE); ite++) {
		hactrl.vapool_bmap[ite / 32] |= (0x1 << (31 - (ite % 32)));
		hactrl.ram_bmap[ite / 32] |= (0x1 << (31 - (ite % 32)));
	}

	return VMM_OK;
}

