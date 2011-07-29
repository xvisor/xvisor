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
#include <vmm_devtree.h>
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

enum cpu_vcpu_cp15_fault_types {
	CP15_TRANS_FAULT=0,
	CP15_ACCESS_FAULT=1,
	CP15_DOMAIN_FAULT=2,
	CP15_PERM_FAULT=3,
};

int cpu_vcpu_cp15_assert_fault(vmm_vcpu_t * vcpu, 
				vmm_user_regs_t * regs, 
				u32 type, u32 far, u32 wnr, u32 page, u32 xn)
{
	u32 fs = 0x0, fsr = 0x0;
	if (!(vcpu->sregs.cp15.c1_sctlr & SCTLR_M_MASK)) {
		cpu_vcpu_halt(vcpu, regs);
		return VMM_EFAIL;
	}
	if (xn) {
		switch (type) {
		case CP15_TRANS_FAULT:
			fs = (page) ? DFSR_FS_TRANS_FAULT_PAGE : 
					DFSR_FS_TRANS_FAULT_SECTION;
			break;
		case CP15_ACCESS_FAULT:
			fs = (page) ? DFSR_FS_ACCESS_FAULT_PAGE : 
					DFSR_FS_ACCESS_FAULT_SECTION;
			break;
		case CP15_DOMAIN_FAULT:
			fs = (page) ? DFSR_FS_DOMAIN_FAULT_PAGE : 
					DFSR_FS_DOMAIN_FAULT_SECTION;
			break;
		case CP15_PERM_FAULT:
			fs = (page) ? DFSR_FS_PERM_FAULT_PAGE : 
					DFSR_FS_PERM_FAULT_SECTION;
			break;
		default:
			return VMM_EFAIL;
		};
		fsr |= ((fs >> 4) << DFSR_FS4_SHIFT);
		fsr |= (fs & DFSR_FS_MASK);
		fsr |= ((wnr << DFSR_WNR_SHIFT) & DFSR_WNR_MASK);
		vcpu->sregs.cp15.c5_dfsr = fsr;
		vcpu->sregs.cp15.c6_dfar = far;
		vmm_vcpu_irq_assert(vcpu, CPU_DATA_ABORT_IRQ, 0x0);
	} else {
		switch (type) {
		case CP15_TRANS_FAULT:
			fs = (page) ? IFSR_FS_TRANS_FAULT_PAGE : 
					IFSR_FS_TRANS_FAULT_SECTION;
			break;
		case CP15_ACCESS_FAULT:
			fs = (page) ? IFSR_FS_ACCESS_FAULT_PAGE : 
					IFSR_FS_ACCESS_FAULT_SECTION;
			break;
		case CP15_DOMAIN_FAULT:
			fs = (page) ? IFSR_FS_DOMAIN_FAULT_PAGE : 
					IFSR_FS_DOMAIN_FAULT_SECTION;
			break;
		case CP15_PERM_FAULT:
			fs = (page) ? IFSR_FS_PERM_FAULT_PAGE : 
					IFSR_FS_PERM_FAULT_SECTION;
			break;
		default:
			return VMM_EFAIL;
		};
		fsr |= ((fs >> 4) << IFSR_FS4_SHIFT);
		fsr |= (fs & IFSR_FS_MASK);
		vcpu->sregs.cp15.c5_ifsr = fsr;
		vcpu->sregs.cp15.c6_ifar = far;
		vmm_vcpu_irq_assert(vcpu, CPU_PREFETCH_ABORT_IRQ, 0x0);
	}
	return VMM_OK;
}

