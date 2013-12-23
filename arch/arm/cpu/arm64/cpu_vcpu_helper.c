/**
 * Copyright (c) 2013 Sukanto Ghosh.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modifycpu_vcpu_helper.c
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
 * @file cpu_vcpu_helper.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief source of VCPU helper functions
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_timer.h>
#include <vmm_manager.h>
#include <vmm_scheduler.h>
#include <arch_barrier.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <cpu_defines.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_spr.h>
#include <cpu_vcpu_helper.h>
#include <generic_timer.h>
#include <arm_features.h>
#include <mmu_lpae.h>

void cpu_vcpu_halt(struct vmm_vcpu *vcpu, arch_regs_t *regs)
{
	if (vmm_manager_vcpu_get_state(vcpu) != VMM_VCPU_STATE_HALTED) {
		vmm_printf("\n");
		cpu_vcpu_dump_user_reg(regs);
		vmm_manager_vcpu_halt(vcpu);
	}
}

static u32 __cpu_vcpu_regmode32_read(arch_regs_t *regs, 
				     u32 mode, u32 reg)
{
	switch (reg) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
		return regs->gpr[reg];
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
		if (mode == CPSR_MODE_FIQ) {
			return regs->gpr[16 + reg];
		} else {
			return regs->gpr[reg];
		}
	case 13:
		switch (mode) {
		case CPSR_MODE_USER:
		case CPSR_MODE_SYSTEM:
			return regs->gpr[13];
		case CPSR_MODE_FIQ:
			return regs->gpr[29];
		case CPSR_MODE_IRQ:
			return regs->gpr[17];
		case CPSR_MODE_SUPERVISOR:
			return regs->gpr[19];
		case CPSR_MODE_ABORT:
			return regs->gpr[21];
		case CPSR_MODE_UNDEFINED:
			return regs->gpr[23];
		case CPSR_MODE_HYPERVISOR:
			return regs->gpr[15];
		default:
			break;
		};
		break;
	case 14:
		switch (mode) {
		case CPSR_MODE_USER:
		case CPSR_MODE_SYSTEM:
			return regs->gpr[14];
		case CPSR_MODE_FIQ:
			return regs->lr;
		case CPSR_MODE_IRQ:
			return regs->gpr[16];
		case CPSR_MODE_SUPERVISOR:
			return regs->gpr[18];
		case CPSR_MODE_ABORT:
			return regs->gpr[20];
		case CPSR_MODE_UNDEFINED:
			return regs->gpr[22];
		case CPSR_MODE_HYPERVISOR:
			return regs->gpr[14];
		default:
			break;
		};
		break;
	case 15:
		return regs->pc;
	default:
		break;
	};

	return 0x0;
}

static void __cpu_vcpu_regmode32_write(arch_regs_t *regs, 
				       u32 mode, u32 reg, u32 val)
{
	switch (reg) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
		regs->gpr[reg] = val;
		break;
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
		if (mode == CPSR_MODE_FIQ) {
			regs->gpr[16 + reg] = val;
		} else {
			regs->gpr[reg] = val;
		}
		break;
	case 13:
		switch (mode) {
		case CPSR_MODE_USER:
		case CPSR_MODE_SYSTEM:
			regs->gpr[13] = val;
			break;
		case CPSR_MODE_FIQ:
			regs->gpr[29] = val;
			break;
		case CPSR_MODE_IRQ:
			regs->gpr[17] = val;
			break;
		case CPSR_MODE_SUPERVISOR:
			regs->gpr[19] = val;
			break;
		case CPSR_MODE_ABORT:
			regs->gpr[21] = val;
			break;
		case CPSR_MODE_UNDEFINED:
			regs->gpr[23] = val;
			break;
		case CPSR_MODE_HYPERVISOR:
			regs->gpr[15] = val;
			break;
		default:
			break;
		};
		break;
	case 14:
		switch (mode) {
		case CPSR_MODE_USER:
		case CPSR_MODE_SYSTEM:
			regs->gpr[14] = val;
		case CPSR_MODE_FIQ:
			regs->lr = val;
		case CPSR_MODE_IRQ:
			regs->gpr[16] = val;
		case CPSR_MODE_SUPERVISOR:
			regs->gpr[18] = val;
		case CPSR_MODE_ABORT:
			regs->gpr[20] = val;
		case CPSR_MODE_UNDEFINED:
			regs->gpr[22] = val;
		case CPSR_MODE_HYPERVISOR:
			regs->gpr[14] = val;
		default:
			break;
		};
		break;
	case 15:
		regs->pc = val;
		break;
	default:
		break;
	};
}

u64 cpu_vcpu_reg64_read(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs, 
			u32 reg) 
{
	u64 ret;

	if (reg < CPU_GPR_COUNT) {
		ret = regs->gpr[reg];
	} else if (reg == 30) {
		ret = regs->lr;
	} else {
		/* No such GPR */
		ret = 0;
	}

	/* Truncate bits[63:32] for AArch32 mode */
	if (regs->pstate & PSR_MODE32) {
		ret = ret & 0xFFFFFFFFULL;
	}

	return ret;
}

void cpu_vcpu_reg64_write(struct vmm_vcpu *vcpu, 
			  arch_regs_t *regs, 
			  u32 reg, u64 val)
{
	/* Truncate bits[63:32] for AArch32 mode */
	if (regs->pstate & PSR_MODE32) {
		val = val & 0xFFFFFFFFULL;
	}

	if (reg < CPU_GPR_COUNT) {
		regs->gpr[reg] = val;
	} else if (reg == 30) {
		regs->lr = val;
	} else {
		/* No such GPR */
	}
}

