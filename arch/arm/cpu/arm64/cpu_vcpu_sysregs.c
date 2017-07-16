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
#include <vmm_smp.h>
#include <vmm_cache.h>
#include <vmm_stdio.h>
#include <libs/stringlib.h>
#include <arch_gicv3.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_switch.h>
#include <cpu_vcpu_sysregs.h>

#include <arm_features.h>

bool cpu_vcpu_cp15_read(struct vmm_vcpu *vcpu,
			arch_regs_t *regs,
			u32 opc1, u32 opc2, u32 CRn, u32 CRm,
			u32 *data)
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
			case ARM_CPUID_CORTEXA7:
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
			case ARM_CPUID_CORTEXA7:
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
	default:
		goto bad_reg;
	};

	return TRUE;

bad_reg:
	vmm_printf("Unimplemented [mrc p15, %d, <Rt>, c%d, c%d, %d]\n", 
		   opc1, CRn, CRm, opc2);
	return FALSE;
}

bool cpu_vcpu_cp15_write(struct vmm_vcpu *vcpu,
			 arch_regs_t *regs,
			 u32 opc1, u32 opc2, u32 CRn, u32 CRm,
			 u32 data)
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
	case 7: /* Cache control. */
		switch (CRm) {
		case 6:	/* Upgrade DCISW to DCCISW, as per HCR.SWIO */
		case 14: /* DCCISW */
			switch (opc2) {
			case 2:
				vmm_cpumask_setall(
					&arm_priv(vcpu)->dflush_needed);
				vmm_cpumask_clear_cpu(vmm_smp_processor_id(),
					&arm_priv(vcpu)->dflush_needed);
				asm volatile("dc cisw, %0" : : "r" (data));
				break;
			default:
				goto bad_reg;
			};
			break;
		case 10: /* DCCSW */
			switch (opc2) {
			case 2:
				vmm_cpumask_setall(
					&arm_priv(vcpu)->dflush_needed);
				vmm_cpumask_clear_cpu(vmm_smp_processor_id(),
					&arm_priv(vcpu)->dflush_needed);
				asm volatile("dc csw, %0" : : "r" (data));
				break;
			default:
				goto bad_reg;
			};
			break;
		default:
			goto bad_reg;
		};
		break;
	default:
		goto bad_reg;
	};

	return TRUE;

bad_reg:
	vmm_printf("Unimplemented [mcr p15, %d, <Rt>, c%d, c%d, %d]\n", 
		   opc1, CRn, CRm, opc2);
	return FALSE;
}