int cpu_vcpu_cp15_trans_fault(vmm_vcpu_t * vcpu, 
			      vmm_user_regs_t * regs, 
			      u32 far, u32 wnr, u32 page, u32 xn)
{
	int rc;
	u8 *p_asid = NULL, *p_dom = NULL;
	u32 victim;
	vmm_guest_region_t *reg;
	cpu_page_t *p;

	/* Find out next victim page from shadow TLB */
	victim = vcpu->sregs.cp15.vtlb.victim;
	p = &vcpu->sregs.cp15.vtlb.page[victim];
	p_asid = &vcpu->sregs.cp15.vtlb.page_asid[victim];
	p_dom = &vcpu->sregs.cp15.vtlb.page_dom[victim];
	if (vcpu->sregs.cp15.vtlb.valid[victim]) {
		/* Remove valid victim page from L1 Page Table */
		if ((rc = cpu_mmu_unmap_page(vcpu->sregs.cp15.l1, p))) {
			return rc;
		}
		vcpu->sregs.cp15.vtlb.valid[victim] = 0;
	}

	/* Get the required page for vcpu */
	if (vcpu->sregs.cp15.c1_sctlr & SCTLR_M_MASK) {
		/* FIXME: MMU enabled for vcpu */
	} else {
		/* MMU disabled for vcpu */
		reg = vmm_guest_aspace_getregion(vcpu->guest, far);
		if (!reg) {
			cpu_vcpu_halt(vcpu, regs);
			return VMM_EFAIL;
		}
		p->pa = reg->hphys_addr + (far - reg->gphys_addr);
		p->va = far;
		p->sz = reg->phys_size - (far - reg->gphys_addr);
		if (TTBL_L1TBL_SECTION_PAGE_SIZE <= p->sz) {
			p->sz = TTBL_L1TBL_SECTION_PAGE_SIZE;
		} else if (TTBL_L2TBL_LARGE_PAGE_SIZE <= p->sz) {
			p->sz = TTBL_L2TBL_LARGE_PAGE_SIZE;
		} else {
			p->sz = TTBL_L2TBL_SMALL_PAGE_SIZE;
		}
		p->imp = 0;
		p->dom = TTBL_L1TBL_TTE_DOM_VCPU_NOMMU;
		if (reg->is_virtual) {
			p->ap = TTBL_AP_SRW_U;
		} else {
			p->ap = TTBL_AP_SRW_URW;
		}
		p->xn = 0;
		p->c = 0;
		p->b = 0;
		*p_asid = 0;
		*p_dom = 0;
	}

	/* Add victim page to L1 page table */
	if ((rc = cpu_mmu_map_page(vcpu->sregs.cp15.l1, p))) {
		return rc;
	}

	/* Mark current victim as valid and 
	 * point to next victim page in shadow TLB */
	vcpu->sregs.cp15.vtlb.valid[victim] = 1;
	victim = (victim + 1) % vcpu->sregs.cp15.vtlb.count;
	vcpu->sregs.cp15.vtlb.victim = victim;

	return VMM_OK;
}

int cpu_vcpu_cp15_access_fault(vmm_vcpu_t * vcpu, 
			       vmm_user_regs_t * regs, 
			       u32 far, u32 wnr, u32 page, u32 xn)
{
	/* We don't do anything about access fault */
	/* Assert access fault to vcpu */
	return cpu_vcpu_cp15_assert_fault(vcpu, regs, CP15_ACCESS_FAULT, 
					  far, wnr, page, xn);
}

int cpu_vcpu_cp15_domain_fault(vmm_vcpu_t * vcpu, 
			       vmm_user_regs_t * regs, 
			       u32 far, u32 wnr, u32 page, u32 xn)
{
	int rc = VMM_OK;
	cpu_page_t pg;
	/* Try to retrive the faulting page */
	if ((rc = cpu_mmu_get_page(vcpu->sregs.cp15.l1, far, &pg))) {
		cpu_vcpu_halt(vcpu, regs);
		return rc;
	}
	if (((vcpu->sregs.cpsr & CPSR_MODE_MASK) == CPSR_MODE_USER) &&
	    (pg.dom == TTBL_L1TBL_TTE_DOM_VCPU_SUPER)) {
		/* Assert permission fault to VCPU */
		rc = cpu_vcpu_cp15_assert_fault(vcpu, regs, CP15_PERM_FAULT, 
						far, wnr, page, xn);
	} else {
		cpu_vcpu_halt(vcpu, regs);
		rc = VMM_EFAIL;
	}
	return rc;
}

