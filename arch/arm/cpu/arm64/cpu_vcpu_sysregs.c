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
 * @file cpu_vcpu_sysregs.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief Source file for VCPU sysreg, cp15, and cp14 emulation
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <libs/stringlib.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_sysregs.h>
#include <mmu_lpae.h>
#include <emulate_arm.h>
#include <emulate_thumb.h>
#include <arm_features.h>

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

bool cpu_vcpu_sysregs_read(struct vmm_vcpu *vcpu, 
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

bool cpu_vcpu_sysregs_write(struct vmm_vcpu *vcpu, 
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

void cpu_vcpu_sysregs_save(struct vmm_vcpu *vcpu)
{
	/* Save EL1 registers */
	arm_priv(vcpu)->sp_el0 = mrs(sp_el0);
	arm_priv(vcpu)->sp_el1 = mrs(sp_el1);
	arm_priv(vcpu)->elr_el1 = mrs(elr_el1);
	arm_priv(vcpu)->spsr_el1 = mrs(spsr_el1);
	arm_priv(vcpu)->spsr_abt = mrs(spsr_abt);
	arm_priv(vcpu)->spsr_und = mrs(spsr_und);
	arm_priv(vcpu)->spsr_irq = mrs(spsr_irq);
	arm_priv(vcpu)->spsr_fiq = mrs(spsr_fiq);
	arm_priv(vcpu)->spsr_irq = mrs(spsr_irq);
	arm_priv(vcpu)->ttbr0 = mrs(ttbr0_el1);
	arm_priv(vcpu)->ttbr1 = mrs(ttbr1_el1);
	arm_priv(vcpu)->sctlr = mrs(sctlr_el1);
	arm_priv(vcpu)->cpacr = mrs(cpacr_el1);
	arm_priv(vcpu)->tcr = mrs(tcr_el1);
	arm_priv(vcpu)->esr = mrs(esr_el1);
	arm_priv(vcpu)->far = mrs(far_el1);
	arm_priv(vcpu)->mair = mrs(mair_el1);
	arm_priv(vcpu)->vbar = mrs(vbar_el1);
	arm_priv(vcpu)->contextidr = mrs(contextidr_el1);
	arm_priv(vcpu)->tpidr_el0 = mrs(tpidr_el0);
	arm_priv(vcpu)->tpidr_el1 = mrs(tpidr_el1);
	arm_priv(vcpu)->tpidrro = mrs(tpidrro_el0);
	if (cpu_supports_thumbee()) {
		arm_priv(vcpu)->teecr = mrs(teecr32_el1);
		arm_priv(vcpu)->teehbr = mrs(teehbr32_el1);
	}
	arm_priv(vcpu)->dacr = mrs(dacr32_el2);
	arm_priv(vcpu)->ifsr = mrs(ifsr32_el2);
}

void cpu_vcpu_sysregs_restore(struct vmm_vcpu *vcpu)
{
	/* Update Stage2 MMU context */
	mmu_lpae_stage2_chttbl(vcpu->guest->id, 
			       arm_guest_priv(vcpu->guest)->ttbl);
	/* Update VPIDR and VMPIDR */
	msr(vpidr_el2, arm_priv(vcpu)->midr);
	msr(vmpidr_el2, arm_priv(vcpu)->mpidr);
	/* Restore EL1 register */
	msr(sp_el0, arm_priv(vcpu)->sp_el0);
	msr(sp_el1, arm_priv(vcpu)->sp_el1);
	msr(elr_el1, arm_priv(vcpu)->elr_el1);
	msr(spsr_el1, arm_priv(vcpu)->spsr_el1);
	msr(spsr_abt, arm_priv(vcpu)->spsr_abt);
	msr(spsr_und, arm_priv(vcpu)->spsr_und);
	msr(spsr_irq, arm_priv(vcpu)->spsr_irq);
	msr(spsr_fiq, arm_priv(vcpu)->spsr_fiq);
	msr(spsr_irq, arm_priv(vcpu)->spsr_irq);
	msr(ttbr0_el1, arm_priv(vcpu)->ttbr0);
	msr(ttbr1_el1, arm_priv(vcpu)->ttbr1);
	msr(sctlr_el1, arm_priv(vcpu)->sctlr);
	msr(cpacr_el1, arm_priv(vcpu)->cpacr);
	msr(tcr_el1, arm_priv(vcpu)->tcr);
	msr(esr_el1, arm_priv(vcpu)->esr);
	msr(far_el1, arm_priv(vcpu)->far);
	msr(mair_el1, arm_priv(vcpu)->mair);
	msr(vbar_el1, arm_priv(vcpu)->vbar);
	msr(contextidr_el1, arm_priv(vcpu)->contextidr);
	msr(tpidr_el0, arm_priv(vcpu)->tpidr_el0);
	msr(tpidr_el1, arm_priv(vcpu)->tpidr_el1);
	msr(tpidrro_el0, arm_priv(vcpu)->tpidrro);
	if (cpu_supports_thumbee()) {
		msr(teecr32_el1, arm_priv(vcpu)->teecr);
		msr(teehbr32_el1, arm_priv(vcpu)->teehbr);
	}
	msr(dacr32_el2, arm_priv(vcpu)->dacr);
	msr(ifsr32_el2, arm_priv(vcpu)->ifsr);
}

int cpu_vcpu_sysregs_init(struct vmm_vcpu *vcpu, u32 cpuid)
{
	/* Initialize VCPU MIDR and MPIDR registers */
	switch (cpuid) {
	case ARM_CPUID_CORTEXA9:
		/* Guest ARM32 Linux running on Cortex-A9
		 * tries to use few ARMv7 instructions which 
		 * are removed in AArch32 instruction set.
		 * 
		 * To take care of this situation, we fake 
		 * PartNum and Revison visible to Cortex-A9
		 * Guest VCPUs.
		 */
		arm_priv(vcpu)->midr = cpuid;
		arm_priv(vcpu)->midr &= 
			~(MIDR_PARTNUM_MASK|MIDR_REVISON_MASK);
		arm_priv(vcpu)->mpidr = (1 << 31) | vcpu->subid;
		break;
	case ARM_CPUID_CORTEXA15:
		arm_priv(vcpu)->midr = cpuid;
		arm_priv(vcpu)->mpidr = (1 << 31) | vcpu->subid;
		break;
	default:
		arm_priv(vcpu)->midr = cpuid;
		arm_priv(vcpu)->mpidr = vcpu->subid;
		break;
	};

	/* Reset special registers which are required 
	 * to have known values upon VCPU reset.
	 *
	 * No need to init the other SPRs as their 
	 * state is unknown as per AArch64 spec
	 */ 
	arm_priv(vcpu)->sctlr = 0x0;
	arm_priv(vcpu)->sp_el0 = 0x0;
	arm_priv(vcpu)->sp_el1 = 0x0;
	arm_priv(vcpu)->elr_el1 = 0x0;
	arm_priv(vcpu)->spsr_el1 = 0x0;
	arm_priv(vcpu)->spsr_abt = 0x0;
	arm_priv(vcpu)->spsr_und = 0x0;
	arm_priv(vcpu)->spsr_irq = 0x0;
	arm_priv(vcpu)->spsr_fiq = 0x0;
	arm_priv(vcpu)->tcr = 0x0;

	return VMM_OK;
}

int cpu_vcpu_sysregs_deinit(struct vmm_vcpu *vcpu)
{
	/* For now nothing to do here. */
	return VMM_OK;
}