u64 cpu_vcpu_reg_read(struct vmm_vcpu *vcpu, 
		      arch_regs_t *regs, 
		      u32 reg) 
{
	if (regs->pstate & PSR_MODE32) {
		return __cpu_vcpu_regmode32_read(regs, 
				regs->pstate & PSR_MODE32_MASK, reg & 0xF);
	} else {
		return cpu_vcpu_reg64_read(vcpu, regs, reg);
	}
}

void cpu_vcpu_reg_write(struct vmm_vcpu *vcpu, 
		        arch_regs_t *regs, 
		        u32 reg, u64 val)
{
	if (regs->pstate & PSR_MODE32) {
		__cpu_vcpu_regmode32_write(regs, 
			regs->pstate & PSR_MODE32_MASK, reg & 0xF, val);
	} else {
		cpu_vcpu_reg64_write(vcpu, regs, reg, val);
	}
}

void __cpu_vcpu_spsr32_update(struct vmm_vcpu *vcpu, u32 mode, u32 new_spsr)
{
	switch (mode) {
	case CPSR_MODE_ABORT:
		msr(spsr_abt, new_spsr);
		arm_priv(vcpu)->spsr_abt = new_spsr;
		break;
	case CPSR_MODE_UNDEFINED:
		msr(spsr_und, new_spsr);
		arm_priv(vcpu)->spsr_und = new_spsr;
		break;
	case CPSR_MODE_SUPERVISOR:
		msr(spsr_el1, new_spsr);
		arm_priv(vcpu)->spsr_el1 = new_spsr;
		break;
	case CPSR_MODE_IRQ:
		msr(spsr_irq, new_spsr);
		arm_priv(vcpu)->spsr_irq = new_spsr;
		break;
	case CPSR_MODE_FIQ:
		msr(spsr_fiq, new_spsr);
		arm_priv(vcpu)->spsr_fiq = new_spsr;
		break;
	case CPSR_MODE_HYPERVISOR:
		msr(spsr_el2, new_spsr);
		break;
	default:
		break;
	};
}

static int __cpu_vcpu_inject_und32(struct vmm_vcpu *vcpu,
				   arch_regs_t *regs)
{
	u32 old_cpsr, new_cpsr, sctlr;

	/* Retrive current SCTLR */
	sctlr = mrs(sctlr_el1);

	/* Compute CPSR changes */
	old_cpsr = new_cpsr = regs->pstate & 0xFFFFFFFFULL;
	new_cpsr &= ~CPSR_MODE_MASK;
	new_cpsr |= (CPSR_MODE_UNDEFINED | CPSR_IRQ_DISABLED);
	new_cpsr &= ~(CPSR_IT2_MASK | 
			CPSR_IT1_MASK | 
			CPSR_JAZZLE_ENABLED | 
			CPSR_BE_ENABLED | 
			CPSR_THUMB_ENABLED);
	if (sctlr & SCTLR_TE_MASK) {
		new_cpsr |= CPSR_THUMB_ENABLED;
	}
	if (sctlr & SCTLR_EE_MASK) {
		new_cpsr |= CPSR_BE_ENABLED;
	}

	/* Update CPSR, SPSR, LR and PC */
	__cpu_vcpu_spsr32_update(vcpu, CPSR_MODE_UNDEFINED, old_cpsr);
	__cpu_vcpu_regmode32_write(regs, CPSR_MODE_UNDEFINED, 14, 
		regs->pc - ((old_cpsr & CPSR_THUMB_ENABLED) ? 2 : 4));
	if (sctlr & SCTLR_V_MASK) {
		regs->pc = CPU_IRQ_HIGHVEC_BASE;
	} else {
		regs->pc = mrs(vbar_el1);
	}
	regs->pc = regs->pc & 0xFFFFFFFFULL;
	regs->pc += 4;
	regs->pstate &= ~0xFFFFFFFFULL;
	regs->pstate |= (u64)new_cpsr;

	return VMM_OK;
}

#define EL1_EXCEPT_SYNC_OFFSET 0x200

static int __cpu_vcpu_inject_und64(struct vmm_vcpu *vcpu,
				   arch_regs_t *regs)
{
	u32 esr;

	/* Save old PSTATE to SPSR_EL1 */
	msr(spsr_el1, regs->pstate);

	/* Save current PC to ELR_EL1 */
	msr(elr_el1, regs->pc);

	/* Update PSTATE */
	regs->pstate = (PSR_MODE64_EL1h |
			PSR_ASYNC_ABORT_DISABLED |
			PSR_FIQ_DISABLED |
			PSR_IRQ_DISABLED |
			PSR_MODE64_DEBUG_DISABLED);

	/* Update PC */
	regs->pc = mrs(vbar_el1) + EL1_EXCEPT_SYNC_OFFSET;

	/* Update ESR_EL1 */
	esr = (EC_UNKNOWN << ESR_EC_SHIFT);
	if (mrs(esr_el2) & ESR_IL_MASK) {
		esr |= ESR_IL_MASK;
	}
	msr(esr_el1, esr);

	return VMM_OK;
}