int cpu_vcpu_cp15_perm_fault(vmm_vcpu_t * vcpu, 
			     vmm_user_regs_t * regs, 
			     u32 far, u32 wnr, u32 page, u32 xn)
{
	int rc = VMM_OK;
	cpu_page_t pg;
	/* Try to retrive the faulting page */
	if ((rc = cpu_mmu_get_page(vcpu->sregs.cp15.l1, far, &pg))) {
		cpu_vcpu_halt(vcpu, regs);
		return rc;
	}
	/* Check if vcpu was trying read/write to virtual space */
	if (xn && (pg.ap == TTBL_AP_SRW_U)) {
		/* Emulate load/store instructions */
		if (regs->cpsr & CPSR_THUMB_ENABLED) {
			return cpu_vcpu_emulate_thumb_inst(vcpu, regs, FALSE);
		} else {
			return cpu_vcpu_emulate_arm_inst(vcpu, regs, FALSE);
		}
	} 
	/* Assert permission fault to vcpu */
	return cpu_vcpu_cp15_assert_fault(vcpu, regs, CP15_PERM_FAULT, 
					  far, wnr, page, xn);
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
					*data = vcpu->sregs.cp15.c0_cpuid;
				case 1: /* Cache Type.  */
					*data = vcpu->sregs.cp15.c0_cachetype;
				case 2: /* TCM status.  */
					*data = 0;
				case 3: /* TLB type register.  */
					*data = 0; /* No lockable TLB entries.  */
				case 5: /* MPIDR */
					/* The MPIDR was standardised in v7; prior to
					 * this it was implemented only in the 11MPCore.
					 * For all other pre-v7 cores it does not exist.
					 */
					if (arm_feature(vcpu, ARM_FEATURE_V7) ||
						arm_cpuid(vcpu) == ARM_CPUID_ARM11MPCORE) {
						int mpidr = vmm_scheduler_guest_vcpu_index(vcpu->guest, vcpu);
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
				default:
					goto bad_reg;
				}
			case 1:
				if (!arm_feature(vcpu, ARM_FEATURE_V6))
					goto bad_reg;
				*data =  vcpu->sregs.cp15.c0_c1[opc2];
			case 2:
				if (!arm_feature(vcpu, ARM_FEATURE_V6))
					goto bad_reg;
				*data = vcpu->sregs.cp15.c0_c2[opc2];
			case 3:
			case 4: 
			case 5: 
			case 6: 
			case 7:
		                *data = 0;
			default:
				goto bad_reg;
			}
		case 1:
			/* These registers aren't documented on arm11 cores.  However
			 * Linux looks at them anyway.  */
			if (!arm_feature(vcpu, ARM_FEATURE_V6))
				goto bad_reg;
			if (CRm != 0)
				goto bad_reg;
			if (!arm_feature(vcpu, ARM_FEATURE_V7))
				*data = 0;
			switch (opc2) {
			case 0:
				*data = vcpu->sregs.cp15.c0_ccsid[vcpu->sregs.cp15.c0_cssel];
			case 1:
				*data = vcpu->sregs.cp15.c0_clid;
			case 7:
				*data = 0;
			}
			goto bad_reg;
		case 2:
			if (opc2 != 0 || CRm != 0)
				goto bad_reg;
			*data = vcpu->sregs.cp15.c0_cssel;
		default:
			goto bad_reg;
		}
	case 1: /* System configuration.  */
		switch (opc2) {
		case 0: /* Control register.  */
			*data = vcpu->sregs.cp15.c1_sctlr;
		case 1: /* Auxiliary control register.  */
			if (!arm_feature(vcpu, ARM_FEATURE_AUXCR))
				goto bad_reg;
			switch (arm_cpuid(vcpu)) {
			case ARM_CPUID_ARM1026:
				*data = 1;
			case ARM_CPUID_ARM1136:
			case ARM_CPUID_ARM1136_R2:
				*data = 7;
			case ARM_CPUID_ARM11MPCORE:
				*data = 1;
			case ARM_CPUID_CORTEXA8:
				*data = 2;
			case ARM_CPUID_CORTEXA9:
				*data = 0;
			default:
				goto bad_reg;
			}
		case 2: /* Coprocessor access register.  */
			*data = vcpu->sregs.cp15.c1_coproc;
		default:
			goto bad_reg;
		}
	case 2: /* MMU Page table control / MPU cache control.  */
		switch (opc2) {
		case 0:
			*data = vcpu->sregs.cp15.c2_base0;
		case 1:
			*data = vcpu->sregs.cp15.c2_base1;
		case 2:
			*data = vcpu->sregs.cp15.c2_control;
		default:
			goto bad_reg;
		}
	case 3: /* MMU Domain access control / MPU write buffer control.  */
		*data = vcpu->sregs.cp15.c3;
	case 4: /* Reserved.  */
		goto bad_reg;
	case 5: /* MMU Fault status / MPU access permission.  */
		switch (opc2) {
		case 0:
			*data = vcpu->sregs.cp15.c5_dfsr;
		case 1:
			*data = vcpu->sregs.cp15.c5_ifsr;
		default:
			goto bad_reg;
		}
	case 6: /* MMU Fault address.  */
		switch (opc2) {
		case 0:
			*data = vcpu->sregs.cp15.c6_dfar;
		case 1:
			if (arm_feature(vcpu, ARM_FEATURE_V6)) {
				/* Watchpoint Fault Adrress.  */
				*data = 0; /* Not implemented.  */
			} else {
				/* Instruction Fault Adrress.  */
				/* Arm9 doesn't have an IFAR, but implementing it anyway
				 * shouldn't do any harm.  */
				*data = vcpu->sregs.cp15.c6_ifar;
			}
		case 2:
			if (arm_feature(vcpu, ARM_FEATURE_V6)) {
				/* Instruction Fault Adrress.  */
				*data = vcpu->sregs.cp15.c6_ifar;
			} else {
				goto bad_reg;
			}
		default:
			goto bad_reg;
		}
	case 7: /* Cache control.  */
		if (CRm == 4 && opc1 == 0 && opc2 == 0) {
			*data = vcpu->sregs.cp15.c7_par;
		}
		/* FIXME: Should only clear Z flag if destination is r15.  */
		regs->cpsr &= ~CPSR_COND_ZERO_MASK;
		*data = 0;
	case 8: /* MMU TLB control.  */
		goto bad_reg;
	case 9: /* Cache lockdown.  */
		switch (opc1) {
		case 0: /* L1 cache.  */
			switch (opc2) {
			case 0:
				*data = vcpu->sregs.cp15.c9_data;
			case 1:
				*data = vcpu->sregs.cp15.c9_insn;
			default:
				goto bad_reg;
			}
		case 1: /* L2 cache */
			if (CRm != 0)
				goto bad_reg;
			/* L2 Lockdown and Auxiliary control.  */
			*data = 0;
		default:
			goto bad_reg;
		}
	case 10: /* MMU TLB lockdown.  */
		/* ??? TLB lockdown not implemented.  */
		*data = 0;
	case 11: /* TCM DMA control.  */
	case 12: /* Reserved.  */
		goto bad_reg;
	case 13: /* Process ID.  */
		switch (opc2) {
		case 0:
			*data = vcpu->sregs.cp15.c13_fcse;
		case 1:
			*data = vcpu->sregs.cp15.c13_context;
		default:
			goto bad_reg;
		}
	case 14: /* Reserved.  */
		goto bad_reg;
	case 15: /* Implementation specific.  */
		*data = 0;
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
			vcpu->sregs.cp15.c0_cssel = data & 0xf;
			break;
		}
		goto bad_reg;
	case 1: /* System configuration.  */
		switch (opc2) {
		case 0:
			vcpu->sregs.cp15.c1_sctlr = data;
			/* ??? Lots of these bits are not implemented.  */
			/* This may enable/disable the MMU, so do a TLB flush. */
			/* FIXME: tlb_flush(vcpu, 1); */
			break;
		case 1: /* Auxiliary control register.  */
			/* Not implemented.  */
			break;
		case 2:
			if (vcpu->sregs.cp15.c1_coproc != data) {
				vcpu->sregs.cp15.c1_coproc = data;
				/* ??? Is this safe when called from within a TB? */
				/* FIXME: tb_flush(vcpu); */
			}
			break;
		default:
			goto bad_reg;
		}
		break;
	case 2: /* MMU Page table control / MPU cache control.  */
		switch (opc2) {
		case 0:
			vcpu->sregs.cp15.c2_base0 = data;
			break;
		case 1:
			vcpu->sregs.cp15.c2_base1 = data;
			break;
		case 2:
			data &= 7;
			vcpu->sregs.cp15.c2_control = data;
			vcpu->sregs.cp15.c2_mask = ~(((u32)0xffffffffu) >> data);
			vcpu->sregs.cp15.c2_base_mask = ~((u32)0x3fffu >> data);
			break;
		default:
			goto bad_reg;
		}
		break;
	case 3: /* MMU Domain access control / MPU write buffer control.  */
		vcpu->sregs.cp15.c3 = data;
		/* Flush TLB as domain not tracked in TLB */
		/* FIXME: tlb_flush(vcpu, 1); */
		break;
	case 4: /* Reserved.  */
		goto bad_reg;
	case 5: /* MMU Fault status / MPU access permission.  */
		switch (opc2) {
		case 0:
			vcpu->sregs.cp15.c5_dfsr = data;
			break;
		case 1:
			vcpu->sregs.cp15.c5_ifsr = data;
			break;
		default:
			goto bad_reg;
		}
		break;
	case 6: /* MMU Fault address / MPU base/size.  */
		switch (opc2) {
		case 0:
			vcpu->sregs.cp15.c6_dfar = data;
			break;
		case 1: /* ??? This is WFAR on armv6 */
		case 2:
			vcpu->sregs.cp15.c6_ifar = data;
			break;
		default:
			goto bad_reg;
		}
		break;
	case 7: /* Cache control.  */
		vcpu->sregs.cp15.c15_i_max = 0x000;
		vcpu->sregs.cp15.c15_i_min = 0xff0;
		if (opc1 != 0) {
			goto bad_reg;
		}
		/* No cache, so nothing to do except VA->PA translations. */
		if (arm_feature(vcpu, ARM_FEATURE_V6K)) {
			switch (CRm) {
			case 4:
				if (arm_feature(vcpu, ARM_FEATURE_V7)) {
					vcpu->sregs.cp15.c7_par = data & 0xfffff6ff;
				} else {
					vcpu->sregs.cp15.c7_par = data & 0xfffff1ff;
				}
				break;
			case 8:
				/* FIXME: */
#if 0
				uint32_t phys_addr;
				target_ulong page_size;
				int prot;
				int ret, is_user = opc2 & 2;
				int access_type = opc2 & 1;
				if (opc2 & 4) {
					/* Other states are only available with TrustZone */
					goto bad_reg;
				}
				ret = get_phys_addr(vcpu, data, access_type, is_user,
						&phys_addr, &prot, &page_size);
				if (ret == 0) {
					/* We do not set any attribute bits in the PAR */
					if (page_size == (1 << 24) && 
					    arm_feature(vcpu, ARM_FEATURE_V7)) {
						vcpu->sregs.cp15.c7_par = (phys_addr & 0xff000000) | 1 << 1;
					} else {
						vcpu->sregs.cp15.c7_par = phys_addr & 0xfffff000;
					}
				} else {
					vcpu->sregs.cp15.c7_par = ((ret & (10 << 1)) >> 5) |
							   ((ret & (12 << 1)) >> 6) |
							   ((ret & 0xf) << 1) | 1;
				}
#endif
				break;
			}
		}
		break;
	case 8: /* MMU TLB control.  */
		switch (opc2) {
		case 0: /* Invalidate all.  */
			/* FIXME: tlb_flush(vcpu, 0); */
			break;
		case 1: /* Invalidate single TLB entry.  */
			/* FIXME: tlb_flush_page(vcpu, data & TARGET_PAGE_MASK); */
			break;
		case 2: /* Invalidate on ASID.  */
			/* FIXME: tlb_flush(vcpu, data == 0); */
			break;
		case 3: /* Invalidate single entry on MVA.  */
			/* ??? This is like case 1, but ignores ASID.  */
			/* FIXME: tlb_flush(vcpu, 1); */
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
					vcpu->sregs.cp15.c9_data = data;
					break;
				case 1:
					vcpu->sregs.cp15.c9_insn = data;
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
			/* Unlike real hardware the qemu TLB uses virtual addresses,
			 * not modified virtual addresses, so this causes a TLB flush.
			 */
			if (vcpu->sregs.cp15.c13_fcse != data) {
				/* FIXME: tlb_flush(vcpu, 1); */
			}
			vcpu->sregs.cp15.c13_fcse = data;
			break;
		case 1:
			/* This changes the ASID, so do a TLB flush.  */
			if (vcpu->sregs.cp15.c13_context != data && 
			    !arm_feature(vcpu, ARM_FEATURE_MPU)) {
				/* FIXME: tlb_flush(vcpu, 0); */
			}
			vcpu->sregs.cp15.c13_context = data;
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
			   void *dst, u32 dst_len)
{
	int rc = VMM_OK;
	u32 vind;
	cpu_page_t pg;
	if ((addr & ~(sizeof(vcpu->sregs.cp15.ovect) - 1)) == 
					vcpu->sregs.cp15.ovect_base) {
		vind = addr & (sizeof(vcpu->sregs.cp15.ovect) - 1);
		switch (dst_len) {
		case 4:
			vind &= ~(0x4 - 1);
			vind /= 0x4;
			*((u32 *)dst) = vcpu->sregs.cp15.ovect[vind];
			break;
		case 2:
			vind &= ~(0x2 - 1);
			vind /= 0x2;
			*((u16 *)dst) = ((u16 *)vcpu->sregs.cp15.ovect)[vind];
			break;
		case 1:
			*((u8 *)dst) = ((u8 *)vcpu->sregs.cp15.ovect)[vind];
			break;
		default:
			rc = VMM_EFAIL;
			break;
		};
	} else {
		rc = cpu_mmu_get_page(vcpu->sregs.cp15.l1, addr, &pg);
		if (rc == VMM_ENOTAVAIL) {
			rc = cpu_vcpu_cp15_trans_fault(vcpu, regs, 
						addr, 0, (pg.va) ? 1 : 0, 1);
			if (!rc) {
				rc = cpu_mmu_get_page(vcpu->sregs.cp15.l1, 
						      addr, &pg);
			}
		}
		if (!rc) {
			switch(pg.ap) {
			case TTBL_AP_SR_U:
			case TTBL_AP_SRW_U:
				rc = vmm_devemu_emulate_read(vcpu->guest, 
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
					rc = VMM_EFAIL;
					break;
				};
				break;
			default:
				rc = VMM_EFAIL;
				break;
			};
		}
	}
	if (rc) {
		cpu_vcpu_halt(vcpu, regs);
	}
	return rc;
}

