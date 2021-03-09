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
	gphys &= PHYS_ADDR_BIT_MASK;
	return ((gphys >> 39) & 0x1fful);
}

static inline u32 ept_pdpt_index(physical_addr_t gphys)
{
	gphys &= PHYS_ADDR_BIT_MASK;
	return ((gphys >> 30) & 0x1fful);
}

static inline u32 ept_pd_index(physical_addr_t gphys)
{
	gphys &= PHYS_ADDR_BIT_MASK;
	return ((gphys >> 21) & 0x1fful);
}

static inline u32 ept_pt_index(physical_addr_t gphys)
{
	gphys &= PHYS_ADDR_BIT_MASK;
	return ((gphys >> 12) & 0x1fful);
}

static inline void
invalidate_ept (int type, struct invept_desc *desc)
{
	/* Specifically not using exception table here.
	 * if feature is not present, it will unnecessary
	 * cause context switch. More expensive */
	if (likely(cpu_has_vmx_invept)) {
		/* most modern CPUs will have this */
		if (unlikely(type == INVEPT_ALL_CONTEXT
		    && !cpu_has_vmx_ept_invept_all_context)) {
			    VM_LOG(LVL_INFO, "EPT all context flush not supported\n");
			    return;
		    }
		if (unlikely(type == INVEPT_SINGLE_CONTEXT
			     && !cpu_has_vmx_ept_invept_single_context)) {
			    VM_LOG(LVL_INFO, "EPT single context flush not supported\n");
			    return;
		    }
		asm volatile("invept (%0), %1\n\t"
		     ::"D"(type), "S"(desc)
		     :"memory", "cc");
	} else {
		VM_LOG(LVL_INFO, "INVEPT instruction is not supported by CPU\n");
	}
}

enum ept_level {
	EPT_LEVEL_PTE,
	EPT_LEVEL_PDE,
	EPT_LEVEL_PDPTE,
	EPT_LEVEL_PML4E,
};

static void decode_ept_entry(enum ept_level level, void *entry,
			    physical_addr_t *paddr, u32 *pg_prot)
{
	ept_pml4e_t *pml4e;
	ept_pdpte_t *pdpte;
	ept_pde_t   *pde;
	ept_pte_t   *pte;

	switch(level) {
		case EPT_LEVEL_PML4E:
		pml4e = (ept_pml4e_t *)entry;
		*paddr = (pml4e->bits.pdpt_base << 12);
		*pg_prot = (pml4e->val & ~EPT_PROT_MASK);
		break;

		case EPT_LEVEL_PDPTE:
		pdpte = (ept_pdpte_t *)entry;
		*paddr = (pdpte->te.pd_base << 12);
		*pg_prot = (pdpte->val & ~EPT_PROT_MASK);
		break;

		case EPT_LEVEL_PDE:
		pde = (ept_pde_t *)entry;
		*paddr = (pde->te.pt_base << 12);
		*pg_prot = (pde->val & ~EPT_PROT_MASK);
		break;

		case EPT_LEVEL_PTE:
		pte = (ept_pte_t *)entry;
		*paddr = (pte->pe.phys << 12);
		*pg_prot = (pte->val & ~EPT_PROT_MASK);
		break;
	}
}

