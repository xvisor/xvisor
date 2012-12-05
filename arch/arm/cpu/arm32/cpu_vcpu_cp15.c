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
 * @file cpu_vcpu_cp15.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief VCPU CP15 Emulation
 * @details This source file implements CP15 coprocessor for each VCPU.
 *
 * The Translation table walk and CP15 register read/write has been 
 * largely adapted from QEMU 0.14.xx targe-arm/helper.c source file
 * which is licensed under GPL.
 */

#include <vmm_heap.h>
#include <vmm_error.h>
#include <vmm_scheduler.h>
#include <vmm_guest_aspace.h>
#include <vmm_vcpu_irq.h>
#include <arch_barrier.h>
#include <libs/stringlib.h>
#include <emulate_arm.h>
#include <emulate_thumb.h>
#include <cpu_mmu.h>
#include <cpu_cache.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_helper.h>
#include <cpu_vcpu_cp15.h>

/* Update Virtual TLB */
static int cpu_vcpu_cp15_vtlb_update(struct vmm_vcpu *vcpu, 
				     struct cpu_page *p, 
				     u32 domain,
				     bool is_virtual)
{
	int rc;
	u32 entry, victim, zone;
	struct arm_vtlb_entry *e = NULL;

	/* Find appropriate zone */
	if (is_virtual) {
		zone = CPU_VCPU_VTLB_ZONE_V;
	} else if (p->ng) {
		zone = CPU_VCPU_VTLB_ZONE_NG;
	} else {
		zone = CPU_VCPU_VTLB_ZONE_G;
	}

	/* Find out next victim entry from TLB */
	victim = arm_priv(vcpu)->cp15.vtlb.victim[zone];
	entry = victim + CPU_VCPU_VTLB_ZONE_START(zone);
	e = &arm_priv(vcpu)->cp15.vtlb.table[entry];
	if (e->valid) {
		/* Remove valid victim page from L1 Page Table */
		rc = cpu_mmu_unmap_page(arm_priv(vcpu)->cp15.l1, &e->page);
		if (rc) {
			return rc;
		}
		e->valid = 0;
		e->ng = 0;
		e->dom = 0;
	}

	/* Save original domain */
	e->dom = domain;

	/* Ensure pages for normal vcpu are non-global */
	e->ng = p->ng;
	p->ng = 1;

#ifndef CONFIG_SMP
	/* Ensure non-shareable pages for normal vcpu when running on UP host.
	 * This will force usage of local monitors in case of UP host.
         */
	p->s = 0;
#endif

	/* Add victim page to L1 page table */
	if ((rc = cpu_mmu_map_page(arm_priv(vcpu)->cp15.l1, p))) {
		return rc;
	}

	/* Mark entry as valid */
	memcpy(&e->page, p, sizeof(struct cpu_page));
	e->valid = 1;

	/* Point to next victim of TLB line */
	victim = victim + 1;
	if (CPU_VCPU_VTLB_ZONE_LEN(zone) <= victim) {
		victim = 0;
	}
	arm_priv(vcpu)->cp15.vtlb.victim[zone] = victim;

	return VMM_OK;
}

int cpu_vcpu_cp15_vtlb_flush(struct vmm_vcpu *vcpu)
{
	int rc;
	u32 vtlb, zone;
	struct arm_vtlb_entry *e;

	for (vtlb = 0; vtlb < CPU_VCPU_VTLB_ENTRY_COUNT; vtlb++) {
		if (arm_priv(vcpu)->cp15.vtlb.table[vtlb].valid) {
			e = &arm_priv(vcpu)->cp15.vtlb.table[vtlb];
			rc = cpu_mmu_unmap_page(arm_priv(vcpu)->cp15.l1,
						&e->page);
			if (rc) {
				return rc;
			}
			e->valid = 0;
			e->ng = 0;
			e->dom = 0;
		}
	}

	for (zone = 0; zone < CPU_VCPU_VTLB_ZONE_COUNT; zone++) {
		arm_priv(vcpu)->cp15.vtlb.victim[zone] = 0;
	}

	return VMM_OK;
}


int cpu_vcpu_cp15_vtlb_flush_va(struct vmm_vcpu *vcpu, virtual_addr_t va)
{
	int rc;
	u32 vtlb;
	struct arm_vtlb_entry *e;

	for (vtlb = 0; vtlb < CPU_VCPU_VTLB_ENTRY_COUNT; vtlb++) {
		if (arm_priv(vcpu)->cp15.vtlb.table[vtlb].valid) {
			e = &arm_priv(vcpu)->cp15.vtlb.table[vtlb];
			if (e->page.va <= va && va < (e->page.va + e->page.sz)) {
				rc = cpu_mmu_unmap_page(arm_priv(vcpu)->cp15.l1,
							&e->page);
				if (rc) {
					return rc;
				}
				e->valid = 0;
				e->ng = 0;
				e->dom = 0;
				break;
			}
		}
	}

	return VMM_OK;
}

int cpu_vcpu_cp15_vtlb_flush_ng(struct vmm_vcpu * vcpu)
{
	int rc;
	u32 vtlb, vtlb_last;
	struct arm_vtlb_entry * e;

	vtlb = CPU_VCPU_VTLB_ZONE_START(CPU_VCPU_VTLB_ZONE_NG);
	vtlb_last = vtlb + CPU_VCPU_VTLB_ZONE_LEN(CPU_VCPU_VTLB_ZONE_NG);
	while (vtlb < vtlb_last) {
		if (arm_priv(vcpu)->cp15.vtlb.table[vtlb].valid) {
			e = &arm_priv(vcpu)->cp15.vtlb.table[vtlb];
			if (e->ng) {
				rc = cpu_mmu_unmap_page(arm_priv(vcpu)->cp15.l1, 
						&e->page);
				if (rc) {
					return rc;
				}
				e->valid = 0;
				e->ng = 0;
				e->dom = 0;
			}
		}
		vtlb++;
	}

	return VMM_OK;
}

int cpu_vcpu_cp15_vtlb_flush_domain(struct vmm_vcpu * vcpu, 
				    u32 dacr_xor_diff)
{
	int rc;
	u32 vtlb;
	struct arm_vtlb_entry * e;

	for (vtlb = 0; vtlb < CPU_VCPU_VTLB_ENTRY_COUNT; vtlb++) {
		if (arm_priv(vcpu)->cp15.vtlb.table[vtlb].valid) {
			e = &arm_priv(vcpu)->cp15.vtlb.table[vtlb];
			if ((dacr_xor_diff >> ((e->dom & 0xF) << 1)) & 0x3) {
				rc = cpu_mmu_unmap_page(arm_priv(vcpu)->cp15.l1,
						&e->page);
				if (rc) {
					return rc;
				}
				e->valid = 0;
				e->ng = 0;
				e->dom = 0;
			}
		}
	}

	return VMM_OK;
}


enum cpu_vcpu_cp15_access_permission {
	CP15_ACCESS_DENIED = 0,
	CP15_ACCESS_GRANTED = 1
};

/* Check section/page access permissions.
 * Returns 1 - permitted, 0 - not-permitted
 */
static inline enum cpu_vcpu_cp15_access_permission check_ap(struct vmm_vcpu *vcpu,
			   int ap, int access_type, int is_user)
{
	switch (ap) {
	case TTBL_AP_S_U:
		if (access_type == CP15_ACCESS_WRITE) {
			return CP15_ACCESS_DENIED;
		}

		switch (arm_priv(vcpu)->cp15.c1_sctlr & (SCTLR_R_MASK | SCTLR_S_MASK))
		{
		case SCTLR_S_MASK:
			if (is_user) {
				return CP15_ACCESS_DENIED;
			}

			return CP15_ACCESS_GRANTED;
			break;
		case SCTLR_R_MASK:
			return CP15_ACCESS_GRANTED;
			break;
		default:
			return CP15_ACCESS_DENIED;
			break;
		}
		break;
	case TTBL_AP_SRW_U:
		if (is_user) {
			return CP15_ACCESS_DENIED;
		}

		return CP15_ACCESS_GRANTED;
		break;
	case TTBL_AP_SRW_UR:
		if (is_user) {
			return (access_type != CP15_ACCESS_WRITE) ? CP15_ACCESS_GRANTED : CP15_ACCESS_DENIED;
		}

		return CP15_ACCESS_GRANTED;
		break;
	case TTBL_AP_SRW_URW:
		return CP15_ACCESS_GRANTED;
		break;
	case TTBL_AP_SR_U:
		if (is_user) {
			return CP15_ACCESS_DENIED;
		}

		return (access_type != CP15_ACCESS_WRITE) ? CP15_ACCESS_GRANTED : CP15_ACCESS_DENIED;
		break;
	case TTBL_AP_SR_UR_DEPRECATED:
		return (access_type != CP15_ACCESS_WRITE) ? CP15_ACCESS_GRANTED : CP15_ACCESS_DENIED;
		break;
	case TTBL_AP_SR_UR:
		if (!arm_feature(vcpu, ARM_FEATURE_V6K)) {
			return CP15_ACCESS_DENIED;
		}

		return (access_type != CP15_ACCESS_WRITE) ? CP15_ACCESS_GRANTED : CP15_ACCESS_DENIED;
		break;
	default:
		return CP15_ACCESS_DENIED;
		break;
	};

	return CP15_ACCESS_DENIED;
}

static physical_addr_t get_level1_table_pa(struct vmm_vcpu *vcpu,
					   virtual_addr_t va)
{
	if (va & arm_priv(vcpu)->cp15.c2_mask) {
		return arm_priv(vcpu)->cp15.c2_base1 & 0xffffc000;
	}

	return arm_priv(vcpu)->cp15.c2_base0 & arm_priv(vcpu)->cp15.c2_base_mask;
}

