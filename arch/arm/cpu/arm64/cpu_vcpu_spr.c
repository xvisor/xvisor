/**
 * Copyright (c) 2013 Sukanto Ghosh.
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
 * @file cpu_vcpu_spr.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief Source file for VCPU sysreg, cp15, and cp14 emulation
 */

#include <vmm_heap.h>
#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_devemu.h>
#include <vmm_scheduler.h>
#include <vmm_host_aspace.h>
#include <vmm_guest_aspace.h>
#include <vmm_vcpu_irq.h>
#include <libs/stringlib.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_helper.h>
#include <cpu_vcpu_emulate.h>
#include <cpu_vcpu_spr.h>
#include <mmu_lpae.h>
#include <emulate_arm.h>
#include <emulate_thumb.h>
#include <arm_features.h>

static int cpu_vcpu_stage2_map(struct vmm_vcpu *vcpu, 
			       arch_regs_t *regs,
			       physical_addr_t fipa)
{
	int rc, rc1;
	u32 reg_flags = 0x0;
	struct cpu_page pg;
	physical_addr_t inaddr, outaddr;
	physical_size_t size, availsz;

	memset(&pg, 0, sizeof(pg));

	inaddr = fipa & TTBL_L3_MAP_MASK;
	size = TTBL_L3_BLOCK_SIZE;
	pg.sh = 3U;

	rc = vmm_guest_physical_map(vcpu->guest, inaddr, size, 
				    &outaddr, &availsz, &reg_flags);
	if (rc) {
		vmm_printf("%s: IPA=0x%lx size=0x%lx map failed\n", 
			   __func__, inaddr, size);
		return rc;
	}

	if (availsz < TTBL_L3_BLOCK_SIZE) {
		vmm_printf("%s: availsz=0x%lx insufficent for IPA=0x%lx\n",
			   __func__, availsz, inaddr);
		return VMM_EFAIL;
	}

	pg.ia = inaddr;
	pg.sz = size;
	pg.oa = outaddr;

	if (reg_flags & (VMM_REGION_ISRAM | VMM_REGION_ISROM)) {
		inaddr = fipa & TTBL_L2_MAP_MASK;
		size = TTBL_L2_BLOCK_SIZE;
		rc = vmm_guest_physical_map(vcpu->guest, inaddr, size, 
				    &outaddr, &availsz, &reg_flags);
		if (!rc && (availsz >= TTBL_L2_BLOCK_SIZE)) {
			pg.ia = inaddr;
			pg.sz = size;
			pg.oa = outaddr;
		}
	}

	if (reg_flags & VMM_REGION_VIRTUAL) {
		pg.af = 0;
		pg.ap = TTBL_HAP_NOACCESS;
	} else if (reg_flags & VMM_REGION_READONLY) {
		pg.af = 1;
		pg.ap = TTBL_HAP_READONLY;
	} else {
		pg.af = 1;
		pg.ap = TTBL_HAP_READWRITE;
	}

	/* memattr in stage 2
	 * ------------------
	 *  0x0 - strongly ordered
	 *  0x5 - normal-memory NC 
	 *  0xA - normal-memory WT
	 *  0xF - normal-memory WB
	 */
	if (reg_flags & VMM_REGION_CACHEABLE) {
		if (reg_flags & VMM_REGION_BUFFERABLE) {
			pg.memattr = 0xF;
		} else {
			pg.memattr = 0xA;
		}
	} else {
		pg.memattr = 0x0;
	}

	/* Try to map the page in Stage2 */
	rc = mmu_lpae_map_page(arm_guest_priv(vcpu->guest)->ttbl, &pg);
	if (rc) {
		/* On SMP Guest, two different VCPUs may try to map same
		 * Guest region in Stage2 at the same time. This may cause
		 * mmu_lpae_map_page() to fail for one of the Guest VCPUs.
		 *
		 * To take care of this situation, we recheck Stage2 mapping
		 * when mmu_lpae_map_page() fails.
		 */
		memset(&pg, 0, sizeof(pg));
		rc1 = mmu_lpae_get_page(arm_guest_priv(vcpu->guest)->ttbl, 
					fipa, &pg);
		if (rc1) {
			return rc1;
		}
		rc = VMM_OK;
	}

	return rc;
}

