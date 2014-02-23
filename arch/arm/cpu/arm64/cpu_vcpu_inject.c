/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file cpu_vcpu_inject.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Source file for VCPU exception injection
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <vmm_host_aspace.h>
#include <vmm_guest_aspace.h>
#include <libs/stringlib.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_helper.h>
#include <cpu_vcpu_inject.h>

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
	cpu_vcpu_spsr32_update(vcpu, CPSR_MODE_UNDEFINED, old_cpsr);
	cpu_vcpu_regmode32_write(regs, CPSR_MODE_UNDEFINED, 14, 
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
	cpu_vcpu_spsr32_update(vcpu, CPSR_MODE_ABORT, old_cpsr);
	cpu_vcpu_regmode32_write(regs, CPSR_MODE_ABORT, 14, 
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

