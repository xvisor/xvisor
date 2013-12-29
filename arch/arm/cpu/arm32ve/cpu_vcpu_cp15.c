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
 * @file cpu_vcpu_cp15.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief VCPU CP15 Emulation
 * @details This source file implements CP15 coprocessor for each VCPU.
 */

#include <vmm_error.h>
#include <vmm_smp.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_cache.h>
#include <libs/stringlib.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_cp15.h>

#include <arm_features.h>
#include <mmu_lpae.h>
#include <emulate_arm.h>
#include <emulate_thumb.h>

bool cpu_vcpu_cp15_read(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs,
			u32 opc1, u32 opc2, u32 CRn, u32 CRm, 
			u32 *data)
{
	struct arm_priv_cp15 *cp15 = &arm_priv(vcpu)->cp15;

	*data = 0x0;
	switch (CRn) {
	case 1: /* System configuration. */
		switch (opc2) {
		case 1: /* Auxiliary control register. */
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
	case 9:
		switch (opc1) {
		case 0:	/* L1 cache. */
			switch (opc2) {
			case 0:
				*data = cp15->c9_data;
				break;
			case 1:
				*data = cp15->c9_insn;
				break;
			default:
				goto bad_reg;
			};
			break;
		case 1:	/* L2 cache */
			if (CRm != 0)
				goto bad_reg;
			/* L2 Lockdown and Auxiliary control. */
			*data = 0;
			break;
		default:
			goto bad_reg;
		};
		break;
	case 15: /* Implementation specific. */
		switch (opc1) {
		case 0:
			switch (arm_cpuid(vcpu)) {
			case ARM_CPUID_CORTEXA9:
			case ARM_CPUID_CORTEXA15:
				/* PCR: Power control register */
				/* Read always zero. */
				*data = 0x0;
				break;
			default:
				goto bad_reg;
			};
			break;
		case 4:
			switch (arm_cpuid(vcpu)) {
			case ARM_CPUID_CORTEXA9:
				/* CBAR: Configuration Base Address Register */
				*data = 0x1e000000;
				break;
			case ARM_CPUID_CORTEXA15:
				/* CBAR: Configuration Base Address Register */
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
	}
	return TRUE;
bad_reg:
	vmm_printf("%s: vcpu=%d opc1=%x opc2=%x CRn=%x CRm=%x (invalid)\n", 
				__func__, vcpu->id, opc1, opc2, CRn, CRm);
	return FALSE;
}

bool cpu_vcpu_cp15_write(struct vmm_vcpu *vcpu, 
			 arch_regs_t *regs,
			 u32 opc1, u32 opc2, u32 CRn, u32 CRm, 
			 u32 data)
{
	struct arm_priv_cp15 *cp15 = &arm_priv(vcpu)->cp15;

	switch (CRn) {
	case 1: /* System configuration. */
		switch (opc2) {
		case 1: /* Auxiliary control register. */
			if (!arm_feature(vcpu, ARM_FEATURE_AUXCR))
				goto bad_reg;
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
				vmm_cpumask_setall(&cp15->dflush_needed);
				vmm_cpumask_clear_cpu(vmm_smp_processor_id(),
						      &cp15->dflush_needed);
				asm volatile("mcr p15, 0, %0, c7, c14, 2" 
					: : "r" (data));
				break;
			default:
				goto bad_reg;
			};
			break;
		case 10: /* DCCSW */
			switch (opc2) {
			case 2:
				vmm_cpumask_setall(&cp15->dflush_needed);
				vmm_cpumask_clear_cpu(vmm_smp_processor_id(),
						      &cp15->dflush_needed);
				asm volatile("mcr p15, 0, %0, c7, c10, 2" 
					: : "r" (data));
				break;
			default:
				goto bad_reg;
			};
			break;
		default:
			goto bad_reg;
		};
		break;
	case 9:
		switch (CRm) {
		case 0:	/* Cache lockdown. */
			switch (opc1) {
			case 0:	/* L1 cache.  */
				switch (opc2) {
				case 0:
					cp15->c9_data = data;
					break;
				case 1:
					cp15->c9_insn = data;
					break;
				default:
					goto bad_reg;
				}
				break;
			case 1:	/* L2 cache. */
				/* Ignore writes to
				 * L2 lockdown/auxiliary registers.
				 */
				break;
			default:
				goto bad_reg;
			}
			break;
		case 1:	/* TCM memory region registers. */
			/* Not implemented.  */
			goto bad_reg;
		case 12: /* Performance monitor control */
			/* Performance monitors are implementation
			 * defined in v7, but with an ARM recommended
			 * set of registers, which we follow (although
			 * we don't actually implement any counters)
			 */
			if (!arm_feature(vcpu, ARM_FEATURE_V7)) {
				goto bad_reg;
			}
			switch (opc2) {
			case 0:	/* performance monitor control register */
				/* only the DP, X, D and E bits are writable */
				cp15->c9_pmcr &= ~0x39;
				cp15->c9_pmcr |= (data & 0x39);
				break;
			case 1:	/* Count enable set register */
				data &= (1 << 31);
				cp15->c9_pmcnten |= data;
				break;
			case 2:	/* Count enable clear */
				data &= (1 << 31);
				cp15->c9_pmcnten &= ~data;
				break;
			case 3:	/* Overflow flag status */
				cp15->c9_pmovsr &= ~data;
				break;
			case 4:	/* Software increment */
				/* RAZ/WI since we don't implement the
				 * software-count event
				 */
				break;
			case 5:	/* Event counter selection register */
				/* Since we don't implement any events,
				 * writing to this register is actually
				 * UNPREDICTABLE. So we choose to RAZ/WI.
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
			case 0:	/* Cycle count register */
				/* Not implemented, so RAZ/WI */
				break;
			case 1:	/* Event type select */
				cp15->c9_pmxevtyper = data & 0xff;
				break;
			case 2:	/* Event count register */
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
			case 0:	/* user enable */
				cp15->c9_pmuserenr = data & 1;
				break;
			case 1:	/* interrupt enable set */
				/* We have no event counters so only
				 * the C bit can be changed
				 */
				data &= (1 << 31);
				cp15->c9_pminten |= data;
				break;
			case 2:	/* interrupt enable clear */
				data &= (1 << 31);
				cp15->c9_pminten &= ~data;
				break;
			}
			break;
		default:
			goto bad_reg;
		}
		break;
	case 15: /* Implementation specific. */
		switch (opc1) {
		case 0:
			switch (arm_cpuid(vcpu)) {
			case ARM_CPUID_CORTEXA9:
			case ARM_CPUID_CORTEXA15:
				/* Power Control Register */
				/* Ignore writes. */;
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
	}
	return TRUE;
bad_reg:
	vmm_printf("%s: vcpu=%d opc1=%x opc2=%x CRn=%x CRm=%x (invalid)\n", 
				__func__, vcpu->id, opc1, opc2, CRn, CRm);
	return FALSE;
}

void cpu_vcpu_cp15_regs_save(struct vmm_vcpu *vcpu)
{
	struct arm_priv_cp15 *cp15 = &arm_priv(vcpu)->cp15;

	cp15->c0_cssel = read_csselr();
	cp15->c1_sctlr = read_sctlr();
	cp15->c1_cpacr = read_cpacr();
	cp15->c2_ttbr0 = read_ttbr0();
	cp15->c2_ttbr1 = read_ttbr1();
	cp15->c2_ttbcr = read_ttbcr();
	cp15->c3_dacr = read_dacr();
	cp15->c5_ifsr = read_ifsr();
	cp15->c5_dfsr = read_dfsr();
	cp15->c5_aifsr = read_aifsr();
	cp15->c5_adfsr = read_adfsr();
	cp15->c6_ifar = read_ifar();
	cp15->c6_dfar = read_dfar();
	cp15->c7_par = read_par();
	cp15->c7_par64 = read_par64();
	cp15->c10_prrr = read_prrr();
	cp15->c10_nmrr = read_nmrr();
	cp15->c12_vbar = read_vbar();
	cp15->c13_fcseidr = read_fcseidr();
	cp15->c13_contextidr = read_contextidr();
	cp15->c13_tls1 = read_tpidrurw();
	cp15->c13_tls2 = read_tpidruro();
	cp15->c13_tls3 = read_tpidrprw();
}

void cpu_vcpu_cp15_regs_restore(struct vmm_vcpu *vcpu)
{
	struct arm_priv_cp15 *cp15 = &arm_priv(vcpu)->cp15;

	mmu_lpae_stage2_chttbl(vcpu->guest->id, 
			       arm_guest_priv(vcpu->guest)->ttbl);
	write_vpidr(cp15->c0_midr);
	write_vmpidr(cp15->c0_mpidr);
	write_csselr(cp15->c0_cssel);
	write_sctlr(cp15->c1_sctlr);
	write_cpacr(cp15->c1_cpacr);
	write_ttbr0(cp15->c2_ttbr0);
	write_ttbr1(cp15->c2_ttbr1);
	write_ttbcr(cp15->c2_ttbcr);
	write_dacr(cp15->c3_dacr);
	write_ifsr(cp15->c5_ifsr);
	write_dfsr(cp15->c5_dfsr);
	write_aifsr(cp15->c5_aifsr);
	write_adfsr(cp15->c5_adfsr);
	write_ifar(cp15->c6_ifar);
	write_dfar(cp15->c6_dfar);
	write_par(cp15->c7_par);
	write_par64(cp15->c7_par64);
	write_prrr(cp15->c10_prrr);
	write_nmrr(cp15->c10_nmrr);
	write_vbar(cp15->c12_vbar);
	write_fcseidr(cp15->c13_fcseidr);
	write_contextidr(cp15->c13_contextidr);
	write_tpidrurw(cp15->c13_tls1);
	write_tpidruro(cp15->c13_tls2);
	write_tpidrprw(cp15->c13_tls3);

	/* Check whether vcpu requires dcache to be flushed on
	 * this host CPU. This is a consequence of doing dcache
	 * operations by set/way.
	 */
	if (vmm_cpumask_test_and_clear_cpu(vmm_smp_processor_id(), 
					   &cp15->dflush_needed)) {
		vmm_flush_cache_all();
	}
}

void cpu_vcpu_cp15_regs_dump(struct vmm_chardev *cdev,
			     struct vmm_vcpu *vcpu)
{
	u32 i;
	struct arm_priv_cp15 *cp15 = &arm_priv(vcpu)->cp15;

	vmm_cprintf(cdev, "CP15 Identification Registers\n");
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x %7s=0x%08x\n",
		    "MIDR", cp15->c0_midr,
		    "MPIDR", cp15->c0_mpidr,
		    "CTR", cp15->c0_cachetype);
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x %7s=0x%08x\n",
		    "PFR0", cp15->c0_pfr0,
		    "PFR1", cp15->c0_pfr1,
		    "DFR0", cp15->c0_dfr0);
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x %7s=0x%08x\n",
		    "AFR0", cp15->c0_afr0,
		    "MMFR0", cp15->c0_mmfr0,
		    "MMFR1", cp15->c0_mmfr1);
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x %7s=0x%08x\n",
		    "MMFR2", cp15->c0_mmfr2,
		    "MMFR3", cp15->c0_mmfr3,
		    "ISAR0", cp15->c0_isar0);
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x %7s=0x%08x\n",
		    "ISAR1", cp15->c0_isar1,
		    "ISAR2", cp15->c0_isar2,
		    "ISAR3", cp15->c0_isar3);
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x %7s=0x%08x",
		    "ISAR4", cp15->c0_isar4,
		    "ISAR5", cp15->c0_isar5,
		    "CSSID00", cp15->c0_ccsid[0]);
	for (i = 1; i < 16; i++) {
		if (i % 3 == 1) {
			vmm_cprintf(cdev, "\n");
		}
		vmm_cprintf(cdev, " %5s%02d=0x%08x",
			    "CCSID", i, cp15->c0_ccsid[i]);
	}
	vmm_cprintf(cdev, "\n");
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x\n",
		    "CLID", cp15->c0_clid,
		    "CSSEL", cp15->c0_cssel);
	vmm_cprintf(cdev, "CP15 Control Registers\n");
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x %7s=0x%08x\n",
		    "SCTLR", cp15->c1_sctlr,
		    "CPACR", cp15->c1_cpacr,
		    "VBAR", cp15->c12_vbar);
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x\n",
		    "FCSEIDR", cp15->c13_fcseidr,
		    "CNTXIDR", cp15->c13_contextidr);
	vmm_cprintf(cdev, "CP15 MMU Registers\n");
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x %7s=0x%08x\n",
		    "TTBR0", cp15->c2_ttbr0,
		    "TTBR1", cp15->c2_ttbr1,
		    "TTBCR", cp15->c2_ttbcr);
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x %7s=0x%08x\n",
		    "DACR", cp15->c3_dacr,
		    "PRRR", cp15->c10_prrr,
		    "NMRR", cp15->c10_nmrr);
	vmm_cprintf(cdev, "CP15 Fault Status Registers\n");
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x %7s=0x%08x\n",
		    "IFSR", cp15->c5_ifsr,
		    "DFSR", cp15->c5_dfsr,
		    "AIFSR", cp15->c5_aifsr);
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x %7s=0x%08x\n",
		    "ADFSR", cp15->c5_adfsr,
		    "IFAR", cp15->c6_ifar,
		    "DFAR", cp15->c6_dfar);
	vmm_cprintf(cdev, "CP15 Address Translation Registers\n");
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%016llx\n",
		    "PAR", cp15->c7_par,
		    "PAR64", cp15->c7_par64);
	vmm_cprintf(cdev, "CP15 Cache Lockdown Registers\n");
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x\n",
		    "CILOCK", cp15->c9_insn,  /* ??? */
		    "CDLOCK", cp15->c9_data); /* ??? */
	vmm_cprintf(cdev, "CP15 Performance Monitor Control Registers\n");
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x %7s=0x%08x\n",
		    "PMCR", cp15->c9_pmcr,
		    "PMCNTEN", cp15->c9_pmcnten,
		    "PMOVSR", cp15->c9_pmovsr);
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x %7s=0x%08x\n",
		    "PMXEVTY", cp15->c9_pmxevtyper,
		    "PMUSREN", cp15->c9_pmuserenr,
		    "PMINTEN", cp15->c9_pminten);
	vmm_cprintf(cdev, "CP15 Thread Local Storage Registers\n");
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x %7s=0x%08x\n",
		    "TPIDURW", cp15->c13_tls1,
		    "TPIDURO", cp15->c13_tls2,
		    "TPIDPRW", cp15->c13_tls3);
}