int cpu_vcpu_cp15_mem_write(vmm_vcpu_t * vcpu, 
			    vmm_user_regs_t * regs,
			    virtual_addr_t addr, 
			    void *src, u32 src_len)
{
	int rc = VMM_OK;
	u32 vind;
	cpu_page_t pg;
	if ((addr & ~(sizeof(vcpu->sregs.cp15.ovect) - 1)) == 
					vcpu->sregs.cp15.ovect_base) {
		vind = addr & (sizeof(vcpu->sregs.cp15.ovect) - 1);
		switch (src_len) {
		case 4:
			vind &= ~(0x4 - 1);
			vind /= 0x4;
			vcpu->sregs.cp15.ovect[vind] = *((u32 *)src);
			break;
		case 2:
			vind &= ~(0x2 - 1);
			vind /= 0x2;
			((u16 *)vcpu->sregs.cp15.ovect)[vind] = *((u16 *)src);
			break;
		case 1:
			((u8 *)vcpu->sregs.cp15.ovect)[vind] = *((u8 *)src);
			break;
		default:
			rc = VMM_EFAIL;
			break;
		};
	} else {
		rc = cpu_mmu_get_page(vcpu->sregs.cp15.l1, addr, &pg);
		if (rc == VMM_ENOTAVAIL) {
			rc = cpu_vcpu_cp15_trans_fault(vcpu, regs, 
						addr, 1, (pg.va) ? 1 : 0, 1);
			if (!rc) {
				rc = cpu_mmu_get_page(vcpu->sregs.cp15.l1, 
						      addr, &pg);
			}
		}
		if (!rc) {
			switch(pg.ap) {
			case TTBL_AP_SRW_U:
				rc = vmm_devemu_emulate_write(vcpu->guest, 
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
					rc = VMM_EFAIL;
					break;
				};
				break;
			default:
				rc = VMM_EFAIL;
				break;
			};
		}
	}
	if (rc) {
		cpu_vcpu_halt(vcpu, regs);
	}
	return rc;
}