static int __cpu_vcpu_inject_abt32(struct vmm_vcpu *vcpu,
				   arch_regs_t *regs,
				   bool is_pabt,
				   virtual_addr_t addr)
{
	u64 far;
	u32 old_cpsr, new_cpsr, sctlr, ttbcr;

	/* Retrive current SCTLR */
	sctlr = mrs(sctlr_el1);

	/* Compute CPSR changes */
	old_cpsr = new_cpsr = regs->pstate & 0xFFFFFFFFULL;
	new_cpsr &= ~CPSR_MODE_MASK;
	new_cpsr |= (CPSR_MODE_ABORT | 
			CPSR_ASYNC_ABORT_DISABLED | 
			CPSR_IRQ_DISABLED);
	new_cpsr &= ~(CPSR_IT2_MASK | 
			CPSR_IT1_MASK | 
			CPSR_JAZZLE_ENABLED | 
			CPSR_BE_ENABLED | 
			CPSR_THUMB_ENABLED);
	if (sctlr & SCTLR_TE_MASK) {
		new_cpsr |= CPSR_THUMB_ENABLED;
	}
	if (sctlr & SCTLR_EE_MASK) {
		new_cpsr |= CPSR_BE_ENABLED;
	}

	/* Update CPSR, SPSR, LR and PC */
	__cpu_vcpu_spsr32_update(vcpu, CPSR_MODE_ABORT, old_cpsr);
	__cpu_vcpu_regmode32_write(regs, CPSR_MODE_ABORT, 14, 
		regs->pc - ((old_cpsr & CPSR_THUMB_ENABLED) ? 4 : 0));
	if (sctlr & SCTLR_V_MASK) {
		regs->pc = CPU_IRQ_HIGHVEC_BASE;
	} else {
		regs->pc = mrs(vbar_el1);
	}
	regs->pc = regs->pc & 0xFFFFFFFFULL;
	regs->pc += (is_pabt) ? 12 : 16;
	regs->pstate &= ~0xFFFFFFFFULL;
	regs->pstate |= (u64)new_cpsr;

	/* Update abort registers */
	ttbcr = mrs(tcr_el1) & 0xFFFFFFFFULL;
	if (is_pabt) {
		/* Set IFAR and IFSR */
		far = mrs(far_el1) & 0xFFFFFFFFULL;
		far |= (addr << 32) & 0xFFFFFFFF00000000ULL;
		msr(far_el1, far);
		if (ttbcr >> 31) { /* LPAE MMU */
			msr(ifsr32_el2, (1 << 9) | 0x34);
		} else { /* Legacy ARMv6 MMU */
			msr(ifsr32_el2, 0x14);
		}
	} else {
		/* Set DFAR and DFSR */
		far = mrs(far_el1) & 0xFFFFFFFF00000000ULL;
		far |= addr & 0xFFFFFFFFULL;
		msr(far_el1, far);
		if (ttbcr >> 31) { /* LPAE MMU */
			msr(esr_el1, (1 << 9) | 0x34);
		} else { /* Legacy ARMv6 MMU */
			msr(esr_el1, 0x14);
		}
	}

	return VMM_OK;
}

static int __cpu_vcpu_inject_abt64(struct vmm_vcpu *vcpu,
				   arch_regs_t *regs,
				   bool is_pabt,
				   virtual_addr_t addr)
{
	u32 esr, old_pstate;
	bool is_aarch32 = (regs->pstate & PSR_MODE32) ? TRUE : FALSE;

	/* Save old PSTATE to SPSR_EL1 */
	old_pstate = regs->pstate;
	msr(spsr_el1, old_pstate);

	/* Save current PC to ELR_EL1 */
	msr(elr_el1, regs->pc);

	/* Update PSTATE */
	regs->pstate = (PSR_MODE64_EL1h |
			PSR_ASYNC_ABORT_DISABLED |
			PSR_FIQ_DISABLED |
			PSR_IRQ_DISABLED |
			PSR_MODE64_DEBUG_DISABLED);

	/* Update PC */
	regs->pc = mrs(vbar_el1) + EL1_EXCEPT_SYNC_OFFSET;

	/* Update FAR_EL1 */
	msr(far_el1, addr);

	/* Update ESR_EL1
	 * NOTE: The guest runs in AArch64 mode when in EL1. If we get
	 * an AArch32 fault or AArch64 EL0t fault then we have trapped 
	 * guest user space.
	 */
	esr = 0;
	if (is_aarch32 || (old_pstate & PSR_MODE64_MASK) == PSR_MODE64_EL0t) {
		if (is_pabt) {
			esr |= (EC_TRAP_LWREL_INST_ABORT << ESR_EC_SHIFT);
		} else {
			esr |= (EC_TRAP_LWREL_DATA_ABORT << ESR_EC_SHIFT);
		}
	} else {
		if (is_pabt) {
			esr |= (EC_CUREL_INST_ABORT << ESR_EC_SHIFT);
		} else {
			esr |= (EC_CUREL_DATA_ABORT << ESR_EC_SHIFT);
		}
	}
	esr |= FSC_SYNC_EXTERNAL_ABORT;
	if (mrs(esr_el2) & ESR_IL_MASK) {
		esr |= ESR_IL_MASK;
	}
	msr(esr_el1, esr);

	return VMM_OK;
}

int cpu_vcpu_inject_undef(struct vmm_vcpu *vcpu,
			  arch_regs_t *regs)
{
	/* Sanity checks */
	if (!vcpu || !regs) {
		return VMM_EFAIL;
	}
	if (vcpu != vmm_scheduler_current_vcpu()) {
		/* This function should only be called for current VCPU */
		vmm_panic("%d not called for current vcpu\n", __func__);
	}

	if (arm_priv(vcpu)->hcr & HCR_RW_MASK) {
		return __cpu_vcpu_inject_und64(vcpu, regs);
	} else {
		return __cpu_vcpu_inject_und32(vcpu, regs);
	}
}