static int ttbl_walk_v6(struct vmm_vcpu *vcpu, virtual_addr_t va, 
		int access_type, int is_user, struct cpu_page *pg, u32 * fs)
{
	physical_addr_t table;
	int type, domain;
	u32 desc;

	pg->va = va;

	/* Pagetable walk.  */
	/* Lookup l1 descriptor.  */
	table = get_level1_table_pa(vcpu, va);

	table |= (va >> 18) & 0x3ffc;
	if (!vmm_guest_physical_read(vcpu->guest, 
				     table, &desc, sizeof(desc))) {
		return VMM_EFAIL;
	}
	type = (desc & 3);
	if (type == 0) {
		/* Section translation fault.  */
		*fs = 5;
		pg->dom = 0;
		goto do_fault;
	} else if (type == 2 && (desc & (1 << 18))) {
		/* Supersection.  */
		pg->dom = 0;
	} else {
		/* Section or page.  */
		pg->dom = (desc >> 5) & 0xF;
	}
	domain = (arm_priv(vcpu)->cp15.c3 >> (pg->dom << 1)) & 3;
	if (domain == 0 || domain == 2) {
		if (type == 2)
			*fs = 9;	/* Section domain fault.  */
		else
			*fs = 11;	/* Page domain fault.  */
		goto do_fault;
	}
	if (type == 2) {
		if (desc & (1 << 18)) {
			/* Supersection.  */
			pg->pa = (desc & 0xff000000) | (va & 0x00ffffff);
			pg->sz = 0x1000000;
		} else {
			/* Section.  */
			pg->pa = (desc & 0xfff00000) | (va & 0x000fffff);
			pg->sz = 0x100000;
		}
		pg->ng = (desc >> 17) & 0x1;
		pg->s = (desc >> 16) & 0x1;
		pg->tex = (desc >> 12) & 0x7;
		pg->ap = ((desc >> 10) & 0x3) | ((desc >> 13) & 0x4);
		pg->xn = (desc >> 4) & 0x1;
		pg->c = (desc >> 3) & 0x1;
		pg->b = (desc >> 2) & 0x1;
		*fs = 13;
	} else {
		/* Lookup l2 entry.  */
		table = (desc & 0xfffffc00);
		table |= ((va >> 10) & 0x3fc);
		if (!vmm_guest_physical_read(vcpu->guest, 
					     table, &desc, sizeof(desc))) {
			return VMM_EFAIL;
		}
		switch (desc & 3) {
		case 0:	/* Page translation fault.  */
			*fs = 7;
			goto do_fault;
		case 1:	/* 64k page.  */
			pg->pa = (desc & 0xffff0000) | (va & 0xffff);
			pg->sz = 0x10000;
			pg->xn = (desc >> 15) & 0x1;
			pg->tex = (desc >> 12) & 0x7;
			break;
		case 2:
		case 3:	/* 4k page.  */
			pg->pa = (desc & 0xfffff000) | (va & 0xfff);
			pg->sz = 0x1000;
			pg->tex = (desc >> 6) & 0x7;
			pg->xn = desc & 0x1;
			break;
		default:
			/* Never happens, but compiler isn't 
			 * smart enough to tell.
			 */
			return VMM_EFAIL;
		}
		pg->ng = (desc >> 11) & 0x1;
		pg->s = (desc >> 10) & 0x1;
		pg->ap = ((desc >> 4) & 0x3) | ((desc >> 7) & 0x4);
		pg->c = (desc >> 3) & 0x1;
		pg->b = (desc >> 2) & 0x1;
		*fs = 15;
	}
	if (domain == 3) {
		/* Page permission not to be checked so, 
		 * give full access using access permissions.
		 */
		pg->ap = TTBL_AP_SRW_URW;
		pg->xn = 0;
	} else {
		if (pg->xn && access_type == 2)
			goto do_fault;
		/* The simplified model uses AP[0] as an access control bit.  */
		if ((arm_priv(vcpu)->cp15.c1_sctlr & (1 << 29))
		    && (pg->ap & 1) == 0) {
			/* Access flag fault.  */
			*fs = (*fs == 15) ? 6 : 3;
			goto do_fault;
		}
		if (check_ap(vcpu, pg->ap, access_type, is_user) == CP15_ACCESS_DENIED) {
			/* Access permission fault.  */
			goto do_fault;
		}
	}
	return VMM_OK;
 do_fault:
	return VMM_EFAIL;
}

static int ttbl_walk_v5(struct vmm_vcpu *vcpu, virtual_addr_t va, 
		int access_type, int is_user, struct cpu_page *pg, u32 * fs)
{
	physical_addr_t table;
	int type, domain;
	u32 desc;

	pg->va = va;

	/* Pagetable walk.  */
	/* Lookup l1 descriptor.  */
	table = get_level1_table_pa(vcpu, va);

	/* compute the L1 descripto physical location */
	table |= (va >> 18) & 0x3ffc;

	/* get it */
	if (!vmm_guest_physical_read(vcpu->guest, 
				     table, &desc, sizeof(desc))) {
		goto do_fault;
	}

	/* extract type */
	type = (desc & TTBL_L1TBL_TTE_TYPE_MASK);

	/* retreive domain info */
	pg->dom = (desc & TTBL_L1TBL_TTE_DOM_MASK) >> TTBL_L1TBL_TTE_DOM_SHIFT;
	domain = (arm_priv(vcpu)->cp15.c3 >> (pg->dom << 1)) & 3;

	switch (type) {
	case TTBL_L1TBL_TTE_TYPE_SECTION: /* 1Mb section.  */
		if (domain == 0 || domain == 2) {
			/* Section domain fault.  */
			*fs = DFSR_FS_DOMAIN_FAULT_SECTION;
			goto do_fault;
			break;
		}

		/* compute physical address */
		pg->pa = (desc & ~TTBL_L1TBL_SECTION_PAGE_MASK) | (va & TTBL_L1TBL_SECTION_PAGE_MASK);
		/* extract access protection */
		pg->ap = (desc & TTBL_L1TBL_TTE_AP_MASK) >> TTBL_L1TBL_TTE_AP_SHIFT;
		/* Set Section size */
		pg->sz = TTBL_L1TBL_SECTION_PAGE_SIZE;
		pg->c = (desc & TTBL_L1TBL_TTE_C_MASK) >> TTBL_L1TBL_TTE_C_SHIFT;
		pg->b = (desc & TTBL_L1TBL_TTE_B_MASK) >> TTBL_L1TBL_TTE_B_SHIFT;

		*fs = DFSR_FS_PERM_FAULT_SECTION;
		break;
	case TTBL_L1TBL_TTE_TYPE_COARSE_L2TBL: /* Coarse pagetable. */
		if (domain == 0 || domain == 2) {
			/* Page domain fault.  */
			*fs = DFSR_FS_DOMAIN_FAULT_PAGE;
			goto do_fault;
			break;
		}

		/* compute L2 table physical address */
		table = desc & 0xfffffc00;

		/* compute L2 desc physical address */
		table |= ((va >> 10) & 0x3fc);

		/* get it */
		if (!vmm_guest_physical_read(vcpu->guest, 
				     table, &desc, sizeof(desc))) {
			goto do_fault;
		}

		switch (desc & TTBL_L2TBL_TTE_TYPE_MASK) {
		case TTBL_L2TBL_TTE_TYPE_LARGE:	/* 64k page.  */
			pg->pa = (desc & 0xffff0000) | (va & 0xffff);
			pg->ap = (desc >> (4 + ((va >> 13) & 6))) & 3;
			pg->sz = TTBL_L2TBL_LARGE_PAGE_SIZE;
			*fs = DFSR_FS_PERM_FAULT_PAGE;
			break;
		case TTBL_L2TBL_TTE_TYPE_SMALL:	/* 4k page.  */
			pg->pa = (desc & 0xfffff000) | (va & 0xfff);
			pg->ap = (desc >> (4 + ((va >> 13) & 6))) & 3;
			pg->sz = TTBL_L2TBL_SMALL_PAGE_SIZE;
			*fs = DFSR_FS_PERM_FAULT_PAGE;
			break;
		case TTBL_L2TBL_TTE_TYPE_FAULT:
		default:
			/* Page translation fault.  */
			*fs = DFSR_FS_TRANS_FAULT_PAGE;
			goto do_fault;
			break;
		}

		pg->c = (desc & TTBL_L2TBL_TTE_C_MASK) >> TTBL_L2TBL_TTE_C_SHIFT;
		pg->b = (desc & TTBL_L2TBL_TTE_B_MASK) >> TTBL_L2TBL_TTE_B_SHIFT;

		break;
	case TTBL_L1TBL_TTE_TYPE_FINE_L2TBL: /* Fine pagetable. */
		if (domain == 0 || domain == 2) {
			/* Page domain fault.  */
			*fs = DFSR_FS_DOMAIN_FAULT_PAGE;
			goto do_fault;
			break;
		}

		table = (desc & 0xfffff000);
		table |= ((va >> 8) & 0xffc);

		if (!vmm_guest_physical_read(vcpu->guest, 
				     table, &desc, sizeof(desc))) {
			goto do_fault;
		}

		switch (desc & TTBL_L2TBL_TTE_TYPE_MASK) {
		case TTBL_L2TBL_TTE_TYPE_LARGE:	/* 64k page.  */
			pg->pa = (desc & 0xffff0000) | (va & 0xffff);
			pg->ap = (desc >> (4 + ((va >> 13) & 6))) & 3;
			pg->sz = TTBL_L2TBL_LARGE_PAGE_SIZE;
			*fs = DFSR_FS_PERM_FAULT_PAGE;
			break;
		case TTBL_L2TBL_TTE_TYPE_SMALL:	/* 4k page.  */
			pg->pa = (desc & 0xfffff000) | (va & 0xfff);
			pg->ap = (desc >> (4 + ((va >> 13) & 6))) & 3;
			pg->sz = TTBL_L2TBL_SMALL_PAGE_SIZE;
			*fs = DFSR_FS_PERM_FAULT_PAGE;
			break;
		case TTBL_L2TBL_TTE_TYPE_TINY:	/* 1k page.  */
			pg->pa = (desc & 0xfffffc00) | (va & 0x3ff);
			pg->ap = (desc >> 4) & 3;
			pg->sz = TTBL_L2TBL_TINY_PAGE_SIZE;
			*fs = DFSR_FS_PERM_FAULT_PAGE;
			break;
		case TTBL_L2TBL_TTE_TYPE_FAULT:	/* Page translation fault.  */
		default:
			*fs = DFSR_FS_TRANS_FAULT_PAGE;
			goto do_fault;
			break;
		}

		pg->c = (desc & TTBL_L2TBL_TTE_C_MASK) >> TTBL_L2TBL_TTE_C_SHIFT;
		pg->b = (desc & TTBL_L2TBL_TTE_B_MASK) >> TTBL_L2TBL_TTE_B_SHIFT;

		break;
	case TTBL_L1TBL_TTE_TYPE_FAULT:
	default:
		pg->dom = 0;
		/* Section translation fault.  */
		*fs = DFSR_FS_TRANS_FAULT_SECTION;
		goto do_fault;
		break;
	}
		
	if (domain == 3) {
		/* Page permission not to be checked so, 
		 * give full access using access permissions.
		 */
		pg->ap = TTBL_AP_SRW_URW;
	} else if (check_ap(vcpu, pg->ap, access_type, is_user) == CP15_ACCESS_DENIED) {
		/* Access permission fault.  */
		goto do_fault;
	}

	return VMM_OK;

 do_fault:
	return VMM_EFAIL;
}