int cpu_vcpu_inst_abort(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs,
			u32 il, u32 iss, 
			physical_addr_t fipa)
{
	switch (iss & ISS_ABORT_FSC_MASK) {
	case FSC_TRANS_FAULT_LEVEL1:
	case FSC_TRANS_FAULT_LEVEL2:
	case FSC_TRANS_FAULT_LEVEL3:
		return cpu_vcpu_stage2_map(vcpu, regs, fipa);
	default:
		break;
	};

	return VMM_EFAIL;
}

int cpu_vcpu_data_abort(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs,
			u32 il, u32 iss, 
			physical_addr_t fipa)
{
	u32 read_count, inst;
	physical_addr_t inst_pa;

	switch (iss & ISS_ABORT_FSC_MASK) {
	case FSC_TRANS_FAULT_LEVEL1:
	case FSC_TRANS_FAULT_LEVEL2:
	case FSC_TRANS_FAULT_LEVEL3:
		return cpu_vcpu_stage2_map(vcpu, regs, fipa);
	case FSC_ACCESS_FAULT_LEVEL1:
	case FSC_ACCESS_FAULT_LEVEL2:
	case FSC_ACCESS_FAULT_LEVEL3:
		if (!(iss & ISS_ABORT_ISV_MASK)) {
			/* Determine instruction physical address */
			va2pa_at(VA2PA_STAGE1, VA2PA_EL1, VA2PA_RD, regs->pc);
			inst_pa = mrs(par_el1);
			inst_pa &= PAR_PA_MASK;
			inst_pa |= (regs->pc & 0x00000FFF);

			/* Read the faulting instruction */
			read_count = 
			vmm_host_memory_read(inst_pa, &inst, sizeof(inst));
			if (read_count != sizeof(inst)) {
				return VMM_EFAIL;
			}
			if (regs->pstate & PSR_THUMB_ENABLED) {
				return emulate_thumb_inst(vcpu, regs, inst);
			} else {
				return emulate_arm_inst(vcpu, regs, inst);
			}
		}
		if (iss & ISS_ABORT_WNR_MASK) {
			return cpu_vcpu_emulate_store(vcpu, regs, 
						      il, iss, fipa);
		} else {
			return cpu_vcpu_emulate_load(vcpu, regs, 
						     il, iss, fipa);
		}
	default:
		vmm_printf("%s: Unhandled FSC=0x%x\n", 
			   __func__, iss & ISS_ABORT_FSC_MASK);
		break;
	};

	return VMM_EFAIL;
}

bool cpu_vcpu_spr_read(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs,
			u32 iss_sysreg, u64 *data)
{
	*data = 0;
	switch (iss_sysreg) {
	case ISS_ACTLR_EL1:
		*data = arm_priv(vcpu)->actlr;
		break;
	default:
		vmm_printf("Guest MSR/MRS Emulation @ PC:0x%X\n", regs->pc);
		goto bad_reg;
	}
	return TRUE;
bad_reg:
	vmm_printf("Unimplemented [mrs <Xt>, %d]\n", iss_sysreg);
	return FALSE;
}

bool cpu_vcpu_spr_write(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs,
			u32 iss_sysreg, u64 data)
{
	switch (iss_sysreg) {
	case ISS_ACTLR_EL1:
		arm_priv(vcpu)->actlr = data;
		break;
	default:
		vmm_printf("Guest MSR/MRS Emulation @ PC:0x%X\n", regs->pc);
		goto bad_reg;
	}
	return TRUE;
bad_reg:
	vmm_printf("Unimplemented [msr %d, <Xt>]\n", iss_sysreg);
	return FALSE;
}