int cpu_vcpu_inject_pabt(struct vmm_vcpu *vcpu,
			 arch_regs_t *regs)
{
	/* Sanity checks */
	if (!vcpu || !regs) {
		return VMM_EFAIL;
	}
	if (vcpu != vmm_scheduler_current_vcpu()) {
		/* This function should only be called for current VCPU */
		vmm_panic("%d not called for current vcpu\n", __func__);
	}

	if (arm_priv(vcpu)->hcr & HCR_RW_MASK) {
		return __cpu_vcpu_inject_abt64(vcpu, regs, TRUE, regs->pc);
	} else {
		return __cpu_vcpu_inject_abt32(vcpu, regs, TRUE, regs->pc);
	}
}

int cpu_vcpu_inject_dabt(struct vmm_vcpu *vcpu,
			 arch_regs_t *regs,
			 virtual_addr_t addr)
{
	/* Sanity checks */
	if (!vcpu || !regs) {
		return VMM_EFAIL;
	}
	if (vcpu != vmm_scheduler_current_vcpu()) {
		/* This function should only be called for current VCPU */
		vmm_panic("%d not called for current vcpu\n", __func__);
	}

	if (arm_priv(vcpu)->hcr & HCR_RW_MASK) {
		return __cpu_vcpu_inject_abt64(vcpu, regs, FALSE, addr);
	} else {
		return __cpu_vcpu_inject_abt32(vcpu, regs, FALSE, addr);
	}
}

int arch_guest_init(struct vmm_guest *guest)
{
	if (!guest->reset_count) {
		guest->arch_priv = vmm_malloc(sizeof(arm_guest_priv_t));
		if (!guest->arch_priv) {
			return VMM_EFAIL;
		}
		arm_guest_priv(guest)->ttbl = mmu_lpae_ttbl_alloc(TTBL_STAGE2);
	}
	return VMM_OK;
}

int arch_guest_deinit(struct vmm_guest *guest)
{
	int rc;

	if (guest->arch_priv) {
		if ((rc = mmu_lpae_ttbl_free(arm_guest_priv(guest)->ttbl))) {
			return rc;
		}
		vmm_free(guest->arch_priv);
	}

	return VMM_OK;
}