u32 cpu_vcpu_cp15_find_page(struct vmm_vcpu *vcpu,
			   virtual_addr_t va,
			   int access_type,
			   bool is_user, struct cpu_page *pg)
{
	int rc = VMM_OK;
	u32 fs = 0x0;
	virtual_addr_t mva = va;

	/* Fast Context Switch Extension. */
	if (mva < 0x02000000) {
		mva += arm_priv(vcpu)->cp15.c13_fcse;
	}

	/* zeroize our page descriptor */
	memset(pg, 0, sizeof(*pg));

	/* Get the required page for vcpu */
	if (arm_priv(vcpu)->cp15.c1_sctlr & SCTLR_M_MASK) {
		/* MMU enabled for vcpu */
		if (arm_priv(vcpu)->cp15.c1_sctlr & SCTLR_V6_MASK) {
			rc = ttbl_walk_v6(vcpu, mva, access_type, 
					  is_user, pg, &fs);
		} else {
			rc = ttbl_walk_v5(vcpu, mva, access_type, 
					  is_user, pg, &fs);
		}
		if (rc) {
			/* FIXME: should be ORed with (pg->dom & 0xF) */
			return (fs << 4) | ((arm_priv(vcpu)->cp15.c3 >> 
						(pg->dom << 1)) & 0x3);
		}
		pg->va = va;
	} else {
		/* MMU disabled for vcpu */
		pg->pa = mva;
		pg->va = va;
		pg->sz = TTBL_L2TBL_SMALL_PAGE_SIZE;
		pg->ap = TTBL_AP_SRW_URW;
		pg->c = 1;
	}

	/* Ensure pages for normal vcpu have aligned va & pa */
	pg->pa &= ~(pg->sz - 1);
	pg->va &= ~(pg->sz - 1);

	return 0;
}

int cpu_vcpu_cp15_assert_fault(struct vmm_vcpu *vcpu,
			      arch_regs_t * regs,
			      u32 far, u32 fs, u32 dom, u32 wnr, u32 xn)
{
	u32 fsr;

	if (!(arm_priv(vcpu)->cp15.c1_sctlr & SCTLR_M_MASK)) {
		cpu_vcpu_halt(vcpu, regs);
		return VMM_EFAIL;
	}
	if (xn) {
		fsr = (fs & DFSR_FS_MASK);
		fsr |= ((dom << DFSR_DOM_SHIFT) & DFSR_DOM_MASK);
		if (arm_feature(vcpu, ARM_FEATURE_V7)) {
			fsr |= ((fs >> 4) << DFSR_FS4_SHIFT);
			fsr |= ((wnr << DFSR_WNR_SHIFT) & DFSR_WNR_MASK);
		}
		arm_priv(vcpu)->cp15.c5_dfsr = fsr;
		arm_priv(vcpu)->cp15.c6_dfar = far;
		vmm_vcpu_irq_assert(vcpu, CPU_DATA_ABORT_IRQ, 0x0);
	} else {
		fsr = fs & IFSR_FS_MASK;
		if (arm_feature(vcpu, ARM_FEATURE_V7)) {
			fsr |= ((fs >> 4) << IFSR_FS4_SHIFT);
			arm_priv(vcpu)->cp15.c6_ifar = far;
		}
		arm_priv(vcpu)->cp15.c5_ifsr = fsr;
		vmm_vcpu_irq_assert(vcpu, CPU_PREFETCH_ABORT_IRQ, 0x0);
	}
	return VMM_OK;
}

int cpu_vcpu_cp15_trans_fault(struct vmm_vcpu *vcpu,
			      arch_regs_t * regs,
			      u32 far, u32 fs, u32 dom,
			      u32 wnr, u32 xn, bool force_user)
{
	u32 orig_domain, tre_index, tre_inner, tre_outer, tre_type;
	u32 ecode, reg_flags;
	bool is_user, is_virtual;
	int rc, access_type;
	struct cpu_page pg;
	physical_size_t availsz;

	if (xn) {
		if (wnr) {
			access_type = CP15_ACCESS_WRITE;
		} else {
			access_type = CP15_ACCESS_READ;
		}
	} else {
		access_type = CP15_ACCESS_EXECUTE;
	}

	if (force_user) {
		is_user = TRUE;
	} else {
		if ((arm_priv(vcpu)->cpsr & CPSR_MODE_MASK) == CPSR_MODE_USER) {
			is_user = TRUE;
		} else {
			is_user = FALSE;
		}
	}

	if ((ecode = cpu_vcpu_cp15_find_page(vcpu, far,
					     access_type, is_user, &pg))) {
		return cpu_vcpu_cp15_assert_fault(vcpu, regs,
						  far, (ecode >> 4),
						  (ecode & 0xF), wnr, xn);
	}
	if (pg.sz > TTBL_L2TBL_SMALL_PAGE_SIZE) {
		pg.sz = TTBL_L2TBL_SMALL_PAGE_SIZE;
		pg.pa = pg.pa + ((far & ~(pg.sz - 1)) - pg.va);
		pg.va = far & ~(pg.sz - 1);
	}

	if ((rc = vmm_guest_physical_map(vcpu->guest,
					 pg.pa, pg.sz,
					 &pg.pa, &availsz,
					 &reg_flags))) {
		vmm_manager_vcpu_halt(vcpu);
		return rc;
	}
	if (availsz < TTBL_L2TBL_SMALL_PAGE_SIZE) {
		return rc;
	}
	orig_domain = pg.dom;
	pg.sz = cpu_mmu_best_page_size(pg.va, pg.pa, availsz);
	switch (pg.ap) {
	case TTBL_AP_S_U:
		pg.dom = TTBL_L1TBL_TTE_DOM_VCPU_USER;
		pg.ap = TTBL_AP_S_U;
		break;
	case TTBL_AP_SRW_U:
		pg.dom = TTBL_L1TBL_TTE_DOM_VCPU_SUPER;
		pg.ap = TTBL_AP_SRW_URW;
		break;
	case TTBL_AP_SRW_UR:
		pg.dom = TTBL_L1TBL_TTE_DOM_VCPU_SUPER_RW_USER_R;
		pg.ap = TTBL_AP_SRW_UR;
		break;
	case TTBL_AP_SRW_URW:
		pg.dom = TTBL_L1TBL_TTE_DOM_VCPU_USER;
		pg.ap = TTBL_AP_SRW_URW;
		break;
#if !defined(CONFIG_ARMV5)
	case TTBL_AP_SR_U:
		pg.dom = TTBL_L1TBL_TTE_DOM_VCPU_SUPER;
		pg.ap = TTBL_AP_SRW_UR;
		break;
	case TTBL_AP_SR_UR_DEPRECATED:
	case TTBL_AP_SR_UR:
		pg.dom = TTBL_L1TBL_TTE_DOM_VCPU_USER;
		pg.ap = TTBL_AP_SRW_UR;
		break;
#endif
	default:
		pg.dom = TTBL_L1TBL_TTE_DOM_VCPU_USER;
		pg.ap = TTBL_AP_S_U;
		break;
	};
	is_virtual = FALSE;
	if (reg_flags & VMM_REGION_VIRTUAL) {
		is_virtual = TRUE;
		switch (pg.ap) {
		case TTBL_AP_SRW_U:
			pg.ap = TTBL_AP_S_U;
			break;
		case TTBL_AP_SRW_UR:
#if !defined(CONFIG_ARMV5)
			pg.ap = TTBL_AP_SR_U;
#else
			/* FIXME: I am not sure this is right */
			pg.ap = TTBL_AP_SRW_U;
#endif
			break;
		case TTBL_AP_SRW_URW:
			pg.ap = TTBL_AP_SRW_U;
			break;
		default:
			break;
		}
	} else if (reg_flags & VMM_REGION_READONLY) {
		switch (pg.ap) {
		case TTBL_AP_SRW_URW:
			pg.ap = TTBL_AP_SRW_UR;
			break;
		default:
			break;
		}
	}

	if (arm_feature(vcpu, ARM_FEATURE_V7) &&
	    (arm_priv(vcpu)->cp15.c1_sctlr & SCTLR_TRE_MASK)) {
		tre_index = ((pg.tex & 0x1) << 2) |
			    ((pg.c & 0x1) << 1) |
			    (pg.b & 0x1);
		tre_inner = arm_priv(vcpu)->cp15.c10_nmrr >> (tre_index * 2);
		tre_inner &= 0x3;
		tre_outer = arm_priv(vcpu)->cp15.c10_nmrr >> (tre_index * 2);
		tre_outer = (tre_outer >> 16) & 0x3;
		tre_type = arm_priv(vcpu)->cp15.c10_prrr >> (tre_index * 2);
		tre_type &= 0x3;
		switch (tre_type) {
		case 0: /* Strongly-Ordered Memory */
			pg.c = 0;
			pg.b = 0;
			pg.tex = 0;
			pg.s = 1;
			break;
		case 1: /* Device Memory */
			pg.c = (tre_inner & 0x2) >> 1;
			pg.b = (tre_inner & 0x1);
			pg.tex = 0x4 | tre_outer;
			pg.s = arm_priv(vcpu)->cp15.c10_prrr >> (16 + pg.s);
			break;
		case 2: /* Normal Memory */
			pg.c = (tre_inner & 0x2) >> 1;
			pg.b = (tre_inner & 0x1);
			pg.tex = 0x4 | tre_outer;
			pg.s = arm_priv(vcpu)->cp15.c10_prrr >> (18 + pg.s);
			break;
		case 3:
		default:
			pg.c = 0;
			pg.b = 0;
			pg.tex = 0;
			pg.s = 0;
			break;
		};
	}

	if (pg.tex & 0x4) {
		if (reg_flags & VMM_REGION_CACHEABLE) {
			if (!(reg_flags & VMM_REGION_BUFFERABLE)) {
				if ((pg.c == 0 && pg.b == 1) ||
				    (pg.c == 1 && pg.b == 1)) {
					pg.c = 1;
					pg.b = 0;
				}
				if (((pg.tex & 0x3) == 0x1) ||
				    ((pg.tex & 0x3) == 0x3)) {
					pg.tex = 0x6;
				}
			}
		} else {
			pg.c = 0;
			pg.b = 0;
			pg.tex = 0x4;
		}
	} else {
		pg.c = pg.c && (reg_flags & VMM_REGION_CACHEABLE);
		pg.b = pg.b && (reg_flags & VMM_REGION_BUFFERABLE);
	}

	return cpu_vcpu_cp15_vtlb_update(vcpu, &pg, orig_domain, is_virtual);
}