int cpu_vcpu_cp15_init(struct vmm_vcpu *vcpu, u32 cpuid)
{
	u32 i, cache_type, last_level;
	struct arm_priv_cp15 *cp15 = &arm_priv(vcpu)->cp15;

	/* Clear all CP15 registers */
	memset(cp15, 0, sizeof(struct arm_priv_cp15));

	/* Reset values of all CP15 registers.
	 *
	 * Note: Almost all CP15 registers would be same as underlying
	 * host because Guest VCPU will directly access these without
	 * trapping. Due to this, quite a few CP15 registers initialized
	 * below are for debugging purpose only.
	 */
	cp15->c2_ttbcr = 0x0;
	cp15->c2_ttbr0 = 0x0;
	cp15->c2_ttbr1 = 0x0;
	cp15->c9_pmcr = (cpuid & 0xFF000000);
	cp15->c10_prrr = 0x0;
	cp15->c10_nmrr = 0x0;
	cp15->c12_vbar = 0x0;
	switch (cpuid) {
	case ARM_CPUID_CORTEXA8:
		cp15->c0_midr = cpuid;
		cp15->c0_mpidr = vcpu->subid;
		cp15->c0_cachetype = 0x82048004;
		cp15->c0_pfr0 = 0x1031;
		cp15->c0_pfr1 = 0x11;
		cp15->c0_dfr0 = 0x400;
		cp15->c0_afr0 = 0x0;
		cp15->c0_mmfr0 = 0x31100003;
		cp15->c0_mmfr1 = 0x20000000;
		cp15->c0_mmfr2 = 0x01202000;
		cp15->c0_mmfr3 = 0x11;
		cp15->c0_isar0 = 0x00101111;
		cp15->c0_isar1 = 0x12112111;
		cp15->c0_isar2 = 0x21232031;
		cp15->c0_isar3 = 0x11112131;
		cp15->c0_isar4 = 0x00111142;
		cp15->c0_isar5 = 0x0;
		cp15->c0_clid = (1 << 27) | (2 << 24) | 3;
		cp15->c0_ccsid[0] = 0xe007e01a;	/* 16k L1 dcache. */
		cp15->c0_ccsid[1] = 0x2007e01a;	/* 16k L1 icache. */
		cp15->c0_ccsid[2] = 0xf0000000;	/* No L2 icache. */
		cp15->c1_sctlr = 0x00c50078;
		break;
	case ARM_CPUID_CORTEXA9:
		/* Guest ARM32 Linux running on Cortex-A9
		 * tries to use few ARMv7 instructions which 
		 * are removed in AArch32 instruction set.
		 * 
		 * To take care of this situation, we fake 
		 * PartNum and Revison visible to Cortex-A9
		 * Guest VCPUs.
		 */
		cp15->c0_midr = cpuid;
		cp15->c0_midr &= ~(MIDR_PARTNUM_MASK|MIDR_REVISON_MASK);
		cp15->c0_mpidr = (1 << 31) | vcpu->subid;
		cp15->c0_cachetype = 0x80038003;
		cp15->c0_pfr0 = 0x1031;
		cp15->c0_pfr1 = 0x11;
		cp15->c0_dfr0 = 0x000;
		cp15->c0_afr0 = 0x0;
		cp15->c0_mmfr0 = 0x00100103;
		cp15->c0_mmfr1 = 0x20000000;
		cp15->c0_mmfr2 = 0x01230000;
		cp15->c0_mmfr3 = 0x00002111;
		cp15->c0_isar0 = 0x00101111;
		cp15->c0_isar1 = 0x13112111;
		cp15->c0_isar2 = 0x21232041;
		cp15->c0_isar3 = 0x11112131;
		cp15->c0_isar4 = 0x00111142;
		cp15->c0_isar5 = 0x0;
		cp15->c0_clid = (1 << 27) | (1 << 24) | 3;
		cp15->c0_ccsid[0] = 0xe00fe015;	/* 16k L1 dcache. */
		cp15->c0_ccsid[1] = 0x200fe015;	/* 16k L1 icache. */
		cp15->c1_sctlr = 0x00c50078;
		break;
	case ARM_CPUID_CORTEXA15:
		cp15->c0_midr = cpuid;
		cp15->c0_mpidr = (1 << 31) | vcpu->subid;
		cp15->c0_cachetype = 0x8444c004;
		cp15->c0_pfr0 = 0x00001131;
		cp15->c0_pfr1 = 0x00011011;
		cp15->c0_dfr0 = 0x02010555;
		cp15->c0_afr0 = 0x00000000;
		cp15->c0_mmfr0 = 0x10201105;
		cp15->c0_mmfr1 = 0x20000000;
		cp15->c0_mmfr2 = 0x01240000;
		cp15->c0_mmfr3 = 0x02102211;
		cp15->c0_isar0 = 0x02101110;
		cp15->c0_isar1 = 0x13112111;
		cp15->c0_isar2 = 0x21232041;
		cp15->c0_isar3 = 0x11112131;
		cp15->c0_isar4 = 0x10011142;
		cp15->c0_clid = 0x0a200023;
		cp15->c0_ccsid[0] = 0x701fe00a; /* 32K L1 dcache */
		cp15->c0_ccsid[1] = 0x201fe00a; /* 32K L1 icache */
		cp15->c0_ccsid[2] = 0x711fe07a; /* 4096K L2 unified cache */
		cp15->c1_sctlr = 0x00c50078;
		break;
	default:
		break;
	}

	/* Current strategy is to show identification registers same
	 * as underlying Host HW so that Guest sees same capabilities
	 * as Host HW.
	 */
	cp15->c0_pfr0 = read_pfr0();
	cp15->c0_pfr1 = read_pfr1();
	cp15->c0_dfr0 = read_dfr0();
	cp15->c0_afr0 = read_afr0();
	cp15->c0_mmfr0 = read_mmfr0();
	cp15->c0_mmfr1 = read_mmfr1();
	cp15->c0_mmfr2 = read_mmfr2();
	cp15->c0_mmfr3 = read_mmfr3();
	cp15->c0_isar0 = read_isar0();
	cp15->c0_isar1 = read_isar1();
	cp15->c0_isar2 = read_isar2();
	cp15->c0_isar3 = read_isar3();
	cp15->c0_isar4 = read_isar4();
	cp15->c0_isar5 = read_isar5();

	/* Cache config register such as CTR, CLIDR, and
	 * CCSIDRx should be same as that of underlying host.
	 *
	 * Note: This for debugging purpose only. The Guest VCPU
	 * will directly access host registers without trapping.
	 */
	cp15->c0_cachetype = read_ctr();
	cp15->c0_clid = read_clidr();
	last_level = (cp15->c0_clid & CLIDR_LOUU_MASK) >> CLIDR_LOUU_SHIFT;
	for (i = 0; i <= last_level; i++) {
		cache_type = cp15->c0_clid >> (i * 3);
		cache_type &= 0x7;
		switch (cache_type) {
		case CLIDR_CTYPE_ICACHE:
			write_csselr((i << 1) | 1);
			cp15->c0_ccsid[(i << 1) | 1] = read_ccsidr();
			break;
		case CLIDR_CTYPE_DCACHE:
		case CLIDR_CTYPE_UNICACHE:
			write_csselr(i << 1);
			cp15->c0_ccsid[i << 1] = read_ccsidr();
			break;
		case CLIDR_CTYPE_SPLITCACHE:
			write_csselr(i << 1);
			cp15->c0_ccsid[i << 1] = read_ccsidr();
			write_csselr((i << 1) | 1);
			cp15->c0_ccsid[(i << 1) | 1] = read_ccsidr();
			break;
		case CLIDR_CTYPE_NOCACHE:
		case CLIDR_CTYPE_RESERVED1:
		case CLIDR_CTYPE_RESERVED2:
		case CLIDR_CTYPE_RESERVED3:
			cp15->c0_ccsid[i << 1] = 0;
			cp15->c0_ccsid[(i << 1) | 1] = 0;
			break;
		};
	}

	/* Clear the dcache flush needed mask */
	vmm_cpumask_clear(&cp15->dflush_needed);

	return VMM_OK;
}

int cpu_vcpu_cp15_deinit(struct vmm_vcpu *vcpu)
{
	/* For now nothing to do here. */
	return VMM_OK;
}