int arch_vcpu_init(struct vmm_vcpu *vcpu)
{
	u32 cpuid = 0;
	const char *attr;
	irq_flags_t flags;

	/* For both Orphan & Normal VCPUs */
	memset(arm_regs(vcpu), 0, sizeof(arch_regs_t));
	arm_regs(vcpu)->pc = vcpu->start_pc;
	arm_regs(vcpu)->sp = vcpu->stack_va + vcpu->stack_sz - 8;
	if (!vcpu->is_normal) {
		arm_regs(vcpu)->pstate = PSR_MODE64_EL2h;
		arm_regs(vcpu)->pstate |= PSR_ASYNC_ABORT_DISABLED;
		return VMM_OK;
	}

	/* Following initialization for normal VCPUs only */
	attr = vmm_devtree_attrval(vcpu->node, 
				   VMM_DEVTREE_COMPATIBLE_ATTR_NAME);
	if (strcmp(attr, "armv7a,cortex-a8") == 0) {
		cpuid = ARM_CPUID_CORTEXA8;
		arm_regs(vcpu)->pstate = PSR_MODE32;
	} else if (strcmp(attr, "armv7a,cortex-a9") == 0) {
		cpuid = ARM_CPUID_CORTEXA9;
		arm_regs(vcpu)->pstate = PSR_MODE32;
	} else if (strcmp(attr, "armv7a,cortex-a15") == 0) {
		cpuid = ARM_CPUID_CORTEXA15;
		arm_regs(vcpu)->pstate = PSR_MODE32;
	} else if (strcmp(attr, "armv8,generic") == 0) {
		cpuid = ARM_CPUID_ARMV8;
	} else {
		return VMM_EFAIL;
	}
	if (arm_regs(vcpu)->pstate == PSR_MODE32) {
		/* Check if the host supports A32 mode @ EL1 */
		if (!cpu_supports_el1_a32()) {
			vmm_printf("Host does not support AArch32 mode\n");
			return VMM_EFAIL;
		}
		arm_regs(vcpu)->pstate |= PSR_ZERO_MASK;
		arm_regs(vcpu)->pstate |= PSR_MODE32_SUPERVISOR;
	} else {
		arm_regs(vcpu)->pstate |= PSR_MODE64_DEBUG_DISABLED;
		arm_regs(vcpu)->pstate |= PSR_MODE64_EL1h;
	}
	arm_regs(vcpu)->pstate |= PSR_ASYNC_ABORT_DISABLED;
	arm_regs(vcpu)->pstate |= PSR_IRQ_DISABLED;
	arm_regs(vcpu)->pstate |= PSR_FIQ_DISABLED;

	/* First time initialization of private context */
	if (!vcpu->reset_count) {
		/* Alloc private context */
		vcpu->arch_priv = vmm_zalloc(sizeof(arm_priv_t));
		if (!vcpu->arch_priv) {
			return VMM_ENOMEM;
		}
		/* Setup CPUID value expected by VCPU in MIDR register
		 * as-per HW specifications.
		 */
		arm_priv(vcpu)->cpuid = cpuid;
		/* Initialize VCPU features */
		arm_priv(vcpu)->features = 0;
		switch (cpuid) {
		case ARM_CPUID_CORTEXA8:
			arm_set_feature(vcpu, ARM_FEATURE_V7);
			arm_set_feature(vcpu, ARM_FEATURE_VFP3);
			arm_set_feature(vcpu, ARM_FEATURE_NEON);
			arm_set_feature(vcpu, ARM_FEATURE_THUMB2EE);
			arm_set_feature(vcpu, ARM_FEATURE_DUMMY_C15_REGS);
			arm_set_feature(vcpu, ARM_FEATURE_TRUSTZONE);
			break;
		case ARM_CPUID_CORTEXA9:
			arm_set_feature(vcpu, ARM_FEATURE_V7);
			arm_set_feature(vcpu, ARM_FEATURE_VFP3);
			arm_set_feature(vcpu, ARM_FEATURE_VFP_FP16);
			arm_set_feature(vcpu, ARM_FEATURE_NEON);
			arm_set_feature(vcpu, ARM_FEATURE_THUMB2EE);
			arm_set_feature(vcpu, ARM_FEATURE_V7MP);
			arm_set_feature(vcpu, ARM_FEATURE_TRUSTZONE);
			break;
		case ARM_CPUID_CORTEXA15:
			arm_set_feature(vcpu, ARM_FEATURE_V7);
			arm_set_feature(vcpu, ARM_FEATURE_VFP4);
			arm_set_feature(vcpu, ARM_FEATURE_VFP_FP16);
			arm_set_feature(vcpu, ARM_FEATURE_NEON);
			arm_set_feature(vcpu, ARM_FEATURE_THUMB2EE);
			arm_set_feature(vcpu, ARM_FEATURE_ARM_DIV);
			arm_set_feature(vcpu, ARM_FEATURE_V7MP);
			arm_set_feature(vcpu, ARM_FEATURE_GENERIC_TIMER);
			arm_set_feature(vcpu, ARM_FEATURE_DUMMY_C15_REGS);
			arm_set_feature(vcpu, ARM_FEATURE_LPAE);
			arm_set_feature(vcpu, ARM_FEATURE_TRUSTZONE);
			break;
		case ARM_CPUID_ARMV8:
			arm_set_feature(vcpu, ARM_FEATURE_V8);
			arm_set_feature(vcpu, ARM_FEATURE_VFP4);
			arm_set_feature(vcpu, ARM_FEATURE_ARM_DIV);
			arm_set_feature(vcpu, ARM_FEATURE_LPAE);
			arm_set_feature(vcpu, ARM_FEATURE_GENERIC_TIMER);
			break;
		default:
			break;
		};
		/* Some features automatically imply others: */
		if (arm_feature(vcpu, ARM_FEATURE_V7)) {
			arm_set_feature(vcpu, ARM_FEATURE_VAPA);
			arm_set_feature(vcpu, ARM_FEATURE_THUMB2);
			arm_set_feature(vcpu, ARM_FEATURE_MPIDR);
			if (!arm_feature(vcpu, ARM_FEATURE_M)) {
				arm_set_feature(vcpu, ARM_FEATURE_V6K);
			} else {
				arm_set_feature(vcpu, ARM_FEATURE_V6);
			}
		}
		if (arm_feature(vcpu, ARM_FEATURE_V6K)) {
			arm_set_feature(vcpu, ARM_FEATURE_V6);
			arm_set_feature(vcpu, ARM_FEATURE_MVFR);
		}
		if (arm_feature(vcpu, ARM_FEATURE_V6)) {
			arm_set_feature(vcpu, ARM_FEATURE_V5);
			if (!arm_feature(vcpu, ARM_FEATURE_M)) {
				arm_set_feature(vcpu, ARM_FEATURE_AUXCR);
			}
		}
		if (arm_feature(vcpu, ARM_FEATURE_V5)) {
			arm_set_feature(vcpu, ARM_FEATURE_V4T);
		}
		if (arm_feature(vcpu, ARM_FEATURE_M)) {
			arm_set_feature(vcpu, ARM_FEATURE_THUMB_DIV);
		}
		if (arm_feature(vcpu, ARM_FEATURE_ARM_DIV)) {
			arm_set_feature(vcpu, ARM_FEATURE_THUMB_DIV);
		}
		if (arm_feature(vcpu, ARM_FEATURE_VFP4)) {
			arm_set_feature(vcpu, ARM_FEATURE_VFP3);
		}
		if (arm_feature(vcpu, ARM_FEATURE_VFP3)) {
			arm_set_feature(vcpu, ARM_FEATURE_VFP);
		}
		if (arm_feature(vcpu, ARM_FEATURE_LPAE)) {
			arm_set_feature(vcpu, ARM_FEATURE_PXN);
		}
		/* Initialize Hypervisor Configuration */
		INIT_SPIN_LOCK(&arm_priv(vcpu)->hcr_lock);
		arm_priv(vcpu)->hcr =  (HCR_TACR_MASK |
					HCR_TIDCP_MASK |
					HCR_TSC_MASK |
					HCR_TWI_MASK |
					HCR_AMO_MASK |
					HCR_IMO_MASK |
					HCR_FMO_MASK |
					HCR_SWIO_MASK |
					HCR_VM_MASK);
		if (!(arm_regs(vcpu)->pstate & PSR_MODE32)) {
			arm_priv(vcpu)->hcr |= HCR_RW_MASK;
		}
		/* Initialize Coprocessor Trap Register */
		arm_priv(vcpu)->cptr = CPTR_TTA_MASK;
		if (!cpu_supports_fpu() ||
		    !arm_feature(vcpu, ARM_FEATURE_VFP)) {
			arm_priv(vcpu)->cptr |= CPTR_TFP_MASK;
		}
		/* Initialize Hypervisor System Trap Register */
		arm_priv(vcpu)->hstr = 0;
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
		/* Generic timer physical & virtual irq for the vcpu */
		attr = vmm_devtree_attrval(vcpu->node, "gentimer_phys_irq");
		arm_gentimer_context(vcpu)->phys_timer_irq = 
						(attr) ? (*(u32 *)attr) : 0;
		attr = vmm_devtree_attrval(vcpu->node, "gentimer_virt_irq");
		arm_gentimer_context(vcpu)->virt_timer_irq = 
						(attr) ? (*(u32 *)attr) : 0;
		/* Cleanup VGIC context first time */
		arm_vgic_cleanup(vcpu);
	}

	/* Clear virtual exception bits in HCR */
	vmm_spin_lock_irqsave(&arm_priv(vcpu)->hcr_lock, flags);
	arm_priv(vcpu)->hcr &= ~(HCR_VSE_MASK | 
				 HCR_VI_MASK | 
				 HCR_VF_MASK);
	vmm_spin_unlock_irqrestore(&arm_priv(vcpu)->hcr_lock, flags);

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

	/* Reset floating point control */
	arm_priv(vcpu)->fpexc32 = 0x0;
	arm_priv(vcpu)->fpcr = 0x0;
	arm_priv(vcpu)->fpsr = 0x0;

	/* Set last host CPU to invalid value */
	arm_priv(vcpu)->last_hcpu = 0xFFFFFFFF;

	/* Reset generic timer context */
	generic_timer_vcpu_context_init(arm_gentimer_context(vcpu));

	return VMM_OK;
}