int cpu_vcpu_cp15_access_fault(struct vmm_vcpu *vcpu,
			       arch_regs_t * regs,
			       u32 far, u32 fs, u32 dom, u32 wnr, u32 xn)
{
	/* We don't do anything about access fault */
	/* Assert fault to vcpu */
	return cpu_vcpu_cp15_assert_fault(vcpu, regs, far, fs, dom, wnr, xn);
}

int cpu_vcpu_cp15_domain_fault(struct vmm_vcpu *vcpu,
			       arch_regs_t * regs,
			       u32 far, u32 fs, u32 dom, u32 wnr, u32 xn)
{
	int rc = VMM_OK;
	struct cpu_page pg;

	/* Try to retrieve the faulting page */
	if ((rc = cpu_mmu_get_page(arm_priv(vcpu)->cp15.l1, far, &pg))) {
		cpu_vcpu_halt(vcpu, regs);
		return rc;
	}
	if (((arm_priv(vcpu)->cpsr & CPSR_MODE_MASK) == CPSR_MODE_USER) &&
	    (pg.dom == TTBL_L1TBL_TTE_DOM_VCPU_SUPER)) {
		/* Remove fault address from VTLB and restart.
		 * Doing this will force us to do TTBL walk If MMU 
		 * is enabled then appropriate fault will be generated 
		 */
		rc = cpu_vcpu_cp15_vtlb_flush_va(vcpu, far);
	} else {
		cpu_vcpu_halt(vcpu, regs);
		rc = VMM_EFAIL;
	}
	return rc;
}

int cpu_vcpu_cp15_perm_fault(struct vmm_vcpu *vcpu,
			     arch_regs_t * regs,
			     u32 far, u32 fs, u32 dom, u32 wnr, u32 xn)
{
	int rc = VMM_OK;
	struct cpu_page *pg = &arm_priv(vcpu)->cp15.virtio_page;

	/* Try to retrieve the faulting page */
	if ((rc = cpu_mmu_get_page(arm_priv(vcpu)->cp15.l1, far, pg))) {
		/* Remove fault address from VTLB and restart.
		 * Doing this will force us to do TTBL walk If MMU 
		 * is enabled then appropriate fault will be generated.
		 */
		return cpu_vcpu_cp15_vtlb_flush_va(vcpu, far);
	}
	/* Check if vcpu was trying read/write to virtual space */
	if (xn && ((pg->ap == TTBL_AP_SRW_U) || (pg->ap == TTBL_AP_SR_U))) {
		/* Emulate load/store instructions */
		arm_priv(vcpu)->cp15.virtio_active = TRUE;
		if (regs->cpsr & CPSR_THUMB_ENABLED) {
			rc = emulate_thumb_inst(vcpu, regs, 
						*((u32 *) regs->pc));
		} else {
			rc = emulate_arm_inst(vcpu, regs, 
						*((u32 *) regs->pc));
		}
		arm_priv(vcpu)->cp15.virtio_active = FALSE;
		return rc;
	}
	/* Remove fault address from VTLB and restart.
	 * Doing this will force us to do TTBL walk If MMU 
	 * is enabled then appropriate fault will be generated.
	 */
	return cpu_vcpu_cp15_vtlb_flush_va(vcpu, far);
}