virtual_addr_t cpu_vcpu_cp15_vector_addr(vmm_vcpu_t * vcpu, u32 irq_no)
{
	virtual_addr_t vaddr;
	irq_no = irq_no % CPU_IRQ_NR;

	if (vcpu->sregs.cp15.c1_sctlr & SCTLR_V_MASK) {
		vaddr = CPU_IRQ_HIGHVEC_BASE;
	} else {
		vaddr = CPU_IRQ_LOWVEC_BASE;
	}

	if (vcpu->sregs.cp15.ovect_base == vaddr) {
		/* FIXME: We assume that guest will use 
		 * LDR PC, [PC, #xx] as first instruction of irq handler */
		vaddr = vcpu->sregs.cp15.ovect[irq_no + 8];
	} else {
		vaddr += 4*irq_no;
	}

	return vaddr;
}

void cpu_vcpu_cp15_sync_cpsr(vmm_vcpu_t * vcpu)
{
	vmm_vcpu_t * cvcpu = vmm_scheduler_current_vcpu();
	vcpu->sregs.cp15.dacr &= 
			~(0x3 << (2 * TTBL_L1TBL_TTE_DOM_VCPU_SUPER));
	if ((vcpu->sregs.cpsr & CPSR_MODE_MASK) == CPSR_MODE_USER) {
		vcpu->sregs.cp15.dacr |= 
		(TTBL_DOM_NOACCESS << (2 * TTBL_L1TBL_TTE_DOM_VCPU_SUPER));
	} else {
		vcpu->sregs.cp15.dacr |= 
		(TTBL_DOM_CLIENT << (2 * TTBL_L1TBL_TTE_DOM_VCPU_SUPER));
	}
	if (cvcpu->num == vcpu->num) {
		cpu_mmu_chdacr(vcpu->sregs.cp15.dacr);
	}
}

