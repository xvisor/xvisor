/**
 * Copyright (c) 2018 Himanshu Chauhan.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file ept.c
 * @author Himanshu Chauhan (hchauhan@xvisor-x86.org)
 * @brief VMX Extended Page Table handling functions
 */
#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>
#include <vmm_guest_aspace.h>
#include <cpu_vm.h>
#include <cpu_inst_decode.h>
#include <cpu_features.h>
#include <cpu_mmu.h>
#include <cpu_pgtbl_helper.h>
#include <arch_guest_helper.h>
#include <vmm_devemu.h>
#include <vmm_manager.h>
#include <vmm_main.h>
#include <vm/ept.h>

static inline u32 ept_pml4_index(physical_addr_t gphys)
{
	if (gphys & (PHYS_ADDR_BIT_MASK))
		return ((u32)-1);

	return ((gphys >> 39) & 0x1fful);
}

static inline u32 ept_pdpt_index(physical_addr_t gphys)
{
	if (gphys & (PHYS_ADDR_BIT_MASK))
		return ((u32)-1);

	return ((gphys >> 30) & 0x1fful);
}

static inline u32 ept_pd_index(physical_addr_t gphys)
{
	if (gphys & (PHYS_ADDR_BIT_MASK))
		return ((u32)-1);

	return ((gphys >> 21) & 0x1fful);
}

static inline u32 ept_pt_index(physical_addr_t gphys)
{
	if (gphys & (PHYS_ADDR_BIT_MASK))
		return ((u32)-1);

	return ((gphys >> 12) & 0x1fful);
}

int ept_create_pte(struct vcpu_hw_context *context,
		   physical_addr_t gphys, physical_addr_t hphys,
		   size_t pg_size, u32 pg_prot)
{
	u32 pml4_index = ept_pml4_index(gphys);
	u32 pdpt_index = ept_pdpt_index(gphys);
	u32 pd_index = ept_pd_index(gphys);
	u32 pt_index = ept_pt_index(gphys);
	ept_pml4e_t *pml4e;
	ept_pdpte_t *pdpte;
	ept_pde_t   *pde;
	ept_pte_t   *pte;
	u64 *pml4 = (u64 *)context->n_cr3;
	physical_addr_t phys;
	virtual_addr_t virt;

	if (pml4_index == -1 || pdpt_index == -1
	    || pd_index == -1 || pt_index == -1) {
		VM_LOG(LVL_ERR,
		       "Page table index calculation failed. (gphys: 0x%lx)\n",
		       gphys);
		return VMM_EFAIL;
	}

	pml4e = (ept_pml4e_t *)(&pml4[pml4_index]);
	pml4e->val &= EPT_PROT_MASK;
	pml4e->val |= pg_prot;
	virt = get_free_page_for_pagemap(context, &phys);
	if (!virt) {
		VM_LOG(LVL_ERR, "System is out of guest page table memory\n");
		return VMM_ENOMEM;
	}
	pml4e->bits.pdpt_base = phys;

	pdpte = (ept_pdpte_t *)(&((u64 *)virt)[pdpt_index]);
	pdpte->val &= EPT_PROT_MASK;
	pdpte->val |= pg_prot;
	virt = get_free_page_for_pagemap(context, &phys);
	if (!virt) {
		VM_LOG(LVL_ERR, "System is out of guest page table memory\n");
		return VMM_ENOMEM;
	}
	if (pg_size == EPT_PAGE_SIZE_1G) {
		pdpte->pe.phys = hphys;
		pdpte->pe.mt = 6; /* write-back memory type */
		pdpte->pe.ign_pat = 1; /* ignore PAT type */
		pdpte->pe.is_page = 1;
		goto _done;
	} else {
		pdpte->te.pd_base = phys;
	}

	pde = (ept_pde_t *)(&((u64 *)virt)[pd_index]);
	pde->val &= EPT_PROT_MASK;
	pde->val |= pg_prot;
	virt = get_free_page_for_pagemap(context, &phys);
	if (!virt) {
		VM_LOG(LVL_ERR, "System is out of guest page table memory\n");
		return VMM_ENOMEM;
	}
	if (pg_size == EPT_PAGE_SIZE_2M) {
		pde->pe.phys = hphys;
		pde->pe.mt = 6;
		pde->pe.ign_pat = 1;
		pde->pe.is_page = 1;
		goto _done;
	} else {
		pde->te.pt_base = phys;
	}

	pte = (ept_pte_t *)(&((u64 *)virt)[pt_index]);
	pte->val &= EPT_PROT_MASK;
	pte->val |= pg_prot;
	pte->pe.phys = hphys;

 _done:
	return VMM_OK;
}

int setup_ept(struct vcpu_hw_context *context)
{
	physical_addr_t pml4_phys;
	eptp_t *eptp = (eptp_t *)&context->eptp;
	virtual_addr_t pml4 = get_free_page_for_pagemap(context, &pml4_phys);

	if (!pml4) {
		VM_LOG(LVL_ERR, "%s: Failed to allocate EPT page\n", __func__);
		return VMM_ENOMEM;
	}

	eptp->bits.mt = 6; /* Write back */
	eptp->bits.pgwl = 3; /* 4 page levels */
	eptp->bits.en_ad = 0;
	eptp->bits.pml4 = pml4_phys;

	context->n_cr3 = pml4;

	return VMM_OK;
}