bool cpu_vcpu_cp14_read(struct vmm_vcpu *vcpu,
			arch_regs_t *regs,
			u32 opc1, u32 opc2, u32 CRn, u32 CRm,
			u32 *data)
{
	struct arm_priv_sysregs *s = &arm_priv(vcpu)->sysregs;

	*data = 0x0;
	switch (opc1) {
	case 6: /* ThumbEE registers */
		if (!arm_feature(vcpu, ARM_FEATURE_THUMB2EE))
			goto bad_reg;
		switch (CRn) {
		case 0:	/* TEECR */
			s->teecr32_el1 = mrs(teecr32_el1);
			*data = s->teecr32_el1;
			break;
		case 1:	/* TEEHBR */
			s->teehbr32_el1 = mrs(teehbr32_el1);
			*data = s->teehbr32_el1;
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
			 u32 data)
{
	struct arm_priv_sysregs *s = &arm_priv(vcpu)->sysregs;

	switch (opc1) {
	case 6: /* ThumbEE registers */
		if (!arm_feature(vcpu, ARM_FEATURE_THUMB2EE))
			goto bad_reg;
		switch (CRn) {
		case 0:	/* TEECR */
			msr(teecr32_el1, data);
			s->teecr32_el1 = data;
			break;
		case 1:	/* TEEHBR */
			msr(teehbr32_el1, data);
			s->teehbr32_el1 = data;
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
	struct arm_priv_sysregs *s = &arm_priv(vcpu)->sysregs;

	*data = 0;
	switch (iss_sysreg) {
	case ISS_ACTLR_EL1:
		*data = s->actlr_el1;
		break;
	case ISS_SRE_EL1:
		/*
		 * GICv3 sysregs are not supposed to be emulated.
		 *
		 * Despite this, some Guest OSes (such as Linux) may
		 * try to force enable GICv3 sysregs via ICC_SRE_EL1.SRE
		 * bit whenever they see GICv3 capability in processor
		 * feature registers. This can be problematic for Guest
		 * with GICv2 running on host with GICv3.
		 *
		 * To handle such Guest OSes, we emulate ICC_SRE_EL1
		 * as RAZ/WI.
		 */
		*data = 0x0;
		break;
	default:
		vmm_printf("Guest MSR/MRS Emulation @ PC:0x%"PRIx64"\n",
			   regs->pc);
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
	struct arm_priv_sysregs *s = &arm_priv(vcpu)->sysregs;

	switch (iss_sysreg) {
	case ISS_ACTLR_EL1:
		s->actlr_el1 = data;
		break;
	case ISS_DCISW_EL1:	/* Upgrade DCISW to DCCISW, as per HCR.SWIO */
	case ISS_DCCISW_EL1: /* DCCISW */
		vmm_cpumask_setall(&arm_priv(vcpu)->dflush_needed);
		vmm_cpumask_clear_cpu(vmm_smp_processor_id(),
				&arm_priv(vcpu)->dflush_needed);
		asm volatile("dc cisw, %0" : : "r" (data));
		break;
	case ISS_DCCSW_EL1: /* DCCSW */
		vmm_cpumask_setall(&arm_priv(vcpu)->dflush_needed);
		vmm_cpumask_clear_cpu(vmm_smp_processor_id(),
				&arm_priv(vcpu)->dflush_needed);
		asm volatile("dc csw, %0" : : "r" (data));
		break;
	case ISS_SRE_EL1:
		/*
		 * GICv3 sysregs are not supposed to be emulated.
		 *
		 * Despite this, some Guest OSes (such as Linux) may
		 * try to force enable GICv3 sysregs via ICC_SRE_EL1.SRE
		 * bit whenever they see GICv3 capability in processor
		 * feature registers. This can be problematic for Guest
		 * with GICv2 running on host with GICv3.
		 *
		 * To handle such Guest OSes, we emulate ICC_SRE_EL1
		 * as RAZ/WI.
		 */
		break;
	default:
		vmm_printf("Guest MSR/MRS Emulation @ PC:0x%"PRIx64"\n",
			   regs->pc);
		goto bad_reg;
	}

	return TRUE;

bad_reg:
	vmm_printf("Unimplemented [msr %d, <Xt>]\n", iss_sysreg);
	return FALSE;
}

void cpu_vcpu_sysregs_save(struct vmm_vcpu *vcpu)
{
	cpu_vcpu_sysregs_regs_save(&arm_priv(vcpu)->sysregs);
}

void cpu_vcpu_sysregs_restore(struct vmm_vcpu *vcpu)
{
	cpu_vcpu_sysregs_regs_restore(&arm_priv(vcpu)->sysregs);

	/* Check whether vcpu requires dcache to be flushed on
	 * this host CPU. This is a consequence of doing dcache
	 * operations by set/way.
	 */
	if (vmm_cpumask_test_and_clear_cpu(vmm_smp_processor_id(),
					   &arm_priv(vcpu)->dflush_needed)) {
		vmm_flush_cache_all();
	}
}

void cpu_vcpu_sysregs_dump(struct vmm_chardev *cdev,
			   struct vmm_vcpu *vcpu)
{
	struct arm_priv_sysregs *s = &arm_priv(vcpu)->sysregs;

	vmm_cprintf(cdev, "System 64bit EL1/EL0 Registers\n");
	vmm_cprintf(cdev, " %11s=0x%016"PRIx64" %11s=0x%016"PRIx64"\n",
		    "SP_EL0", s->sp_el0,
		    "SP_EL1", s->sp_el1);
	vmm_cprintf(cdev, " %11s=0x%016"PRIx64" %11s=0x%016"PRIx64"\n",
		    "ELR_EL1", s->elr_el1,
		    "SPSR_EL1", s->spsr_el1);
	vmm_cprintf(cdev, " %11s=0x%016"PRIx64" %11s=0x%016"PRIx64"\n",
		    "MIDR_EL1", s->midr_el1,
		    "MPIDR_EL1", s->mpidr_el1);
	vmm_cprintf(cdev, " %11s=0x%016"PRIx64" %11s=0x%016"PRIx64"\n",
		    "SCTLR_EL1", s->sctlr_el1,
		    "CPACR_EL1", s->cpacr_el1);
	vmm_cprintf(cdev, " %11s=0x%016"PRIx64" %11s=0x%016"PRIx64"\n",
		    "TTBR0_EL1", s->ttbr0_el1,
		    "TTBR1_EL1", s->ttbr1_el1);
	vmm_cprintf(cdev, " %11s=0x%016"PRIx64" %11s=0x%016"PRIx64"\n",
		    "TCR_EL1", s->tcr_el1,
		    "ESR_EL1", s->esr_el1);
	vmm_cprintf(cdev, " %11s=0x%016"PRIx64" %11s=0x%016"PRIx64"\n",
		    "FAR_EL1", s->far_el1,
		    "PAR_EL1", s->par_el1);
	vmm_cprintf(cdev, " %11s=0x%016"PRIx64" %11s=0x%016"PRIx64"\n",
		    "MAIR_EL1", s->mair_el1,
		    "VBAR_EL1", s->vbar_el1);
	vmm_cprintf(cdev, " %11s=0x%016"PRIx64" %11s=0x%016"PRIx64"\n",
		    "CONTXID_EL1", s->contextidr_el1,
		    "TPIDR_EL0", s->tpidr_el0);
	vmm_cprintf(cdev, " %11s=0x%016"PRIx64" %11s=0x%016"PRIx64"\n",
		    "TPIDRRO_EL0", s->tpidrro_el0,
		    "TPIDR_EL1", s->tpidr_el1);
	vmm_cprintf(cdev, "System 32bit Only Registers\n");
	vmm_cprintf(cdev, " %11s=0x%08"PRIx32"         %11s=0x%08"PRIx32"\n",
		    "SPSR_ABT", s->spsr_abt,
		    "SPSR_UND", s->spsr_und);
	vmm_cprintf(cdev, " %11s=0x%08"PRIx32"         %11s=0x%08"PRIx32"\n",
		    "SPSR_IRQ", s->spsr_irq,
		    "SPSR_FIQ", s->spsr_fiq);
	vmm_cprintf(cdev, " %11s=0x%08"PRIx32"         %11s=0x%08"PRIx32"\n",
		    "DACR32_EL2", s->dacr32_el2,
		    "IFSR32_EL2", s->ifsr32_el2);
	if (!arm_feature(vcpu, ARM_FEATURE_THUMB2EE)) {
		return;
	}
	vmm_cprintf(cdev, " %11s=0x%08"PRIx32"         %11s=0x%08"PRIx32"\n",
		    "TEECR32_EL1", s->teecr32_el1,
		    "TEEHBR32_EL1", s->teehbr32_el1);
}

int cpu_vcpu_sysregs_init(struct vmm_vcpu *vcpu, u32 cpuid)
{
	struct arm_priv_sysregs *s = &arm_priv(vcpu)->sysregs;

	/* Clear all sysregs */
	memset(s, 0, sizeof(struct arm_priv_sysregs));

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
		s->midr_el1 = cpuid;
		s->midr_el1 &= ~(MIDR_PARTNUM_MASK|MIDR_REVISON_MASK);
		s->mpidr_el1 = (1 << 31) | vcpu->subid;
		break;
	case ARM_CPUID_CORTEXA7:
	case ARM_CPUID_CORTEXA15:
		s->midr_el1 = cpuid;
		s->mpidr_el1 = (1 << 31) | vcpu->subid;
		break;
	case ARM_CPUID_ARMV7:
	case ARM_CPUID_ARMV8:
		s->midr_el1 = mrs(midr_el1);
		s->mpidr_el1 = vcpu->subid;
		break;
	default:
		s->midr_el1 = cpuid;
		s->mpidr_el1 = vcpu->subid;
		break;
	};

	/* If host HW does not have ThumbEE then clear ThumbEE feature
	 * flag so that VCPU undefined exception when accessing these
	 * registers.
	 */
	if (!cpu_supports_thumbee()) {
		arm_clear_feature(vcpu, ARM_FEATURE_THUMB2EE);
	}

	return VMM_OK;
}

int cpu_vcpu_sysregs_deinit(struct vmm_vcpu *vcpu)
{
	/* For now nothing to do here. */
	return VMM_OK;
}

