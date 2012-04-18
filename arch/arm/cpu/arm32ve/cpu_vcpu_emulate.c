/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file cpu_vcpu_emulate.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of hardware assisted instruction emulation
 */

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_vcpu_irq.h>
#include <vmm_devemu.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_helper.h>
#include <cpu_vcpu_cp15.h>
#include <cpu_vcpu_emulate.h>

int cpu_vcpu_emulate_wfi(struct vmm_vcpu * vcpu, 
			 arch_regs_t * regs,
			 u32 il, u32 iss)
{
	/* Wait for irq on this vcpu */
	vmm_vcpu_irq_wait(vcpu);

	/* Next instruction */
	regs->pc += (il) ? 4 : 2;

	return VMM_OK;
}

int cpu_vcpu_emulate_mcr_mrc_cp15(struct vmm_vcpu * vcpu, 
				  arch_regs_t * regs, 
				  u32 il, u32 iss)
{
	int rc = VMM_OK;
	u32 opc2, opc1, CRn, Rt, CRm, t;

	/* Retrive MCR/MRC parameters */
	opc2 = (iss & 0x000E0000) >> 17;
	opc1 = (iss & 0x0001C000) >> 14;
	CRn  = (iss & 0x00003C00) >> 10;
	Rt   = (iss & 0x000001E0) >> 5;
	CRm  = (iss & 0x0000001E) >> 1;

	if (iss & 0x1) {
		/* MCR CP15 */
		t = cpu_vcpu_reg_read(vcpu, regs, Rt);
		rc = cpu_vcpu_cp15_write(vcpu, regs, opc1, opc2, CRn, CRm, t);
	} else {
		/* MRC CP15 */
		rc = cpu_vcpu_cp15_read(vcpu, regs, opc1, opc2, CRn, CRm, &t);
		if (!rc) {
			cpu_vcpu_reg_write(vcpu, regs, Rt, t);
		}
	}

	if (!rc) {
		/* Next instruction */
		regs->pc += (il) ? 4 : 2;
	}

	return rc;
}

/* TODO: To be implemeted later */
int cpu_vcpu_emulate_mcrr_mrrc_cp15(struct vmm_vcpu * vcpu, 
				    arch_regs_t * regs, 
				    u32 il, u32 iss)
{
	return VMM_EFAIL;
}


/* TODO: To be implemeted later */
int cpu_vcpu_emulate_mcr_mrc_cp14(struct vmm_vcpu * vcpu, 
				  arch_regs_t * regs, 
				  u32 il, u32 iss)
{
	return VMM_EFAIL;
}

/* TODO: To be implemeted later */
int cpu_vcpu_emulate_ldc_stc_cp14(struct vmm_vcpu * vcpu, 
				  arch_regs_t * regs, 
				  u32 il, u32 iss)
{
	return VMM_EFAIL;
}

/* TODO: To be implemeted later */
int cpu_vcpu_emulate_cp0_cp13(struct vmm_vcpu * vcpu, 
			      arch_regs_t * regs, 
			      u32 il, u32 iss)
{
	return VMM_EFAIL;
}

/* TODO: To be implemeted later */
int cpu_vcpu_emulate_vmrs(struct vmm_vcpu * vcpu, 
			  arch_regs_t * regs, 
			  u32 il, u32 iss)
{
	return VMM_EFAIL;
}

/* TODO: To be implemeted later */
int cpu_vcpu_emulate_jazelle(struct vmm_vcpu * vcpu, 
			     arch_regs_t * regs, 
			     u32 il, u32 iss)
{
	return VMM_EFAIL;
}

/* TODO: To be implemeted later */
int cpu_vcpu_emulate_bxj(struct vmm_vcpu * vcpu, 
			 arch_regs_t * regs, 
			 u32 il, u32 iss)
{
	return VMM_EFAIL;
}

/* TODO: To be implemeted later */
int cpu_vcpu_emulate_mrrc_cp14(struct vmm_vcpu * vcpu, 
			       arch_regs_t * regs, 
			       u32 il, u32 iss)
{
	return VMM_EFAIL;
}

