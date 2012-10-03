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

#include <vmm_heap.h>
#include <vmm_error.h>
#include <vmm_scheduler.h>
#include <vmm_host_aspace.h>
#include <vmm_guest_aspace.h>
#include <vmm_vcpu_irq.h>
#include <libs/stringlib.h>
#include <emulate_arm.h>
#include <emulate_thumb.h>
#include <cpu_mmu.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_helper.h>
#include <cpu_vcpu_emulate.h>
#include <cpu_vcpu_cp15.h>

static int cpu_vcpu_cp15_stage2_map(struct vmm_vcpu *vcpu, 
				    arch_regs_t *regs,
				    physical_addr_t fipa)
{
	int rc;
	irq_flags_t f;
	u32 reg_flags = 0x0;
	struct cpu_page pg;
	physical_size_t availsz;

	memset(&pg, 0, sizeof(pg));

	pg.ia = fipa & TTBL_L3_MAP_MASK;
	pg.sz = TTBL_L3_BLOCK_SIZE;

	rc = vmm_guest_physical_map(vcpu->guest, pg.ia, pg.sz, 
				    &pg.oa, &availsz, &reg_flags);
	if (rc) {
		return rc;
	}

	if (availsz < TTBL_L3_BLOCK_SIZE) {
		return VMM_EFAIL;
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

	if (reg_flags & VMM_REGION_CACHEABLE) {
		if (reg_flags & VMM_REGION_BUFFERABLE) {
			pg.memattr = 0xF;
		} else {
			pg.memattr = 0xA;
		}
	} else {
		pg.memattr = 0x0;
	}

	vmm_spin_lock_irqsave(&arm_guest_priv(vcpu->guest)->ttbl_lock, f);
	rc = cpu_mmu_map_page(arm_guest_priv(vcpu->guest)->ttbl, &pg);
	vmm_spin_unlock_irqrestore(&arm_guest_priv(vcpu->guest)->ttbl_lock, f);

	return rc;
}

int cpu_vcpu_cp15_inst_abort(struct vmm_vcpu *vcpu, 
			     arch_regs_t *regs,
			     u32 il, u32 iss, 
			     virtual_addr_t ifar,
			     physical_addr_t fipa)
{
	switch (iss & ISS_ABORT_FSR_MASK) {
	case FSR_TRANS_FAULT_LEVEL1:
	case FSR_TRANS_FAULT_LEVEL2:
	case FSR_TRANS_FAULT_LEVEL3:
		return cpu_vcpu_cp15_stage2_map(vcpu, regs, fipa);
	default:
		break;
	};

	return VMM_EFAIL;
}

int cpu_vcpu_cp15_data_abort(struct vmm_vcpu *vcpu, 
			     arch_regs_t *regs,
			     u32 il, u32 iss, 
			     virtual_addr_t dfar,
			     physical_addr_t fipa)
{
	u32 read_count, inst;
	physical_addr_t inst_pa;

	switch (iss & ISS_ABORT_FSR_MASK) {
	case FSR_TRANS_FAULT_LEVEL1:
	case FSR_TRANS_FAULT_LEVEL2:
	case FSR_TRANS_FAULT_LEVEL3:
		return cpu_vcpu_cp15_stage2_map(vcpu, regs, fipa);
	case FSR_ACCESS_FAULT_LEVEL1:
	case FSR_ACCESS_FAULT_LEVEL2:
	case FSR_ACCESS_FAULT_LEVEL3:
		if (!(iss & ISS_ABORT_ISV_MASK)) {
			/* Determine instruction physical address */
			va2pa_ns_pr(regs->pc);
			inst_pa = read_par64();
			inst_pa &= PAR64_PA_MASK;
			inst_pa |= (regs->pc & 0x00000FFF);

			/* Read the faulting instruction */
			read_count = vmm_host_physical_read(inst_pa, &inst, sizeof(inst));
			if (read_count != sizeof(inst)) {
				return VMM_EFAIL;
			}
			if (regs->cpsr & CPSR_THUMB_ENABLED) {
				return emulate_thumb_inst(vcpu, regs, inst);
			} else {
				return emulate_arm_inst(vcpu, regs, inst);
			}
		} else {
			if (iss & ISS_ABORT_WNR_MASK) {
				return cpu_vcpu_emulate_store(vcpu, regs, 
							      il, iss, fipa);
			} else {
				return cpu_vcpu_emulate_load(vcpu, regs, 
							     il, iss, fipa);
			}
		}
	default:
		break;
	};

	return VMM_EFAIL;
}


bool cpu_vcpu_cp15_read(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs,
			u32 opc1, u32 opc2, u32 CRn, u32 CRm, 
			u32 *data)
{
	*data = 0x0;
	switch (CRn) {
	case 1: /* System configuration.  */
		switch (opc2) {
		case 0: /* Control register.  */
			*data = arm_priv(vcpu)->cp15.c1_sctlr;
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
		case 2: /* Coprocessor access register.  */
			*data = arm_priv(vcpu)->cp15.c1_cpacr;
			break;
		default:
			goto bad_reg;
		};
		break;
	case 9:
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
	default:
		goto bad_reg;
	}
	return TRUE;
bad_reg:
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
		case 0:
			arm_priv(vcpu)->cp15.c1_sctlr = data;
			write_sctlr(data & ~(SCTLR_A_MASK));
			break;
		case 1: /* Auxiliary control register.  */
			/* Not implemented.  */
			break;
		case 2:
			arm_priv(vcpu)->cp15.c1_cpacr = data;
			write_cpacr(data);
			break;
		default:
			goto bad_reg;
		};
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
	default:
		goto bad_reg;
	}
	return TRUE;