void cpu_vcpu_cp15_context_switch(vmm_vcpu_t * tvcpu, 
				  vmm_vcpu_t * vcpu, 
				  vmm_user_regs_t * regs)
{
	cpu_mmu_chdacr(vcpu->sregs.cp15.dacr);
	cpu_mmu_chttbr(vcpu->sregs.cp15.l1);
}

static u32 cortexa9_cp15_c0_c1[8] =
{ 0x1031, 0x11, 0x000, 0, 0x00100103, 0x20000000, 0x01230000, 0x00002111 };

static u32 cortexa9_cp15_c0_c2[8] =
{ 0x00101111, 0x13112111, 0x21232041, 0x11112131, 0x00111142, 0, 0, 0 };

static u32 cortexa8_cp15_c0_c1[8] =
{ 0x1031, 0x11, 0x400, 0, 0x31100003, 0x20000000, 0x01202000, 0x11 };

static u32 cortexa8_cp15_c0_c2[8] =
{ 0x00101111, 0x12112111, 0x21232031, 0x11112131, 0x00111142, 0, 0, 0 };

int cpu_vcpu_cp15_reset(vmm_vcpu_t * vcpu)
{
	int rc = VMM_OK;
	u32 vtlb;
	cpu_page_t *p = NULL;

	/* Flush the shadow TLB */
	for (vtlb = 0; vtlb < vcpu->sregs.cp15.vtlb.count; vtlb++) {
		if (vcpu->sregs.cp15.vtlb.valid[vtlb]) {
			p = &vcpu->sregs.cp15.vtlb.page[vtlb];
			rc = cpu_mmu_unmap_page(vcpu->sregs.cp15.l1, p);
			if (rc) {
				return rc;
			}
			vcpu->sregs.cp15.vtlb.valid[vtlb] = 0;
		}
	}

	/* Reset values of important registers */
	switch (vcpu->sregs.cp15.c0_cpuid) {
	case ARM_CPUID_CORTEXA8:
		vmm_memcpy(vcpu->sregs.cp15.c0_c1, cortexa8_cp15_c0_c1, 
							8 * sizeof(u32));
		vmm_memcpy(vcpu->sregs.cp15.c0_c2, cortexa8_cp15_c0_c2, 
							8 * sizeof(u32));
		vcpu->sregs.cp15.c0_cachetype = 0x82048004;
		vcpu->sregs.cp15.c0_clid = (1 << 27) | (2 << 24) | 3;
		vcpu->sregs.cp15.c0_ccsid[0] = 0xe007e01a; /* 16k L1 dcache. */
		vcpu->sregs.cp15.c0_ccsid[1] = 0x2007e01a; /* 16k L1 icache. */
		vcpu->sregs.cp15.c0_ccsid[2] = 0xf0000000; /* No L2 icache. */
		vcpu->sregs.cp15.c1_sctlr = 0x00c50078;
		break;
	case ARM_CPUID_CORTEXA9:
		vmm_memcpy(vcpu->sregs.cp15.c0_c1, cortexa9_cp15_c0_c1, 
							8 * sizeof(u32));
		vmm_memcpy(vcpu->sregs.cp15.c0_c2, cortexa9_cp15_c0_c2, 
							8 * sizeof(u32));
		vcpu->sregs.cp15.c0_cachetype = 0x80038003;
		vcpu->sregs.cp15.c0_clid = (1 << 27) | (1 << 24) | 3;
		vcpu->sregs.cp15.c0_ccsid[0] = 0xe00fe015; /* 16k L1 dcache. */
		vcpu->sregs.cp15.c0_ccsid[1] = 0x200fe015; /* 16k L1 icache. */
		vcpu->sregs.cp15.c1_sctlr = 0x00c50078;
		break;
	default:
		break;
	};

	return rc;
}