bool cpu_vcpu_cp15_read(struct vmm_vcpu * vcpu,
			arch_regs_t * regs,
			u32 opc1, u32 opc2, u32 CRn, u32 CRm, u32 * data)
{
	*data = 0x0;
	switch (CRn) {
	case 0:		/* ID codes.  */
		switch (opc1) {
		case 0:
			switch (CRm) {
			case 0:
				switch (opc2) {
				case 0:	/* Device ID.  */
					*data = arm_priv(vcpu)->cp15.c0_cpuid;
					break;
				case 1:	/* Cache Type.  */
					*data =
					    arm_priv(vcpu)->cp15.c0_cachetype;
					break;
				case 2:	/* TCM status.  */
					*data = 0;
					break;
				case 3:	/* TLB type register.  */
					*data = 0;	/* No lockable TLB entries.  */
					break;
				case 5:	/* MPIDR */
					/* The MPIDR was standardised in v7; prior to
					 * this it was implemented only in the 11MPCore.
					 * For all other pre-v7 cores it does not exist.
					 */
					if (arm_feature(vcpu, ARM_FEATURE_V7) ||
					    arm_cpuid(vcpu) ==
					    ARM_CPUID_ARM11MPCORE) {
						int mpidr = vcpu->subid;
						/* We don't support setting cluster ID ([8..11])
						 * so these bits always RAZ.
						 */
						if (arm_feature
						    (vcpu, ARM_FEATURE_V7MP)) {
							mpidr |= (1 << 31);
							/* Cores which are uniprocessor (non-coherent)
							 * but still implement the MP extensions set
							 * bit 30. (For instance, A9UP.) However we do
							 * not currently model any of those cores.
							 */
						}
						*data = mpidr;
					}
					/* otherwise fall through to the unimplemented-reg case */
					break;
				default:
					goto bad_reg;
				}
				break;
			case 1:
				if (!arm_feature(vcpu, ARM_FEATURE_V6))
					goto bad_reg;
				switch (opc2) {
				case 0:
					*data = arm_priv(vcpu)->cp15.c0_pfr0;
					break;
				case 1:
					*data = arm_priv(vcpu)->cp15.c0_pfr1;
					break;
				case 2:
					*data = arm_priv(vcpu)->cp15.c0_dfr0;
					break;
				case 3:
					*data = arm_priv(vcpu)->cp15.c0_afr0;
					break;
				case 4:
					*data = arm_priv(vcpu)->cp15.c0_mmfr0;
					break;
				case 5:
					*data = arm_priv(vcpu)->cp15.c0_mmfr1;
					break;
				case 6:
					*data = arm_priv(vcpu)->cp15.c0_mmfr2;
					break;
				case 7:
					*data = arm_priv(vcpu)->cp15.c0_mmfr3;
					break;
				default:
					*data = 0;
					break;
				};
				break;
			case 2:
				if (!arm_feature(vcpu, ARM_FEATURE_V6))
					goto bad_reg;
				switch (opc2) {
				case 0:
					*data = arm_priv(vcpu)->cp15.c0_isar0;
					break;
				case 1:
					*data = arm_priv(vcpu)->cp15.c0_isar1;
					break;
				case 2:
					*data = arm_priv(vcpu)->cp15.c0_isar2;
					break;
				case 3:
					*data = arm_priv(vcpu)->cp15.c0_isar3;
					break;
				case 4:
					*data = arm_priv(vcpu)->cp15.c0_isar4;
					break;
				case 5:
					*data = arm_priv(vcpu)->cp15.c0_isar5;
					break;
				default:
					*data = 0;
					break;
				};
				break;
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
				*data = 0;
				break;
			default:
				goto bad_reg;
			}
			break;
		case 1:
			/* These registers aren't documented on arm11 cores.  However
			 * Linux looks at them anyway.  */
			if (!arm_feature(vcpu, ARM_FEATURE_V6))
				goto bad_reg;
			if (CRm != 0)
				goto bad_reg;
			if (!arm_feature(vcpu, ARM_FEATURE_V7)) {
				*data = 0;
				break;
			}
			switch (opc2) {
			case 0:
				*data =
				    arm_priv(vcpu)->cp15.
				    c0_ccsid[arm_priv(vcpu)->cp15.c0_cssel];
				break;
			case 1:
				*data = arm_priv(vcpu)->cp15.c0_clid;
				break;
			case 7:
				*data = 0;
				break;
			default:
				goto bad_reg;
			}
			break;
		case 2:
			if (opc2 != 0 || CRm != 0)
				goto bad_reg;
			*data = arm_priv(vcpu)->cp15.c0_cssel;
			break;
		default:
			goto bad_reg;
		};
		break;
	case 1:		/* System configuration.  */
		switch (opc2) {
		case 0:	/* Control register.  */
			*data = arm_priv(vcpu)->cp15.c1_sctlr;
			break;
		case 1:	/* Auxiliary control register.  */
			if (!arm_feature(vcpu, ARM_FEATURE_AUXCR))
				goto bad_reg;
			switch (arm_cpuid(vcpu)) {
			case ARM_CPUID_ARM1026:
				*data = 1;
				break;
			case ARM_CPUID_ARM1136:
			case ARM_CPUID_ARM1136_R2:
				*data = 7;
				break;
			case ARM_CPUID_ARM11MPCORE:
				*data = 1;
				break;
			case ARM_CPUID_CORTEXA8:
				*data = 2;
				break;
			case ARM_CPUID_CORTEXA9:
				*data = 0;
				if (arm_feature(vcpu, ARM_FEATURE_V7MP)) {
					*data |= (1 << 6);
				} else {
					*data &= ~(1 << 6);
				}
				break;
			default:
				goto bad_reg;
			}
			break;
		case 2:	/* Coprocessor access register.  */
			*data = arm_priv(vcpu)->cp15.c1_coproc;
			break;
		default:
			goto bad_reg;
		};
		break;
	case 2:		/* MMU Page table control / MPU cache control.  */
		switch (opc2) {
		case 0:
			*data = arm_priv(vcpu)->cp15.c2_base0;
			break;
		case 1:
			*data = arm_priv(vcpu)->cp15.c2_base1;
			break;
		case 2:
			*data = arm_priv(vcpu)->cp15.c2_control;
			break;
		default:
			goto bad_reg;
		};
		break;
	case 3:		/* MMU Domain access control / MPU write buffer control.  */
		*data = arm_priv(vcpu)->cp15.c3;
		break;
	case 4:		/* Reserved.  */
		goto bad_reg;
	case 5:		/* MMU Fault status / MPU access permission.  */
		switch (opc2) {
		case 0:
			*data = arm_priv(vcpu)->cp15.c5_dfsr;
			break;
		case 1:
			*data = arm_priv(vcpu)->cp15.c5_ifsr;
			break;
		default:
			goto bad_reg;
		};
		break;
	case 6:		/* MMU Fault address.  */
		switch (opc2) {
		case 0:
			*data = arm_priv(vcpu)->cp15.c6_dfar;
			break;
		case 1:
			if (arm_feature(vcpu, ARM_FEATURE_V6)) {
				/* Watchpoint Fault Adrress.  */
				*data = 0;	/* Not implemented.  */
			} else {
				/* Instruction Fault Adrress.  */
				/* Arm9 doesn't have an IFAR, but implementing it anyway
				 * shouldn't do any harm.  */
				*data = arm_priv(vcpu)->cp15.c6_ifar;
			}
			break;
		case 2:
			if (arm_feature(vcpu, ARM_FEATURE_V6)) {
				/* Instruction Fault Adrress.  */
				*data = arm_priv(vcpu)->cp15.c6_ifar;
			} else {
				goto bad_reg;
			}
			break;
		default:
			goto bad_reg;
		};
		break;
	case 7:		/* Cache control.  */
		switch (opc2) {
		case 0:
			if (CRm == 4 && opc1 == 0) {
				*data = arm_priv(vcpu)->cp15.c7_par;
			} else {
				/* FIXME: Should only clear Z flag if destination is r15.  */
				regs->cpsr &= ~CPSR_ZERO_MASK;
				*data = 0;
			}
			break;
		case 3:
			switch (CRm) {
			case 10:	/* Test and clean DCache */
				clean_dcache();
				regs->cpsr |= CPSR_ZERO_MASK;
				*data = 0;
				break;
			case 14:	/* Test, clean and invalidate DCache */
				clean_dcache();
				regs->cpsr |= CPSR_ZERO_MASK;
				*data = 0;
				break;
			default:
				/* FIXME: Should only clear Z flag if destination is r15.  */
				regs->cpsr &= ~CPSR_ZERO_MASK;
				*data = 0;
				break;
			}
			break;
		default:
			/* FIXME: Should only clear Z flag if destination is r15.  */
			regs->cpsr &= ~CPSR_ZERO_MASK;
			*data = 0;
			break;
		}
		break;
	case 8:		/* MMU TLB control.  */
		goto bad_reg;
	case 9:		/* Cache lockdown.  */
		switch (opc1) {
		case 0:	/* L1 cache.  */
			switch (opc2) {
			case 0:
				*data = arm_priv(vcpu)->cp15.c9_data;
				break;
			case 1:
				*data = arm_priv(vcpu)->cp15.c9_insn;
				break;
			default:
				goto bad_reg;
			};
			break;
		case 1:	/* L2 cache */
			if (CRm != 0)
				goto bad_reg;
			/* L2 Lockdown and Auxiliary control.  */
			*data = 0;
			break;
		default:
			goto bad_reg;
		};
		break;
	case 10:		/* MMU TLB lockdown.  */
		/* ??? TLB lockdown not implemented.  */
		*data = 0;
		switch (CRm) {
		case 2:
			switch (opc2) {
			case 0:
				*data = arm_priv(vcpu)->cp15.c10_prrr;
				break;
			case 1:
				*data = arm_priv(vcpu)->cp15.c10_nmrr;
				break;
			default:
				break;
			}
			break;
		default:
			break;
		};
		break;
	case 11:		/* TCM DMA control.  */
	case 12:		/* Reserved.  */
		goto bad_reg;
	case 13:		/* Process ID.  */
		switch (opc2) {
		case 0:
			*data = arm_priv(vcpu)->cp15.c13_fcse;
			break;
		case 1:
			*data = arm_priv(vcpu)->cp15.c13_context;
			break;
		case 2:
			/* TPIDRURW */
			if (arm_feature(vcpu, ARM_FEATURE_V6)) {
				*data = arm_priv(vcpu)->cp15.c13_tls1;
			} else {
				goto bad_reg;
			}
			break;
		case 3:
			/* TPIDRURO */
			if (arm_feature(vcpu, ARM_FEATURE_V6)) {
				*data = arm_priv(vcpu)->cp15.c13_tls2;
			} else {
				goto bad_reg;
			}
			break;
		case 4:
			/* TPIDRPRW */
			if (arm_feature(vcpu, ARM_FEATURE_V6)) {
				*data = arm_priv(vcpu)->cp15.c13_tls3;
			} else {
				goto bad_reg;
			}
			break;
		default:
			goto bad_reg;
		};
		break;
	case 14:		/* Reserved.  */
		goto bad_reg;
	case 15:		/* Implementation specific.  */
		*data = 0;
		break;
	}
	return TRUE;
 bad_reg:
	return FALSE;
}