static int cpu_vcpu_hvc_movs_pc_lr(struct vmm_vcpu * vcpu, 
				   arch_regs_t * regs, 
				   u32 il)
{
	u32 hcr, spsr;

	/* Clear VI, VF from HCR */
	hcr = read_hcr();
	hcr &= ~(HCR_VI_MASK | HCR_VF_MASK);
	arm_priv(vcpu)->hcr = hcr;
	write_hcr(hcr);

	/* Update PC */
	regs->pc = cpu_vcpu_reg_read(vcpu, regs, 14);

	/* Update CPSR */
	spsr = cpu_vcpu_spsr_retrieve(vcpu, regs->cpsr & CPSR_MODE_MASK);
	regs->cpsr = spsr;

	return VMM_OK;
}

int cpu_vcpu_emulate_hvc(struct vmm_vcpu * vcpu, 
			 arch_regs_t * regs, 
			 u32 il, u32 iss)
{
	switch(iss & 0xFFFF) {
	case 0:
		return cpu_vcpu_hvc_movs_pc_lr(vcpu, regs, il);
	default:
		break;
	};

	return VMM_EFAIL;
}

int cpu_vcpu_emulate_load(struct vmm_vcpu * vcpu, 
			  arch_regs_t * regs,
			  u32 il, u32 iss,
			  physical_addr_t ipa)
{
	int rc;
	u8 data8;
	u16 data16;
	u32 data32, srt, sas;

	srt = (iss & ISS_ABORT_SRT_MASK) >> ISS_ABORT_SRT_SHIFT;
	sas = (iss & ISS_ABORT_SAS_MASK) >> ISS_ABORT_SAS_SHIFT;

	switch (sas) {
	case 0:
		rc = vmm_devemu_emulate_read(vcpu, ipa, 
					     &data8, sizeof(data8));
		if (!rc) {
			cpu_vcpu_reg_write(vcpu, regs, srt, data8);
		}
		break;
	case 1:
		rc = vmm_devemu_emulate_read(vcpu, ipa, 
					     &data16, sizeof(data16));
		if (!rc) {
			cpu_vcpu_reg_write(vcpu, regs, srt, data16);
		}
		break;
	case 2:
		rc = vmm_devemu_emulate_read(vcpu, ipa, 
					     &data32, sizeof(data32));
		if (!rc) {
			cpu_vcpu_reg_write(vcpu, regs, srt, data32);
		}
		break;
	default:
		rc = VMM_EFAIL;
		break;
	};

	if (!rc) {
		/* Next instruction */
		regs->pc += (il) ? 4 : 2;
	}

	return rc;
}

int cpu_vcpu_emulate_store(struct vmm_vcpu * vcpu, 
			   arch_regs_t * regs,
			   u32 il, u32 iss,
			   physical_addr_t ipa)
{
	int rc;
	u8 data8;
	u16 data16;
	u32 data32, srt, sas;

	srt = (iss & ISS_ABORT_SRT_MASK) >> ISS_ABORT_SRT_SHIFT;
	sas = (iss & ISS_ABORT_SAS_MASK) >> ISS_ABORT_SAS_SHIFT;

	switch (sas) {
	case 0:
		data8 = cpu_vcpu_reg_read(vcpu, regs, srt);
		rc = vmm_devemu_emulate_write(vcpu, ipa, 
					      &data8, sizeof(data8));
		break;
	case 1:
		data16 = cpu_vcpu_reg_read(vcpu, regs, srt);
		rc = vmm_devemu_emulate_write(vcpu, ipa, 
					      &data16, sizeof(data16));
		break;
	case 2:
		data32 = cpu_vcpu_reg_read(vcpu, regs, srt);
		rc = vmm_devemu_emulate_write(vcpu, ipa, 
					      &data32, sizeof(data32));
		break;
	default:
		rc = VMM_EFAIL;
		break;
	};

	if (!rc) {
		/* Next instruction */
		regs->pc += (il) ? 4 : 2;
	}

	return rc;
}