int cpu_vcpu_cp15_init(vmm_vcpu_t * vcpu, u32 cpuid)
{
	u32 vtlb_count;
	vmm_devtree_node_t *node;
	const char *attrval;

	vmm_memset(&vcpu->sregs.cp15, 0, sizeof(vcpu->sregs.cp15));

	vcpu->sregs.cp15.l1 = cpu_mmu_l1tbl_alloc();
	vcpu->sregs.cp15.dacr = 0x0;
	vcpu->sregs.cp15.dacr |= (TTBL_DOM_CLIENT << 
					(TTBL_L1TBL_TTE_DOM_VCPU_NOMMU * 2));
	vcpu->sregs.cp15.dacr |= (TTBL_DOM_CLIENT << 
					(TTBL_L1TBL_TTE_DOM_VCPU_SUPER * 2));
	vcpu->sregs.cp15.dacr |= (TTBL_DOM_CLIENT << 
					(TTBL_L1TBL_TTE_DOM_VCPU_USER * 2));

	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPRATOR_STRING
				   VMM_DEVTREE_VMMINFO_NODE_NAME);
	if (!node) {
		return VMM_EFAIL;
	}
	attrval = vmm_devtree_attrval(node, MMU_TLBENT_PER_VCPU_ATTR_NAME);
	if (!attrval) {
		return VMM_EFAIL;
	}
	vtlb_count = *((u32 *)attrval);
	vcpu->sregs.cp15.vtlb.count = vtlb_count;
	vcpu->sregs.cp15.vtlb.valid = vmm_malloc(vtlb_count);
	vmm_memset(vcpu->sregs.cp15.vtlb.valid, 0, vtlb_count);
	vcpu->sregs.cp15.vtlb.page_asid = vmm_malloc(vtlb_count);
	vmm_memset(vcpu->sregs.cp15.vtlb.page_asid, 0, vtlb_count);
	vcpu->sregs.cp15.vtlb.page_dom = vmm_malloc(vtlb_count);
	vmm_memset(vcpu->sregs.cp15.vtlb.page_dom, 0, vtlb_count);
	vcpu->sregs.cp15.vtlb.page = vmm_malloc(vtlb_count * 
							sizeof(cpu_page_t));
	vmm_memset(vcpu->sregs.cp15.vtlb.page, 0, vtlb_count * 
							sizeof(cpu_page_t));
	vcpu->sregs.cp15.vtlb.victim = 0;

	if (read_sctlr() & SCTLR_V_MASK) {
		vcpu->sregs.cp15.ovect_base = CPU_IRQ_HIGHVEC_BASE;
	} else {
		vcpu->sregs.cp15.ovect_base = CPU_IRQ_LOWVEC_BASE;
	}

	vcpu->sregs.cp15.c0_cpuid = cpuid;

	return cpu_vcpu_cp15_reset(vcpu);
}