bool cpu_vcpu_cp15_read(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs,
			u32 opc1, u32 opc2, u32 CRn, u32 CRm, 
			u64 *data)
{
	*data = 0x0;
	switch (CRn) {
	case 1: /* System configuration.  */
		switch (opc2) {
		case 1: /* Auxiliary control register.  */
			if (!arm_feature(vcpu, ARM_FEATURE_AUXCR))
				goto bad_reg;
			switch (arm_cpuid(vcpu)) {
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
			case ARM_CPUID_CORTEXA15:
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
		default:
			goto bad_reg;
		};
		break;
	case 15:
		switch (opc1) {
		case 4:	/* CBAR: Configuration Base Address Register */
			switch (arm_cpuid(vcpu)) {
			case ARM_CPUID_CORTEXA9:
				*data = 0x1e000000;
				break;
			case ARM_CPUID_CORTEXA15:
				*data = 0x2c000000;
				break;
			default:
				goto bad_reg;
			};
			break;
		default:
			goto bad_reg;
		};
		break;
	}
	return TRUE;
bad_reg:
	vmm_printf("Unimplemented [mrc p15, %d, <Rt>, c%d, c%d, %d]\n", 
		   opc1, CRn, CRm, opc2);
	return FALSE;
}

bool cpu_vcpu_cp15_write(struct vmm_vcpu *vcpu, 
			 arch_regs_t *regs,
			 u32 opc1, u32 opc2, u32 CRn, u32 CRm, 
			 u64 data)
{
	switch (CRn) {
	case 1: /* System configuration.  */
		switch (opc2) {
		case 1: /* Auxiliary control register.  */
			/* Not implemented.  */
			break;
		default:
			goto bad_reg;
		};
		break;
	}
	return TRUE;
bad_reg:
	vmm_printf("Unimplemented [mcr p15, %d, <Rt>, c%d, c%d, %d]\n", 
		   opc1, CRn, CRm, opc2);
	return FALSE;
}

bool cpu_vcpu_cp14_read(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs,
			u32 opc1, u32 opc2, u32 CRn, u32 CRm, 
			u64 *data)
{
	*data = 0x0;
	switch (opc1) {
	case 6: /* ThumbEE registers */
		switch (CRn) {
			case 0:	/* TEECR */
				*data = arm_priv(vcpu)->teecr;
				break;
			case 1:	/* TEEHBR */
				*data = arm_priv(vcpu)->teehbr;
				break;
			default:
				goto bad_reg;
		};
		break;
	case 0:	/* Debug registers */
	case 1: /* Trace registers */
	case 7: /* Jazelle registers */
	default:
		goto bad_reg;
	};
	return TRUE;
bad_reg:
	vmm_printf("Unimplemented [mrc p14, %d, <Rt>, c%d, c%d, %d]\n", 
		   opc1, CRn, CRm, opc2);
	return FALSE;
}

bool cpu_vcpu_cp14_write(struct vmm_vcpu *vcpu, 
			 arch_regs_t *regs,
			 u32 opc1, u32 opc2, u32 CRn, u32 CRm, 
			 u64 data)
{
	switch (opc1) {
	case 6: /* ThumbEE registers */
		switch (CRn) {
			case 0:	/* TEECR */
				arm_priv(vcpu)->teecr = data;
				break;
			case 1:	/* TEEHBR */
				arm_priv(vcpu)->teehbr = data;
				break;
			default:
				goto bad_reg;
		};
		break;
	case 0:	/* Debug registers */
	case 1: /* Trace registers */
	case 7: /* Jazelle registers */
	default:
		goto bad_reg;
	};
	return TRUE;
bad_reg:
	vmm_printf("Unimplemented [mcr p14, %d, <Rt>, c%d, c%d, %d]\n", 
		   opc1, CRn, CRm, opc2);
	return FALSE;
}

