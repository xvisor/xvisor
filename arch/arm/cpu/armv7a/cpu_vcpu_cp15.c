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
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief Source for vcpu cp15 emulation
 */

#include <vmm_heap.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_devemu.h>
#include <vmm_scheduler.h>
#include <vmm_guest_aspace.h>
#include <vmm_vcpu_irq.h>
#include <cpu_mmu.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_helper.h>
#include <cpu_vcpu_emulate_arm.h>
#include <cpu_vcpu_emulate_thumb.h>
#include <cpu_vcpu_cp15.h>

/* Update Virtual TLB */
static int cpu_vcpu_cp15_vtlb_update(vmm_vcpu_t * vcpu, 
				     cpu_page_t * p)
{
	int rc;
	u32 victim;
	cpu_vtlb_entry_t * e = NULL;

	/* Add victim page to L1 page table */
	if ((rc = cpu_mmu_map_page(vcpu->sregs->cp15.l1, p))) {
		return rc;
	}

	/* Find out next victim entry from TLB */
	victim = vcpu->sregs->cp15.vtlb.victim;
	e = &vcpu->sregs->cp15.vtlb.table[victim];
	if (e->valid) {
		/* Remove valid victim page from L1 Page Table */
		rc = cpu_mmu_unmap_page(vcpu->sregs->cp15.l1, &e->page);
		if (rc) {
			return rc;
		}
		e->valid = 0;
	}

	/* Mark entry as valid */
	vmm_memcpy(&e->page, p, sizeof(cpu_page_t));
	e->valid = 1;

	/* Point to next victim */
	victim = (victim + 1) % vcpu->sregs->cp15.vtlb.count;
	vcpu->sregs->cp15.vtlb.victim = victim;

	return VMM_OK;
}

/** Flush Virtual TLB */
static int cpu_vcpu_cp15_vtlb_flush(vmm_vcpu_t * vcpu)
{
	int rc;
	u32 vtlb;
	cpu_vtlb_entry_t * e;

	for (vtlb = 0; vtlb < vcpu->sregs->cp15.vtlb.count; vtlb++) {
		if (vcpu->sregs->cp15.vtlb.table[vtlb].valid) {
			e = &vcpu->sregs->cp15.vtlb.table[vtlb];
			rc = cpu_mmu_unmap_page(vcpu->sregs->cp15.l1, 
						&e->page);
			if (rc) {
				return rc;
			}
			vcpu->sregs->cp15.vtlb.table[vtlb].valid = 0;
		}
	}

	return VMM_OK;
}

/** Flush given virtual address from Virtual TLB */
static int cpu_vcpu_cp15_vtlb_flush_va(vmm_vcpu_t * vcpu, 
				       virtual_addr_t va)
{
	int rc;
	u32 vtlb;
	cpu_vtlb_entry_t * e;

	for (vtlb = 0; vtlb < vcpu->sregs->cp15.vtlb.count; vtlb++) {
		if (vcpu->sregs->cp15.vtlb.table[vtlb].valid) {
			e = &vcpu->sregs->cp15.vtlb.table[vtlb];
			if (e->page.va <= va &&
			    va < (e->page.va + e->page.sz)) {
				rc = cpu_mmu_unmap_page(vcpu->sregs->cp15.l1, 
						&e->page);
				if (rc) {
					return rc;
				}
				vcpu->sregs->cp15.vtlb.table[vtlb].valid = 0;
				break;
			}
		}
	}

	return VMM_OK;
}

enum cpu_vcpu_cp15_access_types {
	CP15_ACCESS_READ=0,
	CP15_ACCESS_WRITE=1,
	CP15_ACCESS_EXECUTE=2
};

/* Check section/page access permissions.
 * Returns 1 - permitted, 0 - not-permitted
 */
static inline int check_ap(vmm_vcpu_t *vcpu, 
			   int ap, int access_type,
                           int is_user)
{
	switch (ap) {
	case 0:
		if (access_type == 1)
			return 0;
		switch ((vcpu->sregs->cp15.c1_sctlr >> 8) & 3) {
		case 1:
			if (is_user) 
				return 0;
			else 
				return (access_type != CP15_ACCESS_WRITE) ? 1 : 0;
		case 2:
			return (access_type != CP15_ACCESS_WRITE) ? 1 : 0;
		default:
			return 0;
		}
	case 1:
		return is_user ? 0 : 1;
	case 2:
		if (is_user)
			return (access_type != CP15_ACCESS_WRITE) ? 1 : 0;
		else
			return 1;
	case 3:
		return 1;
	case 4: /* Reserved. */
		return 0;
	case 5:
		if (is_user)
			return 0;
		else
			return (access_type != CP15_ACCESS_WRITE) ? 1 : 0;
	case 6:
		return (access_type != CP15_ACCESS_WRITE) ? 1 : 0;
	case 7:
		if (!arm_feature (vcpu, ARM_FEATURE_V6K))
			return 0;
		return (access_type != CP15_ACCESS_WRITE) ? 1 : 0;
	default:
		return 0;
	};

	return 0;
}

static physical_addr_t get_level1_table_pa(vmm_vcpu_t *vcpu, 
					   virtual_addr_t va)
{
	if (va & vcpu->sregs->cp15.c2_mask) {
		return vcpu->sregs->cp15.c2_base1 & 0xffffc000;
	} else {
		return vcpu->sregs->cp15.c2_base0 & 
			vcpu->sregs->cp15.c2_base_mask;
	}
	return 0x0;
}