int arch_vcpu_deinit(struct vmm_vcpu *vcpu)
{
	/* For both Orphan & Normal VCPUs */
	memset(arm_regs(vcpu), 0, sizeof(arch_regs_t));

	/* For Orphan VCPUs do nothing else */
	if (!vcpu->is_normal) {
		return VMM_OK;
	}

	/* Free private context */
	vmm_free(vcpu->arch_priv);
	vcpu->arch_priv = NULL;

	return VMM_OK;
}

static void cpu_vcpu_vfp_simd_save_regs(struct vmm_vcpu *vcpu)
{
	void *addr;

	/* Sanity check */
	if (!vcpu) {
		return;
	}

	/* Do nothing if:
	 * 1. Floating point hardware not available
	 * 2. VCPU does not have VFP feature
	 * 3. Floating point access is disabled
	 */
	if (!cpu_supports_fpu() ||
	    !arm_feature(vcpu, ARM_FEATURE_VFP) ||
	    (mrs(cptr_el2) & CPTR_TFP_MASK)) {
		return;
	}

	/* Save floating point registers */
	addr = &arm_priv(vcpu)->fpregs;
	asm volatile("stp	 q0,  q1, [%0, #0x00]\n\t"
		     "stp	 q2,  q3, [%0, #0x20]\n\t"
		     "stp	 q4,  q5, [%0, #0x40]\n\t"
		     "stp	 q6,  q7, [%0, #0x60]\n\t"
		     :: "r"((char *)(addr) + 0x000));
	asm volatile("stp	 q8,  q9, [%0, #0x00]\n\t"
		     "stp	q10, q11, [%0, #0x20]\n\t"
		     "stp	q12, q13, [%0, #0x40]\n\t"
		     "stp	q14, q15, [%0, #0x60]\n\t"
		     :: "r"((char *)(addr) + 0x080));
	asm volatile("stp	q16, q17, [%0, #0x00]\n\t"
		     "stp	q18, q19, [%0, #0x20]\n\t"
		     "stp	q20, q21, [%0, #0x40]\n\t"
		     "stp	q22, q23, [%0, #0x60]\n\t"
		     :: "r"((char *)(addr) + 0x100));
	asm volatile("stp	q24, q25, [%0, #0x00]\n\t"
		     "stp	q26, q27, [%0, #0x20]\n\t"
		     "stp	q28, q29, [%0, #0x40]\n\t"
		     "stp	q30, q31, [%0, #0x60]\n\t"
		     :: "r"((char *)(addr) + 0x180));
	arm_priv(vcpu)->fpsr = mrs(fpsr);
	arm_priv(vcpu)->fpcr = mrs(fpcr);

	/* Save 32bit floating point control */
	arm_priv(vcpu)->fpexc32 = mrs(fpexc32_el2);
}