int ept_create_pte_map(struct vcpu_hw_context *context,
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
	u32 e_pg_prot;
	physical_addr_t e_phys;
	int rc = 0;
	struct invept_desc id;

	VM_LOG(LVL_DEBUG, "pml4: 0x%"PRIx32" pdpt: 0x%"PRIx32" pd: 0x%"PRIx32" pt: 0x%"PRIx32"\n",
	       pml4_index, pdpt_index, pd_index, pt_index);

	pml4e = (ept_pml4e_t *)(&pml4[pml4_index]);
	decode_ept_entry(EPT_LEVEL_PML4E, (void *)pml4e, &e_phys, &e_pg_prot);
	if (!e_pg_prot) {
		pml4e->val = 0;
		pml4e->val &= EPT_PROT_MASK;
		pml4e->val |= pg_prot;
		virt = get_free_page_for_pagemap(context, &phys);
		if (!virt) {
			VM_LOG(LVL_ERR, "System is out of guest page table memory\n");
			rc =  VMM_ENOMEM;
			goto _done;
		}
		VM_LOG(LVL_DEBUG, "New PDPT Page at 0x%"PRIx64" (Phys: 0x%"PRIx64") for PML4 Index %d.\n",
		       virt, phys, pml4_index);
		memset((void *)virt, 0, PAGE_SIZE);
		pml4e->bits.pdpt_base = EPT_PHYS_4KB_PFN(phys);
	} else {
		if (vmm_host_pa2va(e_phys, &virt) != VMM_OK) {
			VM_LOG(LVL_ERR, "Couldn't map PDPTE physical 0x%"PRIx64" to virtual\n",
			       e_phys);
			rc = VMM_ENOENT;
			goto _done;
		}
		VM_LOG(LVL_DEBUG, "Found PDPT Page at 0x%"PRIx64" (phys: 0x%"PRIx64") for PML4 Index: %d\n",
		       virt, e_phys, pml4_index);
	}
	VM_LOG(LVL_DEBUG, "%s: PML4E: 0x%"PRIx64"\n", __func__, pml4e->val);

	phys = e_phys = e_pg_prot = 0;
	pdpte = (ept_pdpte_t *)(&((u64 *)virt)[pdpt_index]);
	VM_LOG(LVL_DEBUG, "%s: PDPTE: 0x%"PRIx64" (PDPT Index: %d)\n", __func__, pdpte->val, pdpt_index);
	decode_ept_entry(EPT_LEVEL_PDPTE, (void *)pdpte, &e_phys, &e_pg_prot);

	/*
	 * if page protection bits are set and this is marked as a resident
	 * page, its an error. Caller is trying to set a map without freeing
	 * up the other one.
	 */
	if (pdpte->pe.is_page) {
		VM_LOG(LVL_DEBUG, "PDPTE is page\n");
		/* this is marked as 1GB page and new mapping wants otherwise
		 * then its a problem. Caller didn't free this mapping prior
		 * to calling this function */
		if (pg_size != EPT_PAGE_SIZE_1G) {
			VM_LOG(LVL_DEBUG, "New page size is not 1G (0x%"PRIx64"). Delete existing entry first.\n", pg_size);
			rc = VMM_EBUSY;
			goto _done;
		}

		/* caller is trying to create same mapping? */
		if (e_phys == hphys) {
			if (pg_prot != e_pg_prot) {
				pdpte->val |= pg_prot;
				/* need to invalidate ept as prot bits changed */
				goto _invalidate_ept;
			} else {
				/* no change, same as existing mapping */
				rc = VMM_OK;
				goto _done;
			}
		} else {
			/* existing physical is not same as new one. flag as error.
			 * caller should have unmapped this mapping first */
			rc = VMM_EBUSY;
			goto _done;
		}
	}

	if (pg_size == EPT_PAGE_SIZE_1G) {
		VM_LOG(LVL_DEBUG, "Creating map of 1G page at pdpt index: %d\n", pdpt_index);
		pdpte->val = 0;
		pdpte->val &= EPT_PROT_MASK;
		pdpte->val |= pg_prot;
		pdpte->pe.phys = EPT_PHYS_1GB_PFN(hphys);
		pdpte->pe.mt = 6; /* write-back memory type */
		pdpte->pe.ign_pat = 1; /* ignore PAT type */
		pdpte->pe.is_page = 1;
		VM_LOG(LVL_INFO, "New PDPT Entry: 0x%"PRIx64"\n", pdpte->val);
		rc = VMM_OK;
		/* new entry. Invalidate EPT */
		goto _invalidate_ept;
	} else { /* not a 1G page */
		VM_LOG(LVL_DEBUG, "PDPTE doesn't point to 1G page. Looking for PDE\n");
		if (!e_pg_prot) { /* if the page is not currently set */
			VM_LOG(LVL_DEBUG, "PDE page protection not set. Creating new one\n");
			virt = get_free_page_for_pagemap(context, &phys);
			/* allocate a new PDPTE page */
			if (!virt) {
				VM_LOG(LVL_ERR, "System is out of guest page table memory\n");
				rc = VMM_ENOMEM;
				goto _done;
			}
			memset((void *)virt, 0, PAGE_SIZE);
			pdpte->val = 0;
			pdpte->te.pd_base = EPT_PHYS_4KB_PFN(phys);
			pdpte->val &= EPT_PROT_MASK;
			pdpte->val |= pg_prot;
			VM_LOG(LVL_INFO, "New PD Page at 0x%"PRIx64" (Phys: 0x%"PRIx64")\n", virt, phys);
		} else { /* page is already allocated, a mapping in locality exists */
			if (vmm_host_pa2va(e_phys, &virt) != VMM_OK) {
				VM_LOG(LVL_ERR, "Couldn't map PDE physical 0x%"PRIx64" to virtual\n",
				       e_phys);
				rc = VMM_ENOENT;
				goto _done;
			}
			VM_LOG(LVL_DEBUG, "Found PDE at virtual address 0x%"PRIx64"\n", virt);
		}
	}
	VM_LOG(LVL_DEBUG, "%s: PDPTE: 0x%"PRIx64"\n", __func__, pdpte->val);

	phys = e_phys = e_pg_prot = 0;
	pde = (ept_pde_t *)(&((u64 *)virt)[pd_index]);
	VM_LOG(LVL_DEBUG, "PDPTE Entry at index %d = 0x%"PRIx64"\n", pd_index, pde->val);
	decode_ept_entry(EPT_LEVEL_PDE, (void *)pde, &e_phys, &e_pg_prot);

	if (pde->pe.is_page) {
		VM_LOG(LVL_INFO, "PDE is a 2MB Page!\n");
		/* this is marked as 1GB page and new mapping wants otherwise
		 * then its a problem. Caller didn't free this mapping prior
		 * to calling this function */
		if (pg_size != EPT_PAGE_SIZE_2M) {
			VM_LOG(LVL_DEBUG, "New page is not 2M. Delete previous entry first.\n");
			rc = VMM_EBUSY;
			goto _done;
		}

		/* caller is trying to create same mapping? */
		if (e_phys == hphys) {
			VM_LOG(LVL_DEBUG, "Found same physical addres at pd index: %d\n", pd_index);
			if (pg_prot != e_pg_prot) {
				VM_LOG(LVL_DEBUG, "PG prot are not same. Old: 0x%"PRIx32" New: 0x%"PRIx32"\n",
				       e_pg_prot, pg_prot);
				pde->val |= pg_prot;
				rc = VMM_OK;
				/* pgprot changed, invalidate ept */
				goto _invalidate_ept;
			} else {
				VM_LOG(LVL_DEBUG, "No change in page table entry.\n");
				/* no change, same as existing mapping */
				rc = VMM_OK;
				goto _done;
			}
		} else {
			VM_LOG(LVL_DEBUG, "pd index %d is busy. Val: 0x%"PRIx64"\n", pd_index, pde->val);
			/* existing physical is not same as new one. flag as error.
			 * caller should have unmapped this mapping first */
			rc = VMM_EBUSY;
			goto _done;
		}
	}

	/* not a 2MB page, is caller trying to create a 2MB page? */
	if (pg_size == EPT_PAGE_SIZE_2M) {
		VM_LOG(LVL_DEBUG, "Ask is to create 2MB page\n");
		pdpte->val = 0;
		pdpte->val &= EPT_PROT_MASK;
		pdpte->val |= pg_prot;
		pdpte->pe.phys = EPT_PHYS_2MB_PFN(hphys);
		pdpte->pe.mt = 6; /* write-back memory type */
		pdpte->pe.ign_pat = 1; /* ignore PAT type */
		pdpte->pe.is_page = 1;
		VM_LOG(LVL_DEBUG, "New 2MB page. PDE Value: 0x%"PRIx64" at index: %d.\n", pdpte->val, pd_index);
		rc = VMM_OK;
		goto _invalidate_ept;
	} else {
		/* Ok. So this is PDE. Lets find PTE now. */
		if (!e_pg_prot) { /* page for PTE is not currently set */
			VM_LOG(LVL_INFO, "Page protection bits not set in PTE page. Creating new one.\n");
			virt = get_free_page_for_pagemap(context, &phys);
			/* allocate a new PTE page */
			if (!virt) {
				VM_LOG(LVL_ERR, "System is out of guest page table memory\n");
				rc = VMM_ENOMEM;
				goto _done;
			}
			memset((void *)virt, 0, PAGE_SIZE);
			pde->val = 0;
			pde->te.pt_base = EPT_PHYS_4KB_PFN(phys);
			pde->val &= EPT_PROT_MASK;
			pde->val |= pg_prot;
			VM_LOG(LVL_DEBUG, "New PT page at 0x%"PRIx64" (Phys: 0x%"PRIx64")\n",
			       virt, phys);
		} else { /* page is already allocated, a mapping in locality exists */
			if (vmm_host_pa2va(e_phys, &virt) != VMM_OK) {
				VM_LOG(LVL_ERR, "Couldn't map PDE physical 0x%"PRIx64" to virtual\n",
				       e_phys);
				rc = VMM_ENOENT;
				goto _done;
			}
			VM_LOG(LVL_DEBUG, "Found PT at virt 0x%"PRIx64"\n", virt);
		}
	}
	VM_LOG(LVL_DEBUG, "%s: PDE: 0x%"PRIx64"\n", __func__, pde->val);

	e_phys = e_pg_prot = 0;
	pte = (ept_pte_t *)(&((u64 *)virt)[pt_index]);
	VM_LOG(LVL_DEBUG, "PT Entry 0x%"PRIx64" at index: %d\n", pte->val, pt_index);
	decode_ept_entry(EPT_LEVEL_PTE, (void *)pte, &e_phys, &e_pg_prot);
	if (e_pg_prot) { /* mapping exists */
		VM_LOG(LVL_DEBUG, "Page mapping exists: current pgprot: 0x%"PRIx32"\n", e_pg_prot);
		if (e_phys == hphys) {
			VM_LOG(LVL_DEBUG, "Existing physical and asked are same. (e_phys: 0x%"PRIx64" h_phys: 0x%"PRIx64")\n", e_phys, hphys);
			if (e_pg_prot == pg_prot) { /* same mapping */
				VM_LOG(LVL_DEBUG, "Same PG prot: old: 0x%"PRIx32" new: 0x%"PRIx32"\n", e_pg_prot, pg_prot);
				rc = VMM_OK;
				goto _done; /* no change */
			}
			pte->val = 0;
			rc = VMM_OK;
			pte->val |= pg_prot;
			goto _invalidate_ept;
		} else {
			VM_LOG(LVL_DEBUG, "Existing PTE entry found at index: %d but with phys: 0x%"PRIx64" (new: 0x%"PRIx64")\n",
			       pt_index, e_phys, hphys);
			rc = VMM_EBUSY;
			goto _done;
		}
	} else {
		VM_LOG(LVL_DEBUG, "No page protection bits set in PTE. Creating new one\n");
		pte->val = 0;
		pte->val &= EPT_PROT_MASK;
		pte->val |= pg_prot;
		pte->pe.mt = 6;
		pte->pe.phys = EPT_PHYS_4KB_PFN(hphys);
		rc = VMM_OK;
		VM_LOG(LVL_DEBUG, "%s: PTE: 0x%"PRIx64" at index %d\n", __func__, pte->val, pt_index);
		goto _invalidate_ept;
	}

 _invalidate_ept:
	VM_LOG(LVL_DEBUG, "Invalidating EPT\n");

	id.eptp = context->eptp;
	invalidate_ept(INVEPT_SINGLE_CONTEXT, &id);
 _done:
	return rc;
}