static int ttbl_walk_v6(vmm_vcpu_t *vcpu, 
			virtual_addr_t va, 
			int access_type,
			int is_user, 
			cpu_page_t * pg,
			u32 * fs)
{
	physical_addr_t table;
	physical_size_t table_sz;
	int rc, type, domain;
	u32 desc, reg_flags;

	/* Clear memory of page to return */
	vmm_memset(pg, 0, sizeof(cpu_page_t));
	pg->va = va;

	/* Pagetable walk.  */
	/* Lookup l1 descriptor.  */
	table = get_level1_table_pa(vcpu, va);
	rc = vmm_guest_physical_map(vcpu->guest, 
				    table, 
				    0x4000, 
				    &table, 
				    &table_sz,
				    &reg_flags);
	if (rc) {
		return rc;
	}
	if (table_sz < 0x4000) {
		return VMM_EFAIL;
	}
	if (reg_flags & VMM_REGION_VIRTUAL) {
		return VMM_EFAIL;
	}
	table |= (va >> 18) & 0x3ffc;
	desc = cpu_mmu_physical_read32(table);
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
		pg->dom = (desc >> 4) & 0x1e;
	}
	domain = (vcpu->sregs->cp15.c3 >> pg->dom) & 3;
	if (domain == 0 || domain == 2) {
		if (type == 2)
			*fs = 9; /* Section domain fault.  */
		else
			*fs = 11; /* Page domain fault.  */
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
		pg->ap = ((desc >> 10) & 3) | ((desc >> 13) & 4);
		pg->xn = desc & (1 << 4);
		*fs = 13;
	} else {
		/* Lookup l2 entry.  */
		table = (desc & 0xfffffc00);
		reg_flags = 0x0;
		rc = vmm_guest_physical_map(vcpu->guest, 
					    table, 
					    0x400, 
					    &table, 
					    &table_sz,
					    &reg_flags);
		if (rc) {
			return rc;
		}
		if (table_sz < 0x400) {
			return VMM_EFAIL;
		}
		if (reg_flags & VMM_REGION_VIRTUAL) {
			return VMM_EFAIL;
		}
		table |= ((va >> 10) & 0x3fc);
		desc = cpu_mmu_physical_read32(table);
		pg->ap = ((desc >> 4) & 3) | ((desc >> 7) & 4);
		switch (desc & 3) {
		case 0: /* Page translation fault.  */
			*fs = 7;
			goto do_fault;
		case 1: /* 64k page.  */
			pg->pa = (desc & 0xffff0000) | (va & 0xffff);
			pg->xn = desc & (1 << 15);
			pg->sz = 0x10000;
			break;
		case 2: case 3: /* 4k page.  */
			pg->pa = (desc & 0xfffff000) | (va & 0xfff);
			pg->xn = desc & 1;
			pg->sz = 0x1000;
			break;
		default:
			/* Never happens, but compiler isn't 
			 * smart enough to tell.
			 */
			return VMM_EFAIL;
		}
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
		if ((vcpu->sregs->cp15.c1_sctlr & (1 << 29)) && (pg->ap & 1) == 0) {
			/* Access flag fault.  */
			*fs = (*fs == 15) ? 6 : 3;
			goto do_fault;
		}
		if (!check_ap(vcpu, pg->ap, access_type, is_user)) {
			/* Access permission fault.  */
			goto do_fault;
		}
	}
	return VMM_OK;
do_fault:
	return VMM_EFAIL;
}

static u32 cpu_vcpu_cp15_find_page(vmm_vcpu_t * vcpu, 
				   virtual_addr_t va, 
				   int access_type,
				   bool is_user,
				   cpu_page_t * pg)
{
	int rc = VMM_OK;
	u32 fs = 0x0;
	virtual_addr_t mva = (va < 0x02000000) ? 
			     (va + vcpu->sregs->cp15.c13_fcse): va;

	/* Get the required page for vcpu */
	if (vcpu->sregs->cp15.c1_sctlr & SCTLR_M_MASK) {
		/* MMU enabled for vcpu */
		rc = ttbl_walk_v6(vcpu, mva, access_type, is_user, pg, &fs);
		if (rc) {
			/* FIXME: should be ORed with (pg->dom & 0xF) */
			return (fs << 4) | ((vcpu->sregs->cp15.c3 >> pg->dom) & 0x3);
		}
		pg->va = va;
	} else {
		/* MMU disabled for vcpu */
		vmm_memset(pg, 0, sizeof(cpu_page_t));
		pg->pa = mva;
		pg->va = va;
		pg->sz = TTBL_L2TBL_SMALL_PAGE_SIZE;
		pg->ap = TTBL_AP_SRW_URW;
	}
	pg->pa &= ~(pg->sz - 1);
	pg->va &= ~(pg->sz - 1);

	return 0;
}

static int cpu_vcpu_cp15_assert_fault(vmm_vcpu_t * vcpu, 
				      vmm_user_regs_t * regs, 
				      u32 far, u32 fs, u32 dom,
				      u32 wnr, u32 xn)
{
	u32 fsr = 0x0;
	if (!(vcpu->sregs->cp15.c1_sctlr & SCTLR_M_MASK)) {
		cpu_vcpu_halt(vcpu, regs);
		return VMM_EFAIL;
	}
	if (xn) {
		fsr |= ((fs >> 4) << DFSR_FS4_SHIFT);
		fsr |= (fs & DFSR_FS_MASK);
		fsr |= ((wnr << DFSR_WNR_SHIFT) & DFSR_WNR_MASK);
		fsr |= ((dom << DFSR_DOM_SHIFT) & DFSR_DOM_MASK);
		vcpu->sregs->cp15.c5_dfsr = fsr;
		vcpu->sregs->cp15.c6_dfar = far;
		vmm_vcpu_irq_assert(vcpu, CPU_DATA_ABORT_IRQ, 0x0);
	} else {
		fsr |= ((fs >> 4) << IFSR_FS4_SHIFT);
		fsr |= (fs & IFSR_FS_MASK);
		vcpu->sregs->cp15.c5_ifsr = fsr;
		vcpu->sregs->cp15.c6_ifar = far;
		vmm_vcpu_irq_assert(vcpu, CPU_PREFETCH_ABORT_IRQ, 0x0);
	}
	return VMM_OK;
}