bool cpu_vcpu_cp15_write(struct vmm_vcpu * vcpu,
			 arch_regs_t * regs,
			 u32 opc1, u32 opc2, u32 CRn, u32 CRm, u32 data)
{
	u32 tmp;

	switch (CRn) {
	case 0:
		/* ID codes.  */
		if (arm_feature(vcpu, ARM_FEATURE_V7) &&
		    (opc1 == 2) && (CRm == 0) && (opc2 == 0)) {
			arm_priv(vcpu)->cp15.c0_cssel = data & 0xf;
			break;
		}
		goto bad_reg;
	case 1:		/* System configuration.  */
		switch (opc2) {
		case 0:
			/* store old value of sctlr */
			tmp =  arm_priv(vcpu)->cp15.c1_sctlr & SCTLR_MMU_MASK;
			if (arm_feature(vcpu, ARM_FEATURE_V7)) {
				arm_priv(vcpu)->cp15.c1_sctlr &= SCTLR_ROBITS_MASK;
				arm_priv(vcpu)->cp15.c1_sctlr |= (data & ~SCTLR_ROBITS_MASK);
			} else {
				arm_priv(vcpu)->cp15.c1_sctlr &= SCTLR_V5_ROBITS_MASK;
				arm_priv(vcpu)->cp15.c1_sctlr |= (data & ~SCTLR_V5_ROBITS_MASK);
			}

			/* ??? Lots of these bits are not implemented.  */
			if (tmp != (arm_priv(vcpu)->cp15.c1_sctlr & SCTLR_MMU_MASK)) {
				/* For single-core guests flush VTLB only when
				 * MMU related bits in SCTLR changes
				 */
				cpu_vcpu_cp15_vtlb_flush(vcpu);
			} else {
				/* If no change in SCTLR then flush 
				 * non-global pages from VTLB
				 */
				cpu_vcpu_cp15_vtlb_flush_ng(vcpu);
			}
			break;
		case 1:	/* Auxiliary control register.  */
			/* Not implemented.  */
			break;
		case 2:
			if (arm_priv(vcpu)->cp15.c1_coproc != data) {
				arm_priv(vcpu)->cp15.c1_coproc = data;
			}
			break;
		default:
			goto bad_reg;
		};
		break;
	case 2:		/* MMU Page table control / MPU cache control.  */
		switch (opc2) {
		case 0:
			arm_priv(vcpu)->cp15.c2_base0 = data;
			break;
		case 1:
			arm_priv(vcpu)->cp15.c2_base1 = data;
			break;
		case 2:
			data &= 7;
			arm_priv(vcpu)->cp15.c2_control = data;
			arm_priv(vcpu)->cp15.c2_mask =
			    ~(((u32) 0xffffffffu) >> data);
			arm_priv(vcpu)->cp15.c2_base_mask =
			    ~((u32) 0x3fffu >> data);
			break;
		default:
			goto bad_reg;
		};
		break;
	case 3:		/* MMU Domain access control / MPU write buffer control.  */
		tmp = arm_priv(vcpu)->cp15.c3;
		arm_priv(vcpu)->cp15.c3 = data;

		if (tmp != data) {
			cpu_vcpu_cp15_vtlb_flush_domain(vcpu, tmp ^ data);
		}
		break;
	case 4:		/* Reserved.  */
		goto bad_reg;
	case 5:		/* MMU Fault status / MPU access permission.  */
		switch (opc2) {
		case 0:
			arm_priv(vcpu)->cp15.c5_dfsr = data;
			break;
		case 1:
			arm_priv(vcpu)->cp15.c5_ifsr = data;
			break;
		default:
			goto bad_reg;
		};
		break;
	case 6:		/* MMU Fault address / MPU base/size.  */
		switch (opc2) {
		case 0:
			arm_priv(vcpu)->cp15.c6_dfar = data;
			break;
		case 1:	/* ??? This is WFAR on armv6 */
		case 2:
			if (arm_feature(vcpu, ARM_FEATURE_V6)) {
				arm_priv(vcpu)->cp15.c6_ifar = data;
			} else {
				goto bad_reg;
			}
			break;
		default:
			goto bad_reg;
		}
		break;
	case 7:		/* Cache control.  */
		arm_priv(vcpu)->cp15.c15_i_max = 0x000;
		arm_priv(vcpu)->cp15.c15_i_min = 0xff0;
		if (opc1 != 0) {
			goto bad_reg;
		}
		/* Note: Data cache invalidate is a dangerous 
		 * operation since it is possible that Xvisor had its 
		 * own updates in data cache which are not written to 
		 * main memory we might end-up losing those updates 
		 * which can potentially crash the system. 
		 */
		switch (CRm) {
		case 0:
			switch (opc2) {
			case 4:
				/* Legacy wait-for-interrupt */
				/* Emulation for ARMv5, ARMv6 */
				vmm_vcpu_irq_wait(vcpu);
				break;
			default:
				goto bad_reg;
			};
			break;
		case 1: 
			if (arm_feature(vcpu, ARM_FEATURE_V7MP)) {
				/* TODO: Check if treating these as nop is ok */
				switch (opc2) {
				case 0:
					/* Invalidate all I-caches to PoU 
					 * innner-shareable - ICIALLUIS */
					invalidate_icache();
					break;
				case 6:
					/* Invalidate all branch predictors 
					 * innner-shareable - BPIALLUIS */
					invalidate_bpredictor();
					break;
				default:
					goto bad_reg;
				};
			}
			break;
		case 4:
			/* VA->PA translations. */
			if (arm_feature(vcpu, ARM_FEATURE_VAPA)) {
				if (arm_feature(vcpu, ARM_FEATURE_V7)) {
					arm_priv(vcpu)->cp15.c7_par =
					    data & 0xfffff6ff;
				} else {
					arm_priv(vcpu)->cp15.c7_par =
					    data & 0xfffff1ff;
				}
			}
			break;
		case 5:
			switch (opc2) {
			case 0:
				/* Invalidate all instruction caches to PoU */
				/* Emulation for ARMv5, ARMv6, ARMv7 */
				invalidate_icache();
				break;
			case 1:
				/* Invalidate instruction cache line by MVA to PoU */
				/* Emulation for ARMv5, ARMv6, ARMv7 */
				invalidate_icache_mva(data);
				break;
			case 2:
				/* Invalidate instruction cache line by set/way. */
				/* Emulation for ARMv5, ARMv6 */
				invalidate_icache_line(data);
				break;
			case 4:
				/* Instruction synchroization barrier */
				/* Emulation for ARMv5, ARMv6, ARMv7 */
				isb();
				break;
			case 6:
				/* Invalidate entire branch predictor array */
				/* Emulation for ARMv5, ARMv6, ARMv7 */
				invalidate_bpredictor();
				break;
			case 7:
				/* Invalidate MVA from branch predictor array */
				/* Emulation for ARMv5, ARMv6, ARMv7 */
				invalidate_bpredictor_mva(data);
				break;
			default:
				goto bad_reg;
			};
			break;
		case 6:
			switch (opc2) {
			case 0:
				/* Invalidate data caches */
				/* Emulation for ARMv5, ARMv6 */
				/* For safety and correctness upgrade it to 
				 * Clean and invalidate data cache.
				 */
				clean_invalidate_dcache();
				break;
			case 1:
				/* Invalidate data cache line by MVA to PoC. */
				/* Emulation for ARMv5, ARMv6, ARMv7 */
				/* For safety and correctness upgrade it to 
				 * Clean and invalidate data cache.
				 */
				clean_invalidate_dcache_mva(data);
				break;
			case 2:
				/* Invalidate data cache line by set/way. */
				/* Emulation for ARMv5, ARMv6, ARMv7 */
				/* For safety and correctness upgrade it to 
				 * Clean and invalidate data cache.
				 */
				clean_invalidate_dcache_line(data);
				break;
			default:
				goto bad_reg;
			};
			break;
		case 7:
			switch (opc2) {
			case 0:
				/* Invalidate unified cache */
				/* Emulation for ARMv5, ARMv6 */
				/* For safety and correctness upgrade it to
				 * Clean and invalidate unified cache
				 */
				clean_invalidate_idcache();
				break;
			case 1:
				/* Invalidate unified cache line by MVA */
				/* Emulation for ARMv5, ARMv6 */
				/* For safety and correctness upgrade it to
				 * Clean and invalidate unified cache
				 */
				clean_invalidate_idcache_mva(data);
				break;
			case 2:
				/* Invalidate unified cache line by set/way */
				/* Emulation for ARMv5, ARMv6 */
				/* For safety and correctness upgrade it to
				 * Clean and invalidate unified cache
				 */
				clean_invalidate_idcache_line(data);
				break;
			default:
				goto bad_reg;
			};
			break;
		case 8:
			/* VA->PA translations. */
			if (arm_feature(vcpu, ARM_FEATURE_VAPA)) {
				struct cpu_page pg;
				int ret, is_user = opc2 & 2;
				int access_type = opc2 & 1;
				if (opc2 & 4) {
					/* Other states are only available with TrustZone */
					goto bad_reg;
				}
				ret = cpu_vcpu_cp15_find_page(vcpu, data, 
							      access_type, is_user,
							      &pg);
				if (ret == 0) {
					/* We do not set any attribute bits in the PAR */
					if (pg.sz == TTBL_L1TBL_SUPSECTION_PAGE_SIZE && 
					    arm_feature(vcpu, ARM_FEATURE_V7)) {
						arm_priv(vcpu)->cp15.c7_par = (pg.pa & 0xff000000) | 1 << 1;
					} else {
						arm_priv(vcpu)->cp15.c7_par = pg.pa & 0xfffff000;
					}
				} else {
					arm_priv(vcpu)->cp15.c7_par = (((ret >> 9) & 0x1) << 6) |
								   (((ret >> 4) & 0x1F) << 1) | 1;
				}
			}
			break;
		case 10:
			switch (opc2) {
			case 0:
				/* Clean data cache */
				/* Emulation for ARMv6 */
				clean_dcache();
				break;
			case 1:
				/* Clean data cache line by MVA. */
				/* Emulation for ARMv5, ARMv6, ARMv7 */
				clean_dcache_mva(data);
				break;
			case 2:
				/* Clean data cache line by set/way. */
				/* Emulation for ARMv5, ARMv6, ARMv7 */
				clean_dcache_line(data);
				break;
			case 4:
				/* Data synchroization barrier */
				/* Emulation for ARMv5, ARMv6, ARMv7 */
				dsb();
				break;
			case 5:
				/* Data memory barrier */
				/* Emulation for ARMv5, ARMv6, ARMv7 */
				dmb();
				break;
			default:
				goto bad_reg;
			};
			break;
		case 11:
			switch (opc2) {
			case 0:
				/* Clean unified cache */
				/* Emulation for ARMv5, ARMv6 */
				clean_idcache();
				break;
			case 1:
				/* Clean unified cache line by MVA. */
				/* Emulation for ARMv5, ARMv6, ARMv7 */
				clean_idcache_mva(data);
				break;
			case 2:
				/* Clean unified cache line by set/way. */
				/* Emulation for ARMv5, ARMv6 */
				clean_idcache_line(data);
				break;
			default:
				goto bad_reg;
			};
			break;
		case 14:
			switch (opc2) {
			case 0:
				/* Clean and invalidate data cache */
				/* Emulation for ARMv6 */
				clean_invalidate_dcache();
				break;
			case 1:
				/* Clean and invalidate data cache line by MVA */
				/* Emulation for ARMv5, ARMv6, ARMv7 */
				clean_invalidate_dcache_mva(data);
				break;
			case 2:
				/* Clean and invalidate data cache line by set/way */
				/* Emulation for ARMv5, ARMv6, ARMv7 */
				clean_invalidate_dcache_line(data);
				break;
			default:
				goto bad_reg;
			};
			break;
		case 15:
			switch (opc2) {
			case 0:
				/* Clean and invalidate unified cache */
				/* Emulation for ARMv6 */
				clean_invalidate_idcache();
				break;
			case 1:
				/* Clean and Invalidate unified cache line by MVA */
				/* Emulation for ARMv5, ARMv6 */
				clean_invalidate_idcache_mva(data);
				break;
			case 2:
				/* Clean and Invalidate unified cache line by set/way */
				/* Emulation for ARMv5, ARMv6 */
				clean_invalidate_idcache_line(data);
				break;
			default:
				goto bad_reg;
			};
			break;
		default:
			goto bad_reg;
		};
		break;
	case 8:		/* MMU TLB control.  */
		switch (opc2) {
		case 0:	/* Invalidate all.  */
			cpu_vcpu_cp15_vtlb_flush(vcpu);
			break;
		case 1:	/* Invalidate single TLB entry.  */
			cpu_vcpu_cp15_vtlb_flush_va(vcpu, data);
			break;
		case 2: /* Invalidate on ASID.  */
			cpu_vcpu_cp15_vtlb_flush_ng(vcpu);
			break;
		case 3:	/* Invalidate single entry on MVA.  */
			/* ??? This is like case 1, but ignores ASID.  */
			cpu_vcpu_cp15_vtlb_flush_va(vcpu, data);
			break;
		default:
			goto bad_reg;
		}
		break;
	case 9:
		switch (CRm) {
		case 0:	/* Cache lockdown.  */
			switch (opc1) {
			case 0:	/* L1 cache.  */
				switch (opc2) {
				case 0:
					arm_priv(vcpu)->cp15.c9_data = data;
					break;
				case 1:
					arm_priv(vcpu)->cp15.c9_insn = data;
					break;
				default:
					goto bad_reg;
				}
				break;
			case 1:	/* L2 cache.  */
				/* Ignore writes to L2 lockdown/auxiliary registers.  */
				break;
			default:
				goto bad_reg;
			}
			break;
		case 1:	/* TCM memory region registers.  */
			/* Not implemented.  */
			goto bad_reg;
		case 12:	/* Performance monitor control */
			/* Performance monitors are implementation defined in v7,
			 * but with an ARM recommended set of registers, which we
			 * follow (although we don't actually implement any counters)
			 */
			if (!arm_feature(vcpu, ARM_FEATURE_V7)) {
				goto bad_reg;
			}
			switch (opc2) {
			case 0:	/* performance monitor control register */
				/* only the DP, X, D and E bits are writable */
				arm_priv(vcpu)->cp15.c9_pmcr &= ~0x39;
				arm_priv(vcpu)->cp15.c9_pmcr |= (data & 0x39);
				break;
			case 1:	/* Count enable set register */
				data &= (1 << 31);
				arm_priv(vcpu)->cp15.c9_pmcnten |= data;
				break;
			case 2:	/* Count enable clear */
				data &= (1 << 31);
				arm_priv(vcpu)->cp15.c9_pmcnten &= ~data;
				break;
			case 3:	/* Overflow flag status */
				arm_priv(vcpu)->cp15.c9_pmovsr &= ~data;
				break;
			case 4:	/* Software increment */
				/* RAZ/WI since we don't implement 
				 * the software-count event */
				break;
			case 5:	/* Event counter selection register */
				/* Since we don't implement any events, writing to this register
				 * is actually UNPREDICTABLE. So we choose to RAZ/WI.
				 */
				break;
			default:
				goto bad_reg;
			}
			break;
		case 13:	/* Performance counters */
			if (!arm_feature(vcpu, ARM_FEATURE_V7)) {
				goto bad_reg;
			}
			switch (opc2) {
			case 0:	/* Cycle count register: not implemented, so RAZ/WI */
				break;
			case 1:	/* Event type select */
				arm_priv(vcpu)->cp15.c9_pmxevtyper =
				    data & 0xff;
				break;
			case 2:	/* Event count register */
				/* Unimplemented (we have no events), RAZ/WI */
				break;
			default:
				goto bad_reg;
			}
			break;
		case 14:	/* Performance monitor control */
			if (!arm_feature(vcpu, ARM_FEATURE_V7)) {
				goto bad_reg;
			}
			switch (opc2) {
			case 0:	/* user enable */
				arm_priv(vcpu)->cp15.c9_pmuserenr = data & 1;
				/* changes access rights for cp registers, so flush tbs */
				break;
			case 1:	/* interrupt enable set */
				/* We have no event counters so only the C bit can be changed */
				data &= (1 << 31);
				arm_priv(vcpu)->cp15.c9_pminten |= data;
				break;
			case 2:	/* interrupt enable clear */
				data &= (1 << 31);
				arm_priv(vcpu)->cp15.c9_pminten &= ~data;
				break;
			}
			break;
		default:
			goto bad_reg;
		}
		break;
	case 10:		/* MMU TLB lockdown.  */
		/* ??? TLB lockdown not implemented.  */
		switch (CRm) {
		case 2:
			switch (opc2) {
			case 0:
				arm_priv(vcpu)->cp15.c10_prrr = data;
				break;
			case 1:
				arm_priv(vcpu)->cp15.c10_nmrr = data;
				break;
			default:
				break;
			}
			break;
		default:
			break;
		};
		break;
	case 12:		/* Reserved.  */
		goto bad_reg;
	case 13:		/* Process ID.  */
		switch (opc2) {
		case 0:
			/* Unlike real hardware vTLB uses virtual addresses,
			 * not modified virtual addresses, so this causes 
			 * a vTLB flush.
			 */
			if (arm_priv(vcpu)->cp15.c13_fcse != data) {
				cpu_vcpu_cp15_vtlb_flush(vcpu);
			}
			arm_priv(vcpu)->cp15.c13_fcse = data;
			break;
		case 1:
			/* This changes the ASID, 
			 * so flush non-global pages from vTLB.
			 */
			if (arm_priv(vcpu)->cp15.c13_context != data && 
			    !arm_feature(vcpu, ARM_FEATURE_MPU)) {
				cpu_vcpu_cp15_vtlb_flush_ng(vcpu);
			}
			arm_priv(vcpu)->cp15.c13_context = data;
			break;
		case 2:
			if (!arm_feature(vcpu, ARM_FEATURE_V6)) {
				goto bad_reg;
			}
			/* TPIDRURW */
			arm_priv(vcpu)->cp15.c13_tls1 = data;
			write_tpidrurw(data);
			break;
		case 3:
			if (!arm_feature(vcpu, ARM_FEATURE_V6)) {
				goto bad_reg;
			}
			/* TPIDRURO */
			arm_priv(vcpu)->cp15.c13_tls2 = data;
			write_tpidruro(data);
			break;
		case 4:
			if (!arm_feature(vcpu, ARM_FEATURE_V6)) {
				goto bad_reg;
			}
			/* TPIDRPRW */
			arm_priv(vcpu)->cp15.c13_tls3 = data;
			write_tpidrprw(data);
			break;
		default:
			goto bad_reg;
		}
		break;
	case 14:		/* Reserved.  */
		goto bad_reg;
	case 15:		/* Implementation specific.  */
		break;
	}
	return TRUE;
 bad_reg:
	return FALSE;
}

