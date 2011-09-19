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
 * @author Anup patel (anup@brainfault.org)
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
	u32 vapool_bmap_free;
	virtual_addr_t vapool_start;
	virtual_size_t vapool_size;
	u32 *ram_bmap;
	u32 ram_bmap_len;
	u32 ram_bmap_free;
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

	if (hactrl.vapool_bmap_free < bcnt) {
		return VMM_EFAIL;
	}

	found = 0;
	if (aligned && (sz > VMM_PAGE_SIZE)) {
		bpos = (hactrl.vapool_start % sz);
		if (bpos) {
			bpos = VMM_ROUNDUP2_PAGE_SIZE(sz) / VMM_PAGE_SIZE;
		}
		binc = bcnt;
	} else {
		bpos = 0;
		binc = 1;
	}
	for ( ; bpos < (hactrl.vapool_size / VMM_PAGE_SIZE); bpos += binc) {
		bfree = 0;
		for (i = bpos; i < (bpos + bcnt); i++) {
			if (hactrl.vapool_bmap[i >> 5] & 
			    (0x1 << (31 - (i & 0x1F)))) {
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
		hactrl.vapool_bmap[i >> 5] |= (0x1 << (31 - (i & 0x1F)));
		hactrl.vapool_bmap_free--;
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
		hactrl.vapool_bmap[i >> 5] &= ~(0x1 << (31 - (i & 0x1F)));
		hactrl.vapool_bmap_free++;
	}

	return VMM_OK;
}

virtual_addr_t vmm_host_vapool_base(void)
{
	return hactrl.vapool_start;
}

bool vmm_host_vapool_page_isfree(virtual_addr_t va)
{
	u32 bpos;

	if (va < hactrl.vapool_start ||
	    (hactrl.vapool_start + hactrl.vapool_size) <= va) {
		return TRUE;
	}

	bpos = (va - hactrl.vapool_start) / VMM_PAGE_SIZE;

	if (hactrl.vapool_bmap[bpos >> 5] & (0x1 << (31 - (bpos & 0x1F)))) {
		return FALSE;
	}

	return TRUE;
}

u32 vmm_host_vapool_free_page_count(void)
{
	return hactrl.vapool_bmap_free;
}

u32 vmm_host_vapool_total_page_count(void)
{
	return hactrl.vapool_size / VMM_PAGE_SIZE;
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

	if (hactrl.ram_bmap_free < bcnt) {
		return VMM_EFAIL;
	}

	found = 0;
	if (aligned && (sz > VMM_PAGE_SIZE)) {
		bpos = (hactrl.ram_start % sz);
		if (bpos) {
			bpos = VMM_ROUNDUP2_PAGE_SIZE(sz) / VMM_PAGE_SIZE;
		}
		binc = bcnt;
	} else {
		bpos = 0;
		binc = 1;
	}
	for ( ; bpos < (hactrl.ram_size / VMM_PAGE_SIZE); bpos += binc) {
		bfree = 0;
		for (i = bpos; i < (bpos + bcnt); i++) {
			if (hactrl.ram_bmap[i >> 5] & 
			    (0x1 << (31 - (i & 0x1F)))) {
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
		hactrl.ram_bmap[i >> 5] |= (0x1 << (31 - (i & 0x1F)));
		hactrl.ram_bmap_free--;
	}

	return VMM_OK;
}

int vmm_host_ram_reserve(physical_addr_t pa, physical_size_t sz)
{
	u32 i, bcnt, bpos, bfree;

	if ((pa < hactrl.ram_start) ||
	    ((hactrl.ram_start + hactrl.ram_size) <= pa)) {
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

	if (hactrl.ram_bmap_free < bcnt) {
		return VMM_EFAIL;
	}

	bpos = (pa - hactrl.ram_start) / VMM_PAGE_SIZE;
	bfree = 0;
	for (i = bpos; i < (bpos + bcnt); i++) {
		if (hactrl.ram_bmap[i >> 5] & 
		    (0x1 << (31 - (i & 0x1F)))) {
			break;
		}
		bfree++;
	}

	if (bfree != bcnt) {
		return VMM_EFAIL;
	}

	for (i = bpos; i < (bpos + bcnt); i++) {
		hactrl.ram_bmap[i >> 5] |= (0x1 << (31 - (i & 0x1F)));
		hactrl.ram_bmap_free--;
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
		hactrl.ram_bmap[i >> 5] &= ~(0x1 << (31 - (i & 0x1F)));
		hactrl.ram_bmap_free++;
	}

	return VMM_OK;
}

physical_addr_t vmm_host_ram_base(void)
{
	return hactrl.ram_start;
}

bool vmm_host_ram_frame_isfree(physical_addr_t pa)
{
	u32 bpos;

	if (pa < hactrl.ram_start ||
	    (hactrl.ram_start + hactrl.ram_size) <= pa) {
		return TRUE;
	}

	bpos = (pa - hactrl.ram_start) / VMM_PAGE_SIZE;

	if (hactrl.ram_bmap[bpos >> 5] & (0x1 << (31 - (bpos & 0x1F)))) {
		return FALSE;
	}

	return TRUE;
}

u32 vmm_host_ram_free_frame_count(void)
{
	return hactrl.ram_bmap_free;
}

u32 vmm_host_ram_total_frame_count(void)
{
	return hactrl.ram_size / VMM_PAGE_SIZE;
}

virtual_addr_t vmm_host_memmap(physical_addr_t pa, 
			       virtual_size_t sz, 
			       u32 mem_flags)
{
	int rc, ite;
	virtual_addr_t va;
	physical_addr_t tpa;

	sz = VMM_ROUNDUP2_PAGE_SIZE(sz);

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

	sz = VMM_ROUNDUP2_PAGE_SIZE(sz);
	va &= ~(VMM_PAGE_SIZE - 1);

	for (ite = 0; ite < (sz / VMM_PAGE_SIZE); ite++) {
		rc = vmm_cpu_aspace_unmap(va + ite * VMM_PAGE_SIZE, 
					  VMM_PAGE_SIZE);
		if (rc) {
			return rc;
		}
	}

	if ((rc = vmm_host_vapool_free(va, sz))) {
		return rc;
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
	int rc = VMM_OK;
	physical_addr_t pa = 0x0;

	page_va &= ~VMM_PAGE_MASK;

	if ((rc = vmm_cpu_aspace_va2pa(page_va, &pa))) {
		return rc;
	}

	if ((rc = vmm_host_memunmap(page_va, page_count * VMM_PAGE_SIZE))) {
		return rc;
	}

	return vmm_host_ram_free(pa, page_count * VMM_PAGE_SIZE);
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
	int ite, last, max, rc;
	physical_addr_t resv_pa = 0x0;
	virtual_addr_t resv_va = 0x0;
	u32 resv_sz = 0x0, bmap_total_size = 0x0;

	vmm_memset(&hactrl, 0, sizeof(hactrl));

	hactrl.vapool_start = vmm_code_vaddr();
	hactrl.vapool_size = vmm_code_size() + (CONFIG_VAPOOL_SIZE << 20);
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
	hactrl.vapool_bmap_free = hactrl.vapool_size / VMM_PAGE_SIZE;

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
	hactrl.ram_bmap_free = hactrl.ram_size / VMM_PAGE_SIZE;

	bmap_total_size = hactrl.vapool_bmap_len + hactrl.ram_bmap_len;
	bmap_total_size *= sizeof(u32);
	bmap_total_size = VMM_ROUNDUP2_PAGE_SIZE(bmap_total_size);
	resv_pa = hactrl.ram_start;
	resv_va = vmm_code_vaddr() + vmm_code_size();
	resv_sz = bmap_total_size;
	if ((rc = vmm_cpu_aspace_init(&resv_pa, &resv_va, &resv_sz))) {
		return rc;
	}
	if (resv_sz < bmap_total_size) {
		return VMM_EFAIL;
	}
	if ((hactrl.vapool_size <= resv_sz) || 
	    (hactrl.ram_size <= resv_sz)) {
		return VMM_EFAIL;
	}

	hactrl.vapool_bmap = (u32 *)resv_va;
	vmm_memset(hactrl.vapool_bmap, 0, sizeof(u32) * hactrl.vapool_bmap_len);
	max = ((hactrl.vapool_start + hactrl.vapool_size) / VMM_PAGE_SIZE);
	ite = ((vmm_code_vaddr() - hactrl.vapool_start) / VMM_PAGE_SIZE);
	last = ite + (vmm_code_size() / VMM_PAGE_SIZE);
	for ( ; (ite < last) && (ite < max); ite++) {
		hactrl.vapool_bmap[ite >> 5] |= (0x1 << (31 - (ite & 0x1F)));
		hactrl.vapool_bmap_free--;
	}
	ite = ((resv_va - hactrl.vapool_start) / VMM_PAGE_SIZE);
	last = ite + (resv_sz / VMM_PAGE_SIZE);
	for ( ; (ite < last) && (ite < max); ite++) {
		hactrl.vapool_bmap[ite >> 5] |= (0x1 << (31 - (ite & 0x1F)));
		hactrl.vapool_bmap_free--;
	}

	hactrl.ram_bmap = &hactrl.vapool_bmap[hactrl.vapool_bmap_len];
	vmm_memset(hactrl.ram_bmap, 0, sizeof(u32) * hactrl.ram_bmap_len);
	max = ((hactrl.ram_start + hactrl.ram_size) / VMM_PAGE_SIZE);
	ite = ((vmm_code_paddr() - hactrl.ram_start) / VMM_PAGE_SIZE);
	last = ite + (vmm_code_size() / VMM_PAGE_SIZE);
	for ( ; (ite < last) && (ite < max); ite++) {
		hactrl.ram_bmap[ite >> 5] |= (0x1 << (31 - (ite & 0x1F)));
		hactrl.ram_bmap_free--;
	}
	ite = ((resv_pa - hactrl.ram_start) / VMM_PAGE_SIZE);
	last = ite + (resv_sz / VMM_PAGE_SIZE);
	for ( ; (ite < last) && (ite < max); ite++) {
		hactrl.ram_bmap[ite >> 5] |= (0x1 << (31 - (ite & 0x1F)));
		hactrl.ram_bmap_free--;
	}

	return VMM_OK;
}