static void cpu_vcpu_vfp_simd_restore_regs(struct vmm_vcpu *vcpu)
{
	void *addr;

	/* Sanity check */
	if (!vcpu) {
		return;
	}

	/* Do nothing if:
	 * 1. Floating point hardware not available
	 * 2. VCPU does not have VFP feature
	 * 3. Floating point access is disabled
	 */
	if (!cpu_supports_fpu() ||
	    !arm_feature(vcpu, ARM_FEATURE_VFP) ||
	    (mrs(cptr_el2) & CPTR_TFP_MASK)) {
		return;
	}

	/* Restore floating point registers */
	addr = &arm_priv(vcpu)->fpregs;
	asm volatile("ldp	 q0,  q1, [%0, #0x00]\n\t"
		     "ldp	 q2,  q3, [%0, #0x20]\n\t"
		     "ldp	 q4,  q5, [%0, #0x40]\n\t"
		     "ldp	 q6,  q7, [%0, #0x60]\n\t"
		     :: "r"((char *)(addr) + 0x000));
	asm volatile("ldp	 q8,  q9, [%0, #0x00]\n\t"
		     "ldp	q10, q11, [%0, #0x20]\n\t"
		     "ldp	q12, q13, [%0, #0x40]\n\t"
		     "ldp	q14, q15, [%0, #0x60]\n\t"
		     :: "r"((char *)(addr) + 0x080));
	asm volatile("ldp	q16, q17, [%0, #0x00]\n\t"
		     "ldp	q18, q19, [%0, #0x20]\n\t"
		     "ldp	q20, q21, [%0, #0x40]\n\t"
		     "ldp	q22, q23, [%0, #0x60]\n\t"
		     :: "r"((char *)(addr) + 0x100));
	asm volatile("ldp	q24, q25, [%0, #0x00]\n\t"
		     "ldp	q26, q27, [%0, #0x20]\n\t"
		     "ldp	q28, q29, [%0, #0x40]\n\t"
		     "ldp	q30, q31, [%0, #0x60]\n\t"
		     :: "r"((char *)(addr) + 0x180));
	msr(fpsr, arm_priv(vcpu)->fpsr);
	msr(fpcr, arm_priv(vcpu)->fpcr);

	/* Restore 32bit floating point control */
	msr(fpexc32_el2, arm_priv(vcpu)->fpexc32);
}