virtual_addr_t cpu_vcpu_cp15_vector_addr(struct vmm_vcpu * vcpu, u32 irq_no)
{
	virtual_addr_t vaddr;
	irq_no = irq_no % CPU_IRQ_NR;

	if (arm_priv(vcpu)->cp15.c1_sctlr & SCTLR_V_MASK) {
		vaddr = CPU_IRQ_HIGHVEC_BASE;
	} else {
		vaddr = CPU_IRQ_LOWVEC_BASE;
	}

	if (arm_priv(vcpu)->cp15.ovect_base == vaddr) {
		vaddr = (virtual_addr_t)arm_guest_priv(vcpu->guest)->ovect;	
	}

	vaddr += 4 * irq_no;

	return vaddr;
}

void cpu_vcpu_cp15_sync_cpsr(struct vmm_vcpu *vcpu)
{
	struct vmm_vcpu *cvcpu = vmm_scheduler_current_vcpu();
	arm_priv(vcpu)->cp15.dacr &=
	    ~(0x3 << (2 * TTBL_L1TBL_TTE_DOM_VCPU_SUPER));
	arm_priv(vcpu)->cp15.dacr &=
	    ~(0x3 << (2 * TTBL_L1TBL_TTE_DOM_VCPU_SUPER_RW_USER_R));

	if ((arm_priv(vcpu)->cpsr & CPSR_MODE_MASK) == CPSR_MODE_USER) {
		arm_priv(vcpu)->cp15.dacr |=
		    (TTBL_DOM_NOACCESS << (2 * TTBL_L1TBL_TTE_DOM_VCPU_SUPER));
		arm_priv(vcpu)->cp15.dacr |=
		    (TTBL_DOM_CLIENT <<
		     (2 * TTBL_L1TBL_TTE_DOM_VCPU_SUPER_RW_USER_R));
	} else {
		arm_priv(vcpu)->cp15.dacr |=
		    (TTBL_DOM_CLIENT << (2 * TTBL_L1TBL_TTE_DOM_VCPU_SUPER));
		arm_priv(vcpu)->cp15.dacr |=
		    (TTBL_DOM_MANAGER <<
		     (2 * TTBL_L1TBL_TTE_DOM_VCPU_SUPER_RW_USER_R));
	}

	if (cvcpu->id == vcpu->id) {
		cpu_mmu_chdacr(arm_priv(vcpu)->cp15.dacr);
	}
}