bad_reg:
	return FALSE;
}

void cpu_vcpu_cp15_switch_context(struct vmm_vcpu * tvcpu, 
				  struct vmm_vcpu * vcpu)
{
	if (tvcpu && tvcpu->is_normal) {
		arm_priv(tvcpu)->cp15.c0_cssel = read_csselr();
		arm_priv(tvcpu)->cp15.c1_sctlr = read_sctlr();
		arm_priv(tvcpu)->cp15.c2_ttbr0 = read_ttbr0();
		arm_priv(tvcpu)->cp15.c2_ttbr1 = read_ttbr1();
		arm_priv(tvcpu)->cp15.c2_ttbcr = read_ttbcr();
		arm_priv(tvcpu)->cp15.c3_dacr = read_dacr();
		arm_priv(tvcpu)->cp15.c5_ifsr = read_ifsr();
		arm_priv(tvcpu)->cp15.c5_dfsr = read_dfsr();
		arm_priv(tvcpu)->cp15.c6_ifar = read_ifar();
		arm_priv(tvcpu)->cp15.c6_dfar = read_dfar();
		arm_priv(tvcpu)->cp15.c7_par = read_par();
		arm_priv(tvcpu)->cp15.c7_par64 = read_par64();
		arm_priv(tvcpu)->cp15.c10_prrr = read_prrr();
		arm_priv(tvcpu)->cp15.c10_nmrr = read_nmrr();
		arm_priv(tvcpu)->cp15.c12_vbar = read_vbar();
		arm_priv(tvcpu)->cp15.c13_fcseidr = read_fcseidr();
		arm_priv(tvcpu)->cp15.c13_contextidr = read_contextidr();
		arm_priv(tvcpu)->cp15.c13_tls1 = read_tpidrurw();
		arm_priv(tvcpu)->cp15.c13_tls2 = read_tpidruro();
		arm_priv(tvcpu)->cp15.c13_tls3 = read_tpidrprw();
	}
	if (vcpu->is_normal) {
		cpu_mmu_stage2_chttbl(vcpu->guest->id, 
				      arm_guest_priv(vcpu->guest)->ttbl);
		write_vpidr(arm_priv(vcpu)->cp15.c0_cpuid);
		if (arm_feature(vcpu, ARM_FEATURE_V7MP)) {
			write_vmpidr((1 << 31) | vcpu->subid);
		} else {
			write_vmpidr(vcpu->subid);
		}
		write_csselr(arm_priv(vcpu)->cp15.c0_cssel);
		write_sctlr(arm_priv(vcpu)->cp15.c1_sctlr & ~(SCTLR_A_MASK));
		write_cpacr(arm_priv(vcpu)->cp15.c1_cpacr);
		write_ttbr0(arm_priv(vcpu)->cp15.c2_ttbr0);
		write_ttbr1(arm_priv(vcpu)->cp15.c2_ttbr1);
		write_ttbcr(arm_priv(vcpu)->cp15.c2_ttbcr);
		write_dacr(arm_priv(vcpu)->cp15.c3_dacr);
		write_ifsr(arm_priv(vcpu)->cp15.c5_ifsr);
		write_dfsr(arm_priv(vcpu)->cp15.c5_dfsr);
		write_ifar(arm_priv(vcpu)->cp15.c6_ifar);
		write_dfar(arm_priv(vcpu)->cp15.c6_dfar);
		write_par(arm_priv(vcpu)->cp15.c7_par);
		write_par64(arm_priv(vcpu)->cp15.c7_par64);
		write_prrr(arm_priv(vcpu)->cp15.c10_prrr);
		write_nmrr(arm_priv(vcpu)->cp15.c10_nmrr);
		write_vbar(arm_priv(vcpu)->cp15.c12_vbar);
		write_fcseidr(arm_priv(vcpu)->cp15.c13_fcseidr);
		write_contextidr(arm_priv(vcpu)->cp15.c13_contextidr);
		write_tpidrurw(arm_priv(vcpu)->cp15.c13_tls1);
		write_tpidruro(arm_priv(vcpu)->cp15.c13_tls2);
		write_tpidrprw(arm_priv(vcpu)->cp15.c13_tls3);
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

int cpu_vcpu_cp15_init(struct vmm_vcpu *vcpu, u32 cpuid)
{
	u32 i, cache_type, last_level;

	memset(&arm_priv(vcpu)->cp15, 0, sizeof(arm_priv(vcpu)->cp15));

	arm_priv(vcpu)->cp15.c0_cpuid = cpuid;
	arm_priv(vcpu)->cp15.c2_ttbcr = 0x0;
	arm_priv(vcpu)->cp15.c9_pmcr = (cpuid & 0xFF000000);
	/* Reset values of important registers */
	switch (cpuid) {
	case ARM_CPUID_CORTEXA8:
		memcpy(arm_priv(vcpu)->cp15.c0_c1, cortexa8_cp15_c0_c1, 
							8 * sizeof(u32));
		memcpy(arm_priv(vcpu)->cp15.c0_c2, cortexa8_cp15_c0_c2, 
							8 * sizeof(u32));
		arm_priv(vcpu)->cp15.c1_sctlr = 0x00c50078;
		break;
	case ARM_CPUID_CORTEXA9:
		memcpy(arm_priv(vcpu)->cp15.c0_c1, cortexa9_cp15_c0_c1, 
							8 * sizeof(u32));
		memcpy(arm_priv(vcpu)->cp15.c0_c2, cortexa9_cp15_c0_c2, 
							8 * sizeof(u32));
		arm_priv(vcpu)->cp15.c1_sctlr = 0x00c50078;
		break;
	default:
		break;
	};
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

	return VMM_OK;
}

int cpu_vcpu_cp15_deinit(struct vmm_vcpu *vcpu)
{
	memset(&arm_priv(vcpu)->cp15, 0, sizeof(arm_priv(vcpu)->cp15));

	return VMM_OK;
}