static inline void cpu_vcpu_special_regs_save(struct vmm_vcpu *vcpu)
{
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

static inline void cpu_vcpu_special_regs_restore(struct vmm_vcpu *vcpu)
{
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

void arch_vcpu_switch(struct vmm_vcpu *tvcpu, 
		      struct vmm_vcpu *vcpu, 
		      arch_regs_t *regs)
{
	u32 ite;
	irq_flags_t flags;

	/* Save user registers & banked registers */
	if (tvcpu) {
		arm_regs(tvcpu)->pc = regs->pc;
		arm_regs(tvcpu)->lr = regs->lr;
		arm_regs(tvcpu)->sp = regs->sp;
		for (ite = 0; ite < CPU_GPR_COUNT; ite++) {
			arm_regs(tvcpu)->gpr[ite] = regs->gpr[ite];
		}
		arm_regs(tvcpu)->pstate = regs->pstate;
		if (tvcpu->is_normal) {
			/* Save VGIC registers */
			arm_vgic_save(tvcpu);
			/* Save generic timer */
			if (arm_feature(tvcpu, ARM_FEATURE_GENERIC_TIMER)) {
				generic_timer_vcpu_context_save(arm_gentimer_context(tvcpu));
			}
			/* Save special registers */
			cpu_vcpu_special_regs_save(tvcpu);
			/* Save VFP and SIMD register */
			cpu_vcpu_vfp_simd_save_regs(tvcpu);
			/* Update last host CPU */
			arm_priv(tvcpu)->last_hcpu = vmm_smp_processor_id();
		}
	}
	/* Restore user registers & special registers */
	regs->pc = arm_regs(vcpu)->pc;
	regs->lr = arm_regs(vcpu)->lr;
	regs->sp = arm_regs(vcpu)->sp;
	for (ite = 0; ite < CPU_GPR_COUNT; ite++) {
		regs->gpr[ite] = arm_regs(vcpu)->gpr[ite];
	}
	regs->pstate = arm_regs(vcpu)->pstate;
	if (vcpu->is_normal) {
		/* Restore hypervisor context */
		vmm_spin_lock_irqsave(&arm_priv(vcpu)->hcr_lock, flags);
		msr(hcr_el2, arm_priv(vcpu)->hcr);
		vmm_spin_unlock_irqrestore(&arm_priv(vcpu)->hcr_lock, flags);
		msr(cptr_el2, arm_priv(vcpu)->cptr);
		msr(hstr_el2, arm_priv(vcpu)->hstr);
		/* Update Stage2 MMU context */
		mmu_lpae_stage2_chttbl(vcpu->guest->id, 
				      arm_guest_priv(vcpu->guest)->ttbl);
		/* Update VPIDR and VMPIDR */
		msr(vpidr_el2, arm_priv(vcpu)->midr);
		msr(vmpidr_el2, arm_priv(vcpu)->mpidr);
		/* Flush TLB if moved to new host CPU */
		if (arm_priv(vcpu)->last_hcpu != vmm_smp_processor_id()) {
			/* Invalidate all guest TLB enteries because
			 * we might have stale guest TLB enteries from
			 * our previous run on new_hcpu host CPU 
			 */
			inv_tlb_guest_allis();
			/* Ensure changes are visible */
			dsb();
			isb();
		}
		/* Restore VFP and SIMD register */
		cpu_vcpu_vfp_simd_restore_regs(vcpu);
		/* Restore special registers */
		cpu_vcpu_special_regs_restore(vcpu);
		/* Restore generic timer */
		if (arm_feature(vcpu, ARM_FEATURE_GENERIC_TIMER)) {
			generic_timer_vcpu_context_restore(
						arm_gentimer_context(vcpu));
		}
		/* Restore VGIC registers */
		arm_vgic_restore(vcpu);
	}
	/* Clear exclusive monitor */
	clrex();
}

void arch_vcpu_preempt_orphan(void)
{
	/* Trigger HVC call from hypervisor mode. This will cause
	 * do_soft_irq() function to call vmm_scheduler_preempt_orphan()
	 */
	asm volatile ("hvc #0\t\n");
}

static void __cpu_vcpu_dump_user_reg(struct vmm_chardev *cdev, 
				     arch_regs_t *regs)
{
	u32 ite;
	vmm_cprintf(cdev, "  Core Registers\n");
	vmm_cprintf(cdev, "    SP=0x%016lX       LR=0x%016lX\n",
		    regs->sp, regs->lr);
	vmm_cprintf(cdev, "    PC=0x%016lX       PSTATE=0x%08lX\n",
		    regs->pc, (regs->pstate & 0xffffffff));
	vmm_cprintf(cdev, "  General Purpose Registers");
	for (ite = 0; ite < (CPU_GPR_COUNT); ite++) {
		if (ite % 2 == 0)
			vmm_cprintf(cdev, "\n");
		vmm_cprintf(cdev, "    X%02d=0x%016lX  ", ite, regs->gpr[ite]);
	}
	vmm_cprintf(cdev, "\n");
}

void cpu_vcpu_dump_user_reg(arch_regs_t *regs)
{
	__cpu_vcpu_dump_user_reg(NULL, regs);
}

void arch_vcpu_regs_dump(struct vmm_chardev *cdev, struct vmm_vcpu *vcpu)
{
	/* For both Normal & Orphan VCPUs */
	__cpu_vcpu_dump_user_reg(cdev, arm_regs(vcpu));
	/* For only Normal VCPUs */
	if (!vcpu->is_normal) {
		return;
	}
	vmm_cprintf(cdev, "       TTBR_EL2: 0x%016lX\n", 
		    arm_guest_priv(vcpu->guest)->ttbl->tbl_pa);
	vmm_cprintf(cdev, "        HCR_EL2: 0x%016lX\n", 
		    arm_priv(vcpu)->hcr);
	vmm_cprintf(cdev, "       CPTR_EL2: 0x%016lX\n", 
		    arm_priv(vcpu)->cptr);
	vmm_cprintf(cdev, "       HSTR_EL2: 0x%016lX\n", 
		    arm_priv(vcpu)->hstr);
	vmm_cprintf(cdev, "         SP_EL0: 0x%016lX\n", 
		    arm_priv(vcpu)->sp_el0);
	vmm_cprintf(cdev, "         SP_EL1: 0x%016lX\n", 
		    arm_priv(vcpu)->sp_el1);
	vmm_cprintf(cdev, "        ELR_EL1: 0x%016lX\n", 
		    arm_priv(vcpu)->elr_el1);
	vmm_cprintf(cdev, "       SPSR_EL1: 0x%016lX\n", 
		    arm_priv(vcpu)->spsr_el1);
	vmm_cprintf(cdev, "       SPSR_ABT: 0x%08lX\n", 
		    arm_priv(vcpu)->spsr_abt);
	vmm_cprintf(cdev, "       SPSR_UND: 0x%08lX\n", 
		    arm_priv(vcpu)->spsr_und);
	vmm_cprintf(cdev, "       SPSR_IRQ: 0x%08lX\n", 
		    arm_priv(vcpu)->spsr_irq);
	vmm_cprintf(cdev, "       SPSR_FIQ: 0x%08lX\n", 
		    arm_priv(vcpu)->spsr_fiq);
	vmm_cprintf(cdev, "       MIDR_EL1: 0x%016lX\n", 
		    arm_priv(vcpu)->midr);
	vmm_cprintf(cdev, "      MPIDR_EL1: 0x%016lX\n", 
		    arm_priv(vcpu)->mpidr);
	vmm_cprintf(cdev, "      SCTLR_EL1: 0x%016lX\n", 
		    arm_priv(vcpu)->sctlr);
	vmm_cprintf(cdev, "      CPACR_EL1: 0x%016lX\n", 
		    arm_priv(vcpu)->cpacr);
	vmm_cprintf(cdev, "      TTBR0_EL1: 0x%016lX\n", 
		    arm_priv(vcpu)->ttbr0);
	vmm_cprintf(cdev, "      TTBR1_EL1: 0x%016lX\n", 
		    arm_priv(vcpu)->ttbr1);
	vmm_cprintf(cdev, "        TCR_EL1: 0x%016lX\n", 
		    arm_priv(vcpu)->tcr);
	vmm_cprintf(cdev, "        ESR_EL1: 0x%016lX\n", 
		    arm_priv(vcpu)->esr);
	vmm_cprintf(cdev, "        FAR_EL1: 0x%016lX\n", 
		    arm_priv(vcpu)->far);
	vmm_cprintf(cdev, "        PAR_EL1: 0x%016lX\n", 
		    arm_priv(vcpu)->par);
	vmm_cprintf(cdev, "       MAIR_EL1: 0x%016lX\n", 
		    arm_priv(vcpu)->mair);
	vmm_cprintf(cdev, "       VBAR_EL1: 0x%016lX\n", 
		    arm_priv(vcpu)->vbar);
	vmm_cprintf(cdev, " CONTEXTIDR_EL1: 0x%016lX\n", 
		    arm_priv(vcpu)->contextidr);
	vmm_cprintf(cdev, "      TPIDR_EL0: 0x%016lX\n", 
		    arm_priv(vcpu)->tpidr_el0);
	vmm_cprintf(cdev, "      TPIDR_EL1: 0x%016lX\n", 
		    arm_priv(vcpu)->tpidr_el1);
	vmm_cprintf(cdev, "        TPIDRRO: 0x%016lX\n", 
		    arm_priv(vcpu)->tpidrro);
	vmm_cprintf(cdev, "\n");
}

void arch_vcpu_stat_dump(struct vmm_chardev *cdev, struct vmm_vcpu *vcpu)
{
	/* For now no arch specific stats */
}
