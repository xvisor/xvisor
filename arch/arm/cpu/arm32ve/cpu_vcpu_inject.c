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
 * @brief Source file of VCPU exception injection
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_helper.h>
#include <cpu_vcpu_inject.h>

int cpu_vcpu_inject_undef(struct vmm_vcpu *vcpu,
			  arch_regs_t *regs)
{
	u32 old_cpsr, new_cpsr, sctlr;

	/* Sanity checks */
	if (!vcpu || !regs) {
		return VMM_EFAIL;
	}
	if (vcpu != vmm_scheduler_current_vcpu()) {
		/* This function should only be called for current VCPU */
		vmm_panic("%s not called for current vcpu\n", __func__);
	}

	/* Retrive current SCTLR */
	sctlr = read_sctlr();

	/* Compute CPSR changes */
	old_cpsr = new_cpsr = regs->cpsr;
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
	cpu_vcpu_spsr_update(vcpu, CPSR_MODE_UNDEFINED, old_cpsr);
	cpu_vcpu_regmode_write(vcpu, regs, CPSR_MODE_UNDEFINED, 14, 
		regs->pc - ((old_cpsr & CPSR_THUMB_ENABLED) ? 2 : 4));
	if (sctlr & SCTLR_V_MASK) {
		regs->pc = CPU_IRQ_HIGHVEC_BASE;
	} else {
		regs->pc = read_vbar();
	}
	regs->pc += 4;
	regs->cpsr = new_cpsr;

	return VMM_OK;
}

static int __cpu_vcpu_inject_abt(struct vmm_vcpu *vcpu,
				 arch_regs_t *regs,
				 bool is_pabt,
				 virtual_addr_t addr)
{
	u32 old_cpsr, new_cpsr, sctlr, ttbcr;

	/* Sanity checks */
	if (!vcpu || !regs) {
		return VMM_EFAIL;
	}
	if (vcpu != vmm_scheduler_current_vcpu()) {
		/* This function should only be called for current VCPU */
		vmm_panic("%s not called for current vcpu\n", __func__);
	}

	/* Retrive current SCTLR */
	sctlr = read_sctlr();

	/* Compute CPSR changes */
	old_cpsr = new_cpsr = regs->cpsr;
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
	cpu_vcpu_spsr_update(vcpu, CPSR_MODE_ABORT, old_cpsr);
	cpu_vcpu_regmode_write(vcpu, regs, CPSR_MODE_ABORT, 14, 
		regs->pc - ((old_cpsr & CPSR_THUMB_ENABLED) ? 4 : 0));
	if (sctlr & SCTLR_V_MASK) {
		regs->pc = CPU_IRQ_HIGHVEC_BASE;
	} else {
		regs->pc = read_vbar();
	}
	regs->pc += (is_pabt) ? 12 : 16;
	regs->cpsr = new_cpsr;

	/* Update abort registers */
	ttbcr = read_ttbcr();
	if (is_pabt) {
		/* Set IFAR and IFSR */
		write_ifar(addr);
		if (ttbcr >> 31) { /* LPAE MMU */
			write_ifsr((1 << 9) | 0x22);
		} else { /* Legacy ARMv6 MMU */
			write_ifsr(0x2);
		}
	} else {
		/* Set DFAR and DFSR */
		write_dfar(addr);
		if (ttbcr >> 31) { /* LPAE MMU */
			write_dfsr((1 << 9) | 0x22);
		} else { /* Legacy ARMv6 MMU */
			write_dfsr(0x2);
		}
	}

	return VMM_OK;
}

int cpu_vcpu_inject_pabt(struct vmm_vcpu *vcpu,
			 arch_regs_t *regs)
{
	return __cpu_vcpu_inject_abt(vcpu, regs, TRUE, regs->pc);
}

int cpu_vcpu_inject_dabt(struct vmm_vcpu *vcpu,
			 arch_regs_t *regs,
			 virtual_addr_t addr)
{
	return __cpu_vcpu_inject_abt(vcpu, regs, FALSE, addr);
}