int cpu_vcpu_cp15_trans_fault(vmm_vcpu_t * vcpu, 
			      vmm_user_regs_t * regs, 
			      u32 far, u32 fs, u32 dom, 
			      u32 wnr, u32 xn, bool force_user)
{
	u32 ecode, reg_flags;
	bool is_user;
	int rc, access_type;
	cpu_page_t pg;
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
		if ((vcpu->sregs->cpsr & CPSR_MODE_MASK) == CPSR_MODE_USER) {
			is_user = TRUE;
		} else {
			is_user = FALSE;
		}
	}

	if ((ecode = cpu_vcpu_cp15_find_page(vcpu, far, 
					access_type, is_user, 
					&pg))) {
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
		return rc;
	}
	if (availsz < TTBL_L2TBL_SMALL_PAGE_SIZE) {
		return rc;
	}
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
	case TTBL_AP_SR_U:
		pg.dom = TTBL_L1TBL_TTE_DOM_VCPU_SUPER;
		pg.ap = TTBL_AP_SRW_UR;
		break;
	case TTBL_AP_SR_UR_DEPRICATED:
	case TTBL_AP_SR_UR:
		pg.dom = TTBL_L1TBL_TTE_DOM_VCPU_USER;
		pg.ap = TTBL_AP_SRW_UR;
		break;
	default:
		pg.dom = TTBL_L1TBL_TTE_DOM_VCPU_USER;
		pg.ap = TTBL_AP_S_U;
		break;
	};
	if (reg_flags & VMM_REGION_VIRTUAL) {
		switch (pg.ap) {
		case TTBL_AP_SRW_U:
			pg.ap = TTBL_AP_S_U;
			break;
		case TTBL_AP_SRW_UR:
			pg.ap = TTBL_AP_SR_U;
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
	if (pg.c && (reg_flags & VMM_REGION_CACHEABLE)) {
		pg.c = 1;
	} else {
		pg.c = 0;
	}

	return cpu_vcpu_cp15_vtlb_update(vcpu, &pg);
}

int cpu_vcpu_cp15_access_fault(vmm_vcpu_t * vcpu, 
			       vmm_user_regs_t * regs, 
			       u32 far, u32 fs, u32 dom, 
			       u32 wnr, u32 xn)
{
	/* We don't do anything about access fault */
	/* Assert fault to vcpu */
	return cpu_vcpu_cp15_assert_fault(vcpu, regs, far, fs, dom, wnr, xn);
}

int cpu_vcpu_cp15_domain_fault(vmm_vcpu_t * vcpu, 
			       vmm_user_regs_t * regs, 
			       u32 far, u32 fs, u32 dom, 
			       u32 wnr, u32 xn)
{
	int rc = VMM_OK;
	cpu_page_t pg;
	/* Try to retrive the faulting page */
	if ((rc = cpu_mmu_get_page(vcpu->sregs->cp15.l1, far, &pg))) {
		cpu_vcpu_halt(vcpu, regs);
		return rc;
	}
	if (((vcpu->sregs->cpsr & CPSR_MODE_MASK) == CPSR_MODE_USER) &&
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

int cpu_vcpu_cp15_perm_fault(vmm_vcpu_t * vcpu, 
			     vmm_user_regs_t * regs, 
			     u32 far, u32 fs, u32 dom, 
			     u32 wnr, u32 xn)
{
	int rc = VMM_OK;
	cpu_page_t pg;
	/* Try to retrive the faulting page */
	if ((rc = cpu_mmu_get_page(vcpu->sregs->cp15.l1, far, &pg))) {
		cpu_vcpu_halt(vcpu, regs);
		return rc;
	}
	/* Check if vcpu was trying read/write to virtual space */
	if (xn && ((pg.ap == TTBL_AP_SRW_U) || (pg.ap == TTBL_AP_SR_U))) {
		/* Emulate load/store instructions */
		if (regs->cpsr & CPSR_THUMB_ENABLED) {
			return cpu_vcpu_emulate_thumb_inst(vcpu, regs, FALSE);
		} else {
			return cpu_vcpu_emulate_arm_inst(vcpu, regs, FALSE);
		}
	} 
	/* Remove fault address from VTLB and restart.
	 * Doing this will force us to do TTBL walk If MMU 
	 * is enabled then appropriate fault will be generated 
	 */
	return cpu_vcpu_cp15_vtlb_flush_va(vcpu, far);
}

bool cpu_vcpu_cp15_read(vmm_vcpu_t * vcpu, 
			vmm_user_regs_t *regs,
			u32 opc1, u32 opc2, u32 CRn, u32 CRm, 
			u32 *data)
{
	*data = 0x0;
	switch (CRn) {
	case 0: /* ID codes.  */
		switch (opc1) {
		case 0:
			switch (CRm) {
			case 0:
				switch (opc2) {
				case 0: /* Device ID.  */
					*data = vcpu->sregs->cp15.c0_cpuid;
					break;
				case 1: /* Cache Type.  */
					*data = vcpu->sregs->cp15.c0_cachetype;
					break;
				case 2: /* TCM status.  */
					*data = 0;
					break;
				case 3: /* TLB type register.  */
					*data = 0; /* No lockable TLB entries.  */
					break;
				case 5: /* MPIDR */
					/* The MPIDR was standardised in v7; prior to
					 * this it was implemented only in the 11MPCore.
					 * For all other pre-v7 cores it does not exist.
					 */
					if (arm_feature(vcpu, ARM_FEATURE_V7) ||
						arm_cpuid(vcpu) == ARM_CPUID_ARM11MPCORE) {
						int mpidr = vcpu->subid;
						/* We don't support setting cluster ID ([8..11])
						 * so these bits always RAZ.
						 */
						if (arm_feature(vcpu, ARM_FEATURE_V7MP)) {
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
				*data =  vcpu->sregs->cp15.c0_c1[opc2];
				break;
			case 2:
				if (!arm_feature(vcpu, ARM_FEATURE_V6))
					goto bad_reg;
				*data = vcpu->sregs->cp15.c0_c2[opc2];
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
				*data = vcpu->sregs->cp15.c0_ccsid[vcpu->sregs->cp15.c0_cssel];
				break;
			case 1:
				*data = vcpu->sregs->cp15.c0_clid;
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
			*data = vcpu->sregs->cp15.c0_cssel;
			break;
		default:
			goto bad_reg;
		};
		break;
	case 1: /* System configuration.  */
		switch (opc2) {
		case 0: /* Control register.  */
			*data = vcpu->sregs->cp15.c1_sctlr;
			break;
		case 1: /* Auxiliary control register.  */
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
				break;
			default:
				goto bad_reg;
			}
			break;
		case 2: /* Coprocessor access register.  */
			*data = vcpu->sregs->cp15.c1_coproc;
			break;
		default:
			goto bad_reg;
		};
		break;
	case 2: /* MMU Page table control / MPU cache control.  */
		switch (opc2) {
		case 0:
			*data = vcpu->sregs->cp15.c2_base0;
			break;
		case 1:
			*data = vcpu->sregs->cp15.c2_base1;
			break;
		case 2:
			*data = vcpu->sregs->cp15.c2_control;
			break;
		default:
			goto bad_reg;
		};
		break;
	case 3: /* MMU Domain access control / MPU write buffer control.  */
		*data = vcpu->sregs->cp15.c3;
		break;
	case 4: /* Reserved.  */
		goto bad_reg;
	case 5: /* MMU Fault status / MPU access permission.  */
		switch (opc2) {
		case 0:
			*data = vcpu->sregs->cp15.c5_dfsr;
			break;
		case 1:
			*data = vcpu->sregs->cp15.c5_ifsr;
			break;
		default:
			goto bad_reg;
		};
		break;
	case 6: /* MMU Fault address.  */
		switch (opc2) {
		case 0:
			*data = vcpu->sregs->cp15.c6_dfar;
			break;
		case 1:
			if (arm_feature(vcpu, ARM_FEATURE_V6)) {
				/* Watchpoint Fault Adrress.  */
				*data = 0; /* Not implemented.  */
			} else {
				/* Instruction Fault Adrress.  */
				/* Arm9 doesn't have an IFAR, but implementing it anyway
				 * shouldn't do any harm.  */
				*data = vcpu->sregs->cp15.c6_ifar;
			}
			break;
		case 2:
			if (arm_feature(vcpu, ARM_FEATURE_V6)) {
				/* Instruction Fault Adrress.  */
				*data = vcpu->sregs->cp15.c6_ifar;
			} else {
				goto bad_reg;
			}
			break;
		default:
			goto bad_reg;
		};
		break;
	case 7: /* Cache control.  */
		if (CRm == 4 && opc1 == 0 && opc2 == 0) {
			*data = vcpu->sregs->cp15.c7_par;
			break;
		}
		/* FIXME: Should only clear Z flag if destination is r15.  */
		regs->cpsr &= ~CPSR_ZERO_MASK;
		*data = 0;
		break;
	case 8: /* MMU TLB control.  */
		goto bad_reg;
	case 9: /* Cache lockdown.  */
		switch (opc1) {
		case 0: /* L1 cache.  */
			switch (opc2) {
			case 0:
				*data = vcpu->sregs->cp15.c9_data;
				break;
			case 1:
				*data = vcpu->sregs->cp15.c9_insn;
				break;
			default:
				goto bad_reg;
			};
			break;
		case 1: /* L2 cache */
			if (CRm != 0)
				goto bad_reg;
			/* L2 Lockdown and Auxiliary control.  */
			*data = 0;
			break;
		default:
			goto bad_reg;
		};
		break;
	case 10: /* MMU TLB lockdown.  */
		/* ??? TLB lockdown not implemented.  */
		*data = 0;
		break;
	case 11: /* TCM DMA control.  */
	case 12: /* Reserved.  */
		goto bad_reg;
	case 13: /* Process ID.  */
		switch (opc2) {
		case 0:
			*data = vcpu->sregs->cp15.c13_fcse;
			break;
		case 1:
			*data = vcpu->sregs->cp15.c13_context;
			break;
		case 2:
			/* TPIDRURW */
			*data = vcpu->sregs->cp15.c13_tls1;
			break;
		case 3:
			/* TPIDRURO */
			*data = vcpu->sregs->cp15.c13_tls2;
			break;
		case 4:
			/* TPIDRPRW */
			*data = vcpu->sregs->cp15.c13_tls3;
			break;
		default:
			goto bad_reg;
		};
		break;
	case 14: /* Reserved.  */
		goto bad_reg;
	case 15: /* Implementation specific.  */
		*data = 0;
		break;
	}
	return TRUE;
bad_reg:
	return FALSE;
}

bool cpu_vcpu_cp15_write(vmm_vcpu_t * vcpu, 
			 vmm_user_regs_t *regs,
			 u32 opc1, u32 opc2, u32 CRn, u32 CRm, 
			 u32 data)
{
	switch (CRn) {
	case 0:
		/* ID codes.  */
		if (arm_feature(vcpu, ARM_FEATURE_V7) && 
		    (opc1 == 2) && (CRm == 0) && (opc2 == 0)) {
			vcpu->sregs->cp15.c0_cssel = data & 0xf;
			break;
		}
		goto bad_reg;
	case 1: /* System configuration.  */
		switch (opc2) {
		case 0:
			vcpu->sregs->cp15.c1_sctlr = data;
			/* ??? Lots of these bits are not implemented.  */
			/* This may enable/disable the MMU, so do a TLB flush. */
			cpu_vcpu_cp15_vtlb_flush(vcpu);
			break;
		case 1: /* Auxiliary control register.  */
			/* Not implemented.  */
			break;
		case 2:
			if (vcpu->sregs->cp15.c1_coproc != data) {
				vcpu->sregs->cp15.c1_coproc = data;
			}
			break;
		default:
			goto bad_reg;
		};
		break;
	case 2: /* MMU Page table control / MPU cache control.  */
		switch (opc2) {
		case 0:
			vcpu->sregs->cp15.c2_base0 = data;
			break;
		case 1:
			vcpu->sregs->cp15.c2_base1 = data;
			break;
		case 2:
			data &= 7;
			vcpu->sregs->cp15.c2_control = data;
			vcpu->sregs->cp15.c2_mask = ~(((u32)0xffffffffu) >> data);
			vcpu->sregs->cp15.c2_base_mask = ~((u32)0x3fffu >> data);
			break;
		default:
			goto bad_reg;
		};
		break;
	case 3: /* MMU Domain access control / MPU write buffer control.  */
		vcpu->sregs->cp15.c3 = data;
		/* Flush TLB as domain not tracked in TLB */
		cpu_vcpu_cp15_vtlb_flush(vcpu);
		break;
	case 4: /* Reserved.  */
		goto bad_reg;
	case 5: /* MMU Fault status / MPU access permission.  */
		switch (opc2) {
		case 0:
			vcpu->sregs->cp15.c5_dfsr = data;
			break;
		case 1:
			vcpu->sregs->cp15.c5_ifsr = data;
			break;
		default:
			goto bad_reg;
		};
		break;
	case 6: /* MMU Fault address / MPU base/size.  */
		switch (opc2) {
		case 0:
			vcpu->sregs->cp15.c6_dfar = data;
			break;
		case 1: /* ??? This is WFAR on armv6 */
		case 2:
			vcpu->sregs->cp15.c6_ifar = data;
			break;
		default:
			goto bad_reg;
		}
		break;
	case 7: /* Cache control.  */
		vcpu->sregs->cp15.c15_i_max = 0x000;
		vcpu->sregs->cp15.c15_i_min = 0xff0;
		if (opc1 != 0) {
			goto bad_reg;
		}
		/* No cache, so nothing to do except VA->PA translations. */
		if (arm_feature(vcpu, ARM_FEATURE_VAPA)) {
			switch (CRm) {
			case 4:
				if (arm_feature(vcpu, ARM_FEATURE_V7)) {
					vcpu->sregs->cp15.c7_par = data & 0xfffff6ff;
				} else {
					vcpu->sregs->cp15.c7_par = data & 0xfffff1ff;
				}
				break;
			case 8: 
				{
					cpu_page_t pg;
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
							vcpu->sregs->cp15.c7_par = (pg.pa & 0xff000000) | 1 << 1;
						} else {
							vcpu->sregs->cp15.c7_par = pg.pa & 0xfffff000;
						}
					} else {
						vcpu->sregs->cp15.c7_par = (((ret >> 9) & 0x1) << 6) |
									   (((ret >> 4) & 0x1F) << 1) | 1;
					}
				} 
				break;
			default:
				goto bad_reg;
			};
		}
		break;
	case 8: /* MMU TLB control.  */
		switch (opc2) {
		case 0: /* Invalidate all.  */
			cpu_vcpu_cp15_vtlb_flush(vcpu);
			break;
		case 1: /* Invalidate single TLB entry.  */
			cpu_vcpu_cp15_vtlb_flush_va(vcpu, data);
			break;
		case 2: /* Invalidate on ASID.  */
			cpu_vcpu_cp15_vtlb_flush(vcpu);
			break;
		case 3: /* Invalidate single entry on MVA.  */
			/* ??? This is like case 1, but ignores ASID.  */
			cpu_vcpu_cp15_vtlb_flush(vcpu);
			break;
		default:
			goto bad_reg;
		}
		break;
	case 9:
		switch (CRm) {
		case 0: /* Cache lockdown.  */
			switch (opc1) {
			case 0: /* L1 cache.  */
				switch (opc2) {
				case 0:
					vcpu->sregs->cp15.c9_data = data;
					break;
				case 1:
					vcpu->sregs->cp15.c9_insn = data;
					break;
				default:
					goto bad_reg;
				}
				break;
			case 1: /* L2 cache.  */
				/* Ignore writes to L2 lockdown/auxiliary registers.  */
				break;
			default:
				goto bad_reg;
			}
			break;
		case 1: /* TCM memory region registers.  */
			/* Not implemented.  */
			goto bad_reg;
		case 12: /* Performance monitor control */
			/* Performance monitors are implementation defined in v7,
			 * but with an ARM recommended set of registers, which we
			 * follow (although we don't actually implement any counters)
			 */
			if (!arm_feature(vcpu, ARM_FEATURE_V7)) {
				goto bad_reg;
			}
			switch (opc2) {
			case 0: /* performance monitor control register */
				/* only the DP, X, D and E bits are writable */
				vcpu->sregs->cp15.c9_pmcr &= ~0x39;
				vcpu->sregs->cp15.c9_pmcr |= (data & 0x39);
				break;
			case 1: /* Count enable set register */
				data &= (1 << 31);
				vcpu->sregs->cp15.c9_pmcnten |= data;
				break;
			case 2: /* Count enable clear */
				data &= (1 << 31);
				vcpu->sregs->cp15.c9_pmcnten &= ~data;
				break;
			case 3: /* Overflow flag status */
				vcpu->sregs->cp15.c9_pmovsr &= ~data;
				break;
			case 4: /* Software increment */
				/* RAZ/WI since we don't implement 
				 * the software-count event */
				break;
			case 5: /* Event counter selection register */
				/* Since we don't implement any events, writing to this register
				 * is actually UNPREDICTABLE. So we choose to RAZ/WI.
				 */
				break;
			default:
				goto bad_reg;
			}
			break;
		case 13: /* Performance counters */
			if (!arm_feature(vcpu, ARM_FEATURE_V7)) {
				goto bad_reg;
			}
			switch (opc2) {
			case 0: /* Cycle count register: not implemented, so RAZ/WI */
				break;
			case 1: /* Event type select */
				vcpu->sregs->cp15.c9_pmxevtyper = data & 0xff;
				break;
			case 2: /* Event count register */
				/* Unimplemented (we have no events), RAZ/WI */
				break;
			default:
				goto bad_reg;
			}
			break;
		case 14: /* Performance monitor control */
			if (!arm_feature(vcpu, ARM_FEATURE_V7)) {
				goto bad_reg;
			}
			switch (opc2) {
			case 0: /* user enable */
				vcpu->sregs->cp15.c9_pmuserenr = data & 1;
				/* changes access rights for cp registers, so flush tbs */
				break;
			case 1: /* interrupt enable set */
				/* We have no event counters so only the C bit can be changed */
				data &= (1 << 31);
				vcpu->sregs->cp15.c9_pminten |= data;
				break;
			case 2: /* interrupt enable clear */
				data &= (1 << 31);
				vcpu->sregs->cp15.c9_pminten &= ~data;
				break;
			}
			break;
		default:
			goto bad_reg;
		}
		break;
	case 10: /* MMU TLB lockdown.  */
		/* ??? TLB lockdown not implemented.  */
		break;
	case 12: /* Reserved.  */
		goto bad_reg;
	case 13: /* Process ID.  */
		switch (opc2) {
		case 0:
			/* Unlike real hardware the xvisor TLB uses virtual addresses,
			 * not modified virtual addresses, so this causes a TLB flush.
			 */
			if (vcpu->sregs->cp15.c13_fcse != data) {
				cpu_vcpu_cp15_vtlb_flush(vcpu);
			}
			vcpu->sregs->cp15.c13_fcse = data;
			break;
		case 1:
			/* This changes the ASID, so do a TLB flush.  */
			if (vcpu->sregs->cp15.c13_context != data && 
			    !arm_feature(vcpu, ARM_FEATURE_MPU)) {
				cpu_vcpu_cp15_vtlb_flush(vcpu);
			}
			vcpu->sregs->cp15.c13_context = data;
			break;
		case 2:
			/* TPIDRURW */
			vcpu->sregs->cp15.c13_tls1 = data;
			write_tpidrurw(data);
			break;
		case 3:
			/* TPIDRURO */
			vcpu->sregs->cp15.c13_tls2 = data;
			write_tpidruro(data);
			break;
		case 4:
			/* TPIDRPRW */
			vcpu->sregs->cp15.c13_tls3 = data;
			write_tpidrprw(data);
			break;
		default:
			goto bad_reg;
		}
		break;
	case 14: /* Reserved.  */
		goto bad_reg;
	case 15: /* Implementation specific.  */
		break;
	}
	return TRUE;
bad_reg:
	return FALSE;
}

int cpu_vcpu_cp15_mem_read(vmm_vcpu_t * vcpu, 
			   vmm_user_regs_t * regs,
			   virtual_addr_t addr, 
			   void *dst, u32 dst_len,
			   bool force_unpriv)
{
	int rc = VMM_OK;
	bool is_user = FALSE;
	u32 vind, ecode;
	cpu_page_t pg;
	if ((addr & ~(sizeof(vcpu->sregs->cp15.ovect) - 1)) == 
					vcpu->sregs->cp15.ovect_base) {
		if (force_unpriv) {
			is_user = TRUE;
		} else {
			if ((vcpu->sregs->cpsr & CPSR_MODE_MASK) == CPSR_MODE_USER) {
				is_user = TRUE;
			} else {
				is_user = FALSE;
			}
		}
		if ((ecode = cpu_vcpu_cp15_find_page(vcpu, addr, 
					CP15_ACCESS_READ, is_user, &pg))) {
			cpu_vcpu_cp15_assert_fault(vcpu, regs, 
			addr, (ecode >> 4), (ecode & 0xF), 0, 1);
			return VMM_EFAIL;
		}
		vind = addr & (sizeof(vcpu->sregs->cp15.ovect) - 1);
		switch (dst_len) {
		case 4:
			vind &= ~(0x4 - 1);
			vind /= 0x4;
			*((u32 *)dst) = vcpu->sregs->cp15.ovect[vind];
			break;
		case 2:
			vind &= ~(0x2 - 1);
			vind /= 0x2;
			*((u16 *)dst) = ((u16 *)vcpu->sregs->cp15.ovect)[vind];
			break;
		case 1:
			*((u8 *)dst) = ((u8 *)vcpu->sregs->cp15.ovect)[vind];
			break;
		default:
			return VMM_EFAIL;
			break;
		};
	} else {
		rc = cpu_mmu_get_page(vcpu->sregs->cp15.l1, addr, &pg);
		if (rc == VMM_ENOTAVAIL) {
			if (pg.va) {
				rc = cpu_vcpu_cp15_trans_fault(vcpu, regs, 
				addr, DFSR_FS_TRANS_FAULT_PAGE, 0, 0, 1, force_unpriv);
			} else {
				rc = cpu_vcpu_cp15_trans_fault(vcpu, regs, 
				addr, DFSR_FS_TRANS_FAULT_SECTION, 0, 0, 1, force_unpriv);
			}
			if (!rc) {
				rc = cpu_mmu_get_page(vcpu->sregs->cp15.l1, addr, &pg);
			}
		}
		if (rc) {
			cpu_vcpu_halt(vcpu, regs);
			return rc;
		}
		switch(pg.ap) {
		case TTBL_AP_SR_U:
		case TTBL_AP_SRW_U:
			return vmm_devemu_emulate_read(vcpu->guest, 
				(addr - pg.va) + pg.pa, dst, dst_len);
			break;
		case TTBL_AP_SRW_UR:
		case TTBL_AP_SRW_URW:
			switch (dst_len) {
			case 4:
				*((u32 *)dst) = *((u32 *)addr);
				break;
			case 2:
				*((u16 *)dst) = *((u16 *)addr);
				break;
			case 1:
				*((u8 *)dst) = *((u8 *)addr);
				break;
			default:
				return VMM_EFAIL;
				break;
			};
			break;
		default:
			/* Remove fault address from VTLB and restart.
			 * Doing this will force us to do TTBL walk If MMU 
			 * is enabled then appropriate fault will be generated 
			 */
			cpu_vcpu_cp15_vtlb_flush_va(vcpu, addr);
			return VMM_EFAIL;
			break;
		};
	}
	return VMM_OK;
}

int cpu_vcpu_cp15_mem_write(vmm_vcpu_t * vcpu, 
			    vmm_user_regs_t * regs,
			    virtual_addr_t addr, 
			    void *src, u32 src_len,
			    bool force_unpriv)
{
	int rc = VMM_OK;
	bool is_user = FALSE;
	u32 vind, ecode;
	cpu_page_t pg;
	if ((addr & ~(sizeof(vcpu->sregs->cp15.ovect) - 1)) == 
					vcpu->sregs->cp15.ovect_base) {
		if (force_unpriv) {
			is_user = TRUE;
		} else {
			if ((vcpu->sregs->cpsr & CPSR_MODE_MASK) == CPSR_MODE_USER) {
				is_user = TRUE;
			} else {
				is_user = FALSE;
			}
		}
		if ((ecode = cpu_vcpu_cp15_find_page(vcpu, addr, 
					CP15_ACCESS_WRITE, is_user, &pg))) {
			cpu_vcpu_cp15_assert_fault(vcpu, regs, 
			addr, (ecode >> 4), (ecode & 0xF), 1, 1);
			return VMM_EFAIL;
		}
		vind = addr & (sizeof(vcpu->sregs->cp15.ovect) - 1);
		switch (src_len) {
		case 4:
			vind &= ~(0x4 - 1);
			vind /= 0x4;
			vcpu->sregs->cp15.ovect[vind] = *((u32 *)src);
			break;
		case 2:
			vind &= ~(0x2 - 1);
			vind /= 0x2;
			((u16 *)vcpu->sregs->cp15.ovect)[vind] = *((u16 *)src);
			break;
		case 1:
			((u8 *)vcpu->sregs->cp15.ovect)[vind] = *((u8 *)src);
			break;
		default:
			return VMM_EFAIL;
			break;
		};
	} else {
		rc = cpu_mmu_get_page(vcpu->sregs->cp15.l1, addr, &pg);
		if (rc == VMM_ENOTAVAIL) {
			if (pg.va) {
				rc = cpu_vcpu_cp15_trans_fault(vcpu, regs, addr, 
				DFSR_FS_TRANS_FAULT_PAGE, 0, 1, 1, force_unpriv);
			} else {
				rc = cpu_vcpu_cp15_trans_fault(vcpu, regs, addr, 
				DFSR_FS_TRANS_FAULT_SECTION, 0, 1, 1, force_unpriv);
			}
			if (!rc) {
				rc = cpu_mmu_get_page(vcpu->sregs->cp15.l1, addr, &pg);
			}
		}
		if (rc) {
			cpu_vcpu_halt(vcpu, regs);
			return rc;
		}
		switch(pg.ap) {
		case TTBL_AP_SRW_U:
			return vmm_devemu_emulate_write(vcpu->guest, 
				(addr - pg.va) + pg.pa, src, src_len);
			break;
		case TTBL_AP_SRW_URW:
			switch (src_len) {
			case 4:
				*((u32 *)addr) = *((u32 *)src);
				break;
			case 2:
				*((u16 *)addr) = *((u16 *)src);
				break;
			case 1:
				*((u8 *)addr) = *((u8 *)src);
				break;
			default:
				return VMM_EFAIL;
				break;
			};
			break;
		default:
			/* Remove fault address from VTLB and restart.
			 * Doing this will force us to do TTBL walk If MMU 
			 * is enabled then appropriate fault will be generated 
			 */
			cpu_vcpu_cp15_vtlb_flush_va(vcpu, addr);
			return VMM_EFAIL;
			break;
		};
	}
	return VMM_OK;
}

virtual_addr_t cpu_vcpu_cp15_vector_addr(vmm_vcpu_t * vcpu, u32 irq_no)
{
	virtual_addr_t vaddr;
	irq_no = irq_no % CPU_IRQ_NR;

	if (vcpu->sregs->cp15.c1_sctlr & SCTLR_V_MASK) {
		vaddr = CPU_IRQ_HIGHVEC_BASE;
	} else {
		vaddr = CPU_IRQ_LOWVEC_BASE;
	}

	if (vcpu->sregs->cp15.ovect_base == vaddr) {
		/* FIXME: We assume that guest will use 
		 * LDR PC, [PC, #xx] as first instruction of irq handler */
		vaddr = vcpu->sregs->cp15.ovect[irq_no + 8];
	} else {
		vaddr += 4*irq_no;
	}

	return vaddr;
}

void cpu_vcpu_cp15_sync_cpsr(vmm_vcpu_t * vcpu)
{
	vmm_vcpu_t * cvcpu = vmm_scheduler_current_vcpu();
	vcpu->sregs->cp15.dacr &= 
			~(0x3 << (2 * TTBL_L1TBL_TTE_DOM_VCPU_SUPER));
	vcpu->sregs->cp15.dacr &= 
			~(0x3 << (2 * TTBL_L1TBL_TTE_DOM_VCPU_SUPER_RW_USER_R));
	if ((vcpu->sregs->cpsr & CPSR_MODE_MASK) == CPSR_MODE_USER) {
		vcpu->sregs->cp15.dacr |= 
		(TTBL_DOM_NOACCESS << (2 * TTBL_L1TBL_TTE_DOM_VCPU_SUPER));
		vcpu->sregs->cp15.dacr |= 
		(TTBL_DOM_CLIENT << (2 * TTBL_L1TBL_TTE_DOM_VCPU_SUPER_RW_USER_R));
	} else {
		vcpu->sregs->cp15.dacr |= 
		(TTBL_DOM_CLIENT << (2 * TTBL_L1TBL_TTE_DOM_VCPU_SUPER));
		vcpu->sregs->cp15.dacr |= 
		(TTBL_DOM_MANAGER << (2 * TTBL_L1TBL_TTE_DOM_VCPU_SUPER_RW_USER_R));
	}
	if (cvcpu->id == vcpu->id) {
		cpu_mmu_chdacr(vcpu->sregs->cp15.dacr);
	}
}

void cpu_vcpu_cp15_switch_context(vmm_vcpu_t * tvcpu, vmm_vcpu_t * vcpu)
{
	if (tvcpu && tvcpu->is_normal) {
		tvcpu->sregs->cp15.c13_tls1 = read_tpidrurw();
		tvcpu->sregs->cp15.c13_tls2 = read_tpidruro();
		tvcpu->sregs->cp15.c13_tls3 = read_tpidrprw();
	}
	if (vcpu->is_normal) {
		cpu_mmu_chdacr(vcpu->sregs->cp15.dacr);
		cpu_mmu_chttbr(vcpu->sregs->cp15.l1);
		write_tpidrurw(vcpu->sregs->cp15.c13_tls1);
		write_tpidruro(vcpu->sregs->cp15.c13_tls2);
		write_tpidrprw(vcpu->sregs->cp15.c13_tls3);
	} else {
		cpu_mmu_chttbr(cpu_mmu_l1tbl_default());
	}
}

static u32 cortexa9_cp15_c0_c1[8] =
{ 0x1031, 0x11, 0x000, 0, 0x00100103, 0x20000000, 0x01230000, 0x00002111 };

static u32 cortexa9_cp15_c0_c2[8] =
{ 0x00101111, 0x13112111, 0x21232041, 0x11112131, 0x00111142, 0, 0, 0 };

static u32 cortexa8_cp15_c0_c1[8] =
{ 0x1031, 0x11, 0x400, 0, 0x31100003, 0x20000000, 0x01202000, 0x11 };

static u32 cortexa8_cp15_c0_c2[8] =
{ 0x00101111, 0x12112111, 0x21232031, 0x11112131, 0x00111142, 0, 0, 0 };

int cpu_vcpu_cp15_init(vmm_vcpu_t * vcpu, u32 cpuid)
{
	int rc = VMM_OK;
	u32 vtlb_count;

	if (!vcpu->reset_count) {
		vmm_memset(&vcpu->sregs->cp15, 0, sizeof(vcpu->sregs->cp15));
		vcpu->sregs->cp15.l1 = cpu_mmu_l1tbl_alloc();
		vcpu->sregs->cp15.dacr = 0x0;
		vcpu->sregs->cp15.dacr |= (TTBL_DOM_CLIENT << 
				(TTBL_L1TBL_TTE_DOM_VCPU_SUPER * 2));
		vcpu->sregs->cp15.dacr |= (TTBL_DOM_MANAGER << 
				(TTBL_L1TBL_TTE_DOM_VCPU_SUPER_RW_USER_R * 2));
		vcpu->sregs->cp15.dacr |= (TTBL_DOM_CLIENT << 
				(TTBL_L1TBL_TTE_DOM_VCPU_USER * 2));
		vtlb_count = CONFIG_ARMV7A_VTLB_ENTRY_COUNT;
		vcpu->sregs->cp15.vtlb.count = vtlb_count;
		vcpu->sregs->cp15.vtlb.table = vmm_malloc(vtlb_count *
						sizeof(cpu_vtlb_entry_t));
		vmm_memset(vcpu->sregs->cp15.vtlb.table, 0, vtlb_count * 
						sizeof(cpu_vtlb_entry_t));
		vcpu->sregs->cp15.vtlb.victim = 0;

		if (read_sctlr() & SCTLR_V_MASK) {
			vcpu->sregs->cp15.ovect_base = CPU_IRQ_HIGHVEC_BASE;
		} else {
			vcpu->sregs->cp15.ovect_base = CPU_IRQ_LOWVEC_BASE;
		}
	} else {
		if ((rc = cpu_vcpu_cp15_vtlb_flush(vcpu))) {
			return rc;
		}
	}

	vcpu->sregs->cp15.c0_cpuid = cpuid;
	vcpu->sregs->cp15.c2_control = 0x0;
	vcpu->sregs->cp15.c2_mask = 0x0;
	vcpu->sregs->cp15.c2_base_mask = 0xFFFFC000;
	vcpu->sregs->cp15.c9_pmcr = (cpuid & 0xFF000000);
	/* Reset values of important registers */
	switch (cpuid) {
	case ARM_CPUID_CORTEXA8:
		vmm_memcpy(vcpu->sregs->cp15.c0_c1, cortexa8_cp15_c0_c1, 
							8 * sizeof(u32));
		vmm_memcpy(vcpu->sregs->cp15.c0_c2, cortexa8_cp15_c0_c2, 
							8 * sizeof(u32));
		vcpu->sregs->cp15.c0_cachetype = 0x82048004;
		vcpu->sregs->cp15.c0_clid = (1 << 27) | (2 << 24) | 3;
		vcpu->sregs->cp15.c0_ccsid[0] = 0xe007e01a; /* 16k L1 dcache. */
		vcpu->sregs->cp15.c0_ccsid[1] = 0x2007e01a; /* 16k L1 icache. */
		vcpu->sregs->cp15.c0_ccsid[2] = 0xf0000000; /* No L2 icache. */
		vcpu->sregs->cp15.c1_sctlr = 0x00c50078;
		break;
	case ARM_CPUID_CORTEXA9:
		vmm_memcpy(vcpu->sregs->cp15.c0_c1, cortexa9_cp15_c0_c1, 
							8 * sizeof(u32));
		vmm_memcpy(vcpu->sregs->cp15.c0_c2, cortexa9_cp15_c0_c2, 
							8 * sizeof(u32));
		vcpu->sregs->cp15.c0_cachetype = 0x80038003;
		vcpu->sregs->cp15.c0_clid = (1 << 27) | (1 << 24) | 3;
		vcpu->sregs->cp15.c0_ccsid[0] = 0xe00fe015; /* 16k L1 dcache. */
		vcpu->sregs->cp15.c0_ccsid[1] = 0x200fe015; /* 16k L1 icache. */
		vcpu->sregs->cp15.c1_sctlr = 0x00c50078;
		break;
	default:
		break;
	};

	return rc;
}