void cpu_vcpu_cp15_switch_context(struct vmm_vcpu *tvcpu, struct vmm_vcpu *vcpu)
{
	if (tvcpu && tvcpu->is_normal) {
		arm_priv(tvcpu)->cp15.c13_tls1 = read_tpidrurw();
		arm_priv(tvcpu)->cp15.c13_tls2 = read_tpidruro();
		arm_priv(tvcpu)->cp15.c13_tls3 = read_tpidrprw();
	}
	if (vcpu->is_normal) {
		cpu_mmu_chdacr(arm_priv(vcpu)->cp15.dacr);
		cpu_mmu_chttbr(arm_priv(vcpu)->cp15.l1);
		write_tpidrurw(arm_priv(vcpu)->cp15.c13_tls1);
		write_tpidruro(arm_priv(vcpu)->cp15.c13_tls2);
		write_tpidrprw(arm_priv(vcpu)->cp15.c13_tls3);
	} else {
		if (tvcpu) {
			if (tvcpu->is_normal) {
				cpu_mmu_chttbr(cpu_mmu_l1tbl_default());
			}
		} else {
			cpu_mmu_chttbr(cpu_mmu_l1tbl_default());
		}
	}
	/* Ensure pending memory operations are complete */
	dsb();
	isb();
}

int cpu_vcpu_cp15_init(struct vmm_vcpu *vcpu, u32 cpuid)
{
	int rc = VMM_OK;
#if defined(CONFIG_ARMV7A)
	u32 i, cache_type, last_level;
#endif

	if (!vcpu->reset_count) {
		memset(&arm_priv(vcpu)->cp15, 0,
			   sizeof(arm_priv(vcpu)->cp15));
		arm_priv(vcpu)->cp15.l1 = cpu_mmu_l1tbl_alloc();
	} else {
		if ((rc = cpu_vcpu_cp15_vtlb_flush(vcpu))) {
			return rc;
		}
	}

	arm_priv(vcpu)->cp15.dacr = 0x0;
	arm_priv(vcpu)->cp15.dacr |= (TTBL_DOM_CLIENT <<
				      (TTBL_L1TBL_TTE_DOM_VCPU_SUPER *
				       2));
	arm_priv(vcpu)->cp15.dacr |=
		    (TTBL_DOM_MANAGER <<
		     (TTBL_L1TBL_TTE_DOM_VCPU_SUPER_RW_USER_R * 2));
	arm_priv(vcpu)->cp15.dacr |=
		    (TTBL_DOM_CLIENT << (TTBL_L1TBL_TTE_DOM_VCPU_USER * 2));

	if (read_sctlr() & SCTLR_V_MASK) {
		arm_priv(vcpu)->cp15.ovect_base = CPU_IRQ_HIGHVEC_BASE;
	} else {
		arm_priv(vcpu)->cp15.ovect_base = CPU_IRQ_LOWVEC_BASE;
	}

	arm_priv(vcpu)->cp15.virtio_active = FALSE;
	memset(&arm_priv(vcpu)->cp15.virtio_page, 0,
		   sizeof(struct cpu_page));

	arm_priv(vcpu)->cp15.c0_cpuid = cpuid;
	arm_priv(vcpu)->cp15.c2_control = 0x0;
	arm_priv(vcpu)->cp15.c2_base0 = 0x0;
	arm_priv(vcpu)->cp15.c2_base1 = 0x0;
	arm_priv(vcpu)->cp15.c2_mask = 0x0;
	arm_priv(vcpu)->cp15.c2_base_mask = 0xFFFFC000;
	arm_priv(vcpu)->cp15.c9_pmcr = (cpuid & 0xFF000000);
	arm_priv(vcpu)->cp15.c10_prrr = 0x0;
	arm_priv(vcpu)->cp15.c10_nmrr = 0x0;
	/* Reset values of important registers */
	switch (cpuid) {
	case ARM_CPUID_ARM926:
		arm_priv(vcpu)->cp15.c0_cachetype = 0x1dd20d2;
		arm_priv(vcpu)->cp15.c1_sctlr = 0x00090078;
		break;
	case ARM_CPUID_CORTEXA8:
		arm_priv(vcpu)->cp15.c0_cachetype = 0x82048004;
		arm_priv(vcpu)->cp15.c0_pfr0 = 0x1031;
		arm_priv(vcpu)->cp15.c0_pfr1 = 0x11;
		arm_priv(vcpu)->cp15.c0_dfr0 = 0x400;
		arm_priv(vcpu)->cp15.c0_afr0 = 0x0;
		arm_priv(vcpu)->cp15.c0_mmfr0 = 0x31100003;
		arm_priv(vcpu)->cp15.c0_mmfr1 = 0x20000000;
		arm_priv(vcpu)->cp15.c0_mmfr2 = 0x01202000;
		arm_priv(vcpu)->cp15.c0_mmfr3 = 0x11;
		arm_priv(vcpu)->cp15.c0_isar0 = 0x00101111;
		arm_priv(vcpu)->cp15.c0_isar1 = 0x12112111;
		arm_priv(vcpu)->cp15.c0_isar2 = 0x21232031;
		arm_priv(vcpu)->cp15.c0_isar3 = 0x11112131;
		arm_priv(vcpu)->cp15.c0_isar4 = 0x00111142;
		arm_priv(vcpu)->cp15.c0_isar5 = 0x0;
		arm_priv(vcpu)->cp15.c0_clid = (1 << 27) | (2 << 24) | 3;
		arm_priv(vcpu)->cp15.c0_ccsid[0] = 0xe007e01a;	/* 16k L1 dcache. */
		arm_priv(vcpu)->cp15.c0_ccsid[1] = 0x2007e01a;	/* 16k L1 icache. */
		arm_priv(vcpu)->cp15.c0_ccsid[2] = 0xf0000000;	/* No L2 icache. */
		arm_priv(vcpu)->cp15.c1_sctlr = 0x00c50078;
		break;
	case ARM_CPUID_CORTEXA9:
		arm_priv(vcpu)->cp15.c0_cachetype = 0x80038003;
		arm_priv(vcpu)->cp15.c0_pfr0 = 0x1031;
		arm_priv(vcpu)->cp15.c0_pfr1 = 0x11;
		arm_priv(vcpu)->cp15.c0_dfr0 = 0x000;
		arm_priv(vcpu)->cp15.c0_afr0 = 0x0;
		arm_priv(vcpu)->cp15.c0_mmfr0 = 0x00100103;
		arm_priv(vcpu)->cp15.c0_mmfr1 = 0x20000000;
		arm_priv(vcpu)->cp15.c0_mmfr2 = 0x01230000;
		arm_priv(vcpu)->cp15.c0_mmfr3 = 0x00002111;
		arm_priv(vcpu)->cp15.c0_isar0 = 0x00101111;
		arm_priv(vcpu)->cp15.c0_isar1 = 0x13112111;
		arm_priv(vcpu)->cp15.c0_isar2 = 0x21232041;
		arm_priv(vcpu)->cp15.c0_isar3 = 0x11112131;
		arm_priv(vcpu)->cp15.c0_isar4 = 0x00111142;
		arm_priv(vcpu)->cp15.c0_isar5 = 0x0;
		arm_priv(vcpu)->cp15.c0_clid = (1 << 27) | (1 << 24) | 3;
		arm_priv(vcpu)->cp15.c0_ccsid[0] = 0xe00fe015;	/* 16k L1 dcache. */
		arm_priv(vcpu)->cp15.c0_ccsid[1] = 0x200fe015;	/* 16k L1 icache. */
		arm_priv(vcpu)->cp15.c1_sctlr = 0x00c50078;
		break;
	default:
		break;
	}

#if defined(CONFIG_ARMV7A)
	if (arm_feature(vcpu, ARM_FEATURE_V7)) {
		/* Cache config register such as CTR, CLIDR, and CCSIDRx
		 * should be same as that of underlying host.
		 */
		arm_priv(vcpu)->cp15.c0_cachetype = read_ctr();
		arm_priv(vcpu)->cp15.c0_clid = read_clidr();
		last_level = (arm_priv(vcpu)->cp15.c0_clid & CLIDR_LOUU_MASK) 
							>> CLIDR_LOUU_SHIFT;
		for (i = 0; i <= last_level; i++) {
			cache_type = arm_priv(vcpu)->cp15.c0_clid >> (i * 3);
			cache_type &= 0x7;
			switch (cache_type) {
			case CLIDR_CTYPE_ICACHE:
				write_csselr((i << 1) | 1);
				arm_priv(vcpu)->cp15.c0_ccsid[(i << 1) | 1] = 
								read_ccsidr();
				break;
			case CLIDR_CTYPE_DCACHE:
			case CLIDR_CTYPE_UNICACHE:
				write_csselr(i << 1);
				arm_priv(vcpu)->cp15.c0_ccsid[i << 1] = 
								read_ccsidr();
				break;
			case CLIDR_CTYPE_SPLITCACHE:
				write_csselr(i << 1);
				arm_priv(vcpu)->cp15.c0_ccsid[i << 1] = 
								read_ccsidr();
				write_csselr((i << 1) | 1);
				arm_priv(vcpu)->cp15.c0_ccsid[(i << 1) | 1] = 
								read_ccsidr();
				break;
			case CLIDR_CTYPE_NOCACHE:
			case CLIDR_CTYPE_RESERVED1:
			case CLIDR_CTYPE_RESERVED2:
			case CLIDR_CTYPE_RESERVED3:
				arm_priv(vcpu)->cp15.c0_ccsid[i << 1] = 0;
				arm_priv(vcpu)->cp15.c0_ccsid[(i << 1) | 1] = 0;
				break;
			};
		}
	}
#endif

	return rc;
}

int cpu_vcpu_cp15_deinit(struct vmm_vcpu *vcpu)
{
	int rc;

	if ((rc = cpu_mmu_l1tbl_free(arm_priv(vcpu)->cp15.l1))) {
		return rc;
	}

	memset(&arm_priv(vcpu)->cp15, 0, sizeof(arm_priv(vcpu)->cp15));

	return VMM_OK;
}
