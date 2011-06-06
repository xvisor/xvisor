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
 * @file cpu_host_aspace.c
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief CPU specific source file for host virtual address space management.
 */

#include <vmm_string.h>
#include <vmm_cpu.h>
#include <cpu_mmu.h>

int vmm_cpu_aspace_init(void)
{
	/* We don't need to do any thing here 
	 * everything will be done by cpu_mmu_init() 
	 */
	return cpu_mmu_init();
}

int vmm_cpu_iomap(virtual_addr_t va, virtual_size_t sz, physical_addr_t pa)
{
	cpu_page_t p;
	vmm_memset(&p, 0, sizeof(p));
	p.pa = pa;
	p.va = va;
	p.sz = sz;
	p.imp = 1;
	p.dom = TTBL_L1TBL_TTE_DOM_RESERVED;
	p.ap = TTBL_AP_SRW_U;
	p.xn = 1;
	p.c = 0;
	p.b = 0;
	return cpu_mmu_map_reserved_page(&p);
}

int vmm_cpu_iounmap(virtual_addr_t va, virtual_size_t sz)
{
	int rc;
	cpu_page_t p;
	rc = cpu_mmu_get_reserved_page(va, &p);
	if (rc) {
		return rc;
	}
	return cpu_mmu_unmap_reserved_page(&p);
}