int setup_ept(struct vcpu_hw_context *context)
{
	physical_addr_t pml4_phys;
	eptp_t *eptp = (eptp_t *)&context->eptp;
	virtual_addr_t pml4 = get_free_page_for_pagemap(context, &pml4_phys);

	VM_LOG(LVL_INFO, "%s: PML4 vaddr: 0x%016lx paddr: 0x%016lx\n",
	       __func__, pml4, pml4_phys);

	if (!pml4) {
		VM_LOG(LVL_ERR, "%s: Failed to allocate EPT page\n", __func__);
		return VMM_ENOMEM;

	}

	/* most of the reserved bits want zeros */
	memset((void *)pml4, 0, PAGE_SIZE);

	eptp->val = 0;
	eptp->bits.mt = (vmx_ept_vpid_cap & (0x01UL << 8) ? 0 /* UC */
		 : (vmx_ept_vpid_cap & (0x1UL << 14)) ? 6 /* WB */
		 : 6);

	eptp->bits.pgwl = 3; /* 4 page levels */
	eptp->bits.en_ad = 0;
	eptp->bits.pml4 = EPT_PHYS_4KB_PFN(pml4_phys);

	VM_LOG(LVL_DEBUG, "%s: EPTP: 0x%16lx (0x%16lx)\n", __func__, eptp->val, context->eptp);

	context->n_cr3 = pml4;

	/* Mark the reset vector as no r/w/e so that guest faults at first execution.
	 * This is required because the bios hasn't been mapped into guest. Only when
	 * it will fault at reset, we will map the bios on the fly. */
	ept_create_pte_map(context, 0xF000ULL, 0, PAGE_SIZE, 0);

	return VMM_OK;
}
