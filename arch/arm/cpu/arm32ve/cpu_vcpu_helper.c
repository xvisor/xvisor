/**
 * Copyright (c) 2012 Anup Patel.
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
 * @author Anup Patel (anup@brainfault.org)
 * @brief source of VCPU helper functions
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_manager.h>
#include <vmm_scheduler.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <arch_barrier.h>
#include <cpu_defines.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_cp15.h>
#include <cpu_vcpu_helper.h>
#include <generic_timer.h>
#include <arm_features.h>
#include <mmu_lpae.h>

void cpu_vcpu_halt(struct vmm_vcpu *vcpu, arch_regs_t *regs)
{
	if (vcpu->state != VMM_VCPU_STATE_HALTED) {
		vmm_printf("\n");
		cpu_vcpu_dump_user_reg(regs);
		vmm_manager_vcpu_halt(vcpu);
	}
}

u32 cpu_vcpu_regmode_read(struct vmm_vcpu *vcpu, 
			  arch_regs_t *regs, 
			  u32 mode,
			  u32 reg_num)
{
	u32 hwreg;
	switch (reg_num) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
		return regs->gpr[reg_num];
	case 8:
		if (mode == CPSR_MODE_FIQ) {
			asm volatile (" mrs     %0, r8_fiq\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->gpr_fiq[reg_num - 8] = hwreg;
			return hwreg;
		} else {
			return regs->gpr[reg_num];
		}
	case 9:
		if (mode == CPSR_MODE_FIQ) {
			asm volatile (" mrs     %0, r9_fiq\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->gpr_fiq[reg_num - 8] = hwreg;
			return hwreg;
		} else {
			return regs->gpr[reg_num];
		}
	case 10:
		if (mode == CPSR_MODE_FIQ) {
			asm volatile (" mrs     %0, r10_fiq\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->gpr_fiq[reg_num - 8] = hwreg;
			return hwreg;
		} else {
			return regs->gpr[reg_num];
		}
	case 11:
		if (mode == CPSR_MODE_FIQ) {
			asm volatile (" mrs     %0, r11_fiq\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->gpr_fiq[reg_num - 8] = hwreg;
			return hwreg;
		} else {
			return regs->gpr[reg_num];
		}
	case 12:
		if (mode == CPSR_MODE_FIQ) {
			asm volatile (" mrs     %0, r12_fiq\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->gpr_fiq[reg_num - 8] = hwreg;
			return hwreg;
		} else {
			return regs->gpr[reg_num];
		}
	case 13:
		switch (mode) {
		case CPSR_MODE_USER:
		case CPSR_MODE_SYSTEM:
			asm volatile (" mrs     %0, SP_usr\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->sp_usr = hwreg;
			return hwreg;
		case CPSR_MODE_FIQ:
			asm volatile (" mrs     %0, SP_fiq\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->sp_fiq = hwreg;
			return hwreg;
		case CPSR_MODE_IRQ:
			asm volatile (" mrs     %0, SP_irq\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->sp_irq = hwreg;
			return hwreg;
		case CPSR_MODE_SUPERVISOR:
			asm volatile (" mrs     %0, SP_svc\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->sp_svc = hwreg;
			return hwreg;
		case CPSR_MODE_ABORT:
			asm volatile (" mrs     %0, SP_abt\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->sp_abt = hwreg;
			return hwreg;
		case CPSR_MODE_UNDEFINED:
			asm volatile (" mrs     %0, SP_und\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->sp_und = hwreg;
			return hwreg;
		default:
			break;
		};
		break;
	case 14:
		switch (mode) {
		case CPSR_MODE_USER:
		case CPSR_MODE_SYSTEM:
			return regs->lr;
		case CPSR_MODE_FIQ:
			asm volatile (" mrs     %0, LR_fiq\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->lr_fiq = hwreg;
			return hwreg;
		case CPSR_MODE_IRQ:
			asm volatile (" mrs     %0, LR_irq\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->lr_irq = hwreg;
			return hwreg;
		case CPSR_MODE_SUPERVISOR:
			asm volatile (" mrs     %0, LR_svc\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->lr_svc = hwreg;
			return hwreg;
		case CPSR_MODE_ABORT:
			asm volatile (" mrs     %0, LR_abt\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->lr_abt = hwreg;
			return hwreg;
		case CPSR_MODE_UNDEFINED:
			asm volatile (" mrs     %0, LR_und\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->lr_und = hwreg;
			return hwreg;
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

void cpu_vcpu_regmode_write(struct vmm_vcpu *vcpu, 
			    arch_regs_t *regs, 
			    u32 mode,
			    u32 reg_num,
			    u32 reg_val)
{
	switch (reg_num) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
		regs->gpr[reg_num] = reg_val;
		break;
	case 8:
		if (mode == CPSR_MODE_FIQ) {
			asm volatile (" msr     r8_fiq, %0\n\t"
				      ::"r" (reg_val) :"memory", "cc");
			arm_priv(vcpu)->gpr_fiq[reg_num - 8] = reg_val;
		} else {
			regs->gpr[reg_num] = reg_val;
		}
		break;
	case 9:
		if (mode == CPSR_MODE_FIQ) {
			asm volatile (" msr     r9_fiq, %0\n\t"
				      ::"r" (reg_val) :"memory", "cc");
			arm_priv(vcpu)->gpr_fiq[reg_num - 8] = reg_val;
		} else {
			regs->gpr[reg_num] = reg_val;
		}
		break;
	case 10:
		if (mode == CPSR_MODE_FIQ) {
			asm volatile (" msr     r10_fiq, %0\n\t"
				      ::"r" (reg_val) :"memory", "cc");
			arm_priv(vcpu)->gpr_fiq[reg_num - 8] = reg_val;
		} else {
			regs->gpr[reg_num] = reg_val;
		}
		break;
	case 11:
		if (mode == CPSR_MODE_FIQ) {
			asm volatile (" msr     r11_fiq, %0\n\t"
				      ::"r" (reg_val) :"memory", "cc");
			arm_priv(vcpu)->gpr_fiq[reg_num - 8] = reg_val;
		} else {
			regs->gpr[reg_num] = reg_val;
		}
		break;
	case 12:
		if (mode == CPSR_MODE_FIQ) {
			asm volatile (" msr     r12_fiq, %0\n\t"
				      ::"r" (reg_val) :"memory", "cc");
			arm_priv(vcpu)->gpr_fiq[reg_num - 8] = reg_val;
		} else {
			regs->gpr[reg_num] = reg_val;
		}
		break;
	case 13:
		switch (mode) {
		case CPSR_MODE_USER:
		case CPSR_MODE_SYSTEM:
			asm volatile (" msr     SP_usr, %0\n\t"
				      ::"r" (reg_val) :"memory", "cc");
			arm_priv(vcpu)->sp_usr = reg_val;
			break;
		case CPSR_MODE_FIQ:
			asm volatile (" msr     SP_fiq, %0\n\t"
				      ::"r" (reg_val) :"memory", "cc"); 
			arm_priv(vcpu)->sp_fiq = reg_val;
			break;
		case CPSR_MODE_IRQ:
			asm volatile (" msr     SP_irq, %0\n\t"
				      ::"r" (reg_val) :"memory", "cc");
			arm_priv(vcpu)->sp_irq = reg_val;
			break;
		case CPSR_MODE_SUPERVISOR:
			asm volatile (" msr     SP_svc, %0\n\t"
				      ::"r" (reg_val) :"memory", "cc");
			arm_priv(vcpu)->sp_svc = reg_val;
			break;
		case CPSR_MODE_ABORT:
			asm volatile (" msr     SP_abt, %0\n\t"
				      ::"r" (reg_val) :"memory", "cc");
			arm_priv(vcpu)->sp_abt = reg_val;
			break;
		case CPSR_MODE_UNDEFINED:
			asm volatile (" msr     SP_und, %0\n\t"
				      ::"r" (reg_val) :"memory", "cc");
			arm_priv(vcpu)->sp_und = reg_val;
			break;
		default:
			break;
		};
		break;
	case 14:
		switch (mode) {
		case CPSR_MODE_USER:
		case CPSR_MODE_SYSTEM:
			regs->lr = reg_val;
			break;
		case CPSR_MODE_FIQ:
			asm volatile (" msr     LR_fiq, %0\n\t"
				      ::"r" (reg_val) :"memory", "cc");
			arm_priv(vcpu)->lr_fiq = reg_val;
			break;
		case CPSR_MODE_IRQ:
			asm volatile (" msr     LR_irq, %0\n\t"
				      ::"r" (reg_val) :"memory", "cc");
			arm_priv(vcpu)->lr_irq = reg_val;
			break;
		case CPSR_MODE_SUPERVISOR:
			asm volatile (" msr     LR_svc, %0\n\t"
				      ::"r" (reg_val) :"memory", "cc");
			arm_priv(vcpu)->lr_svc = reg_val;
			break;
		case CPSR_MODE_ABORT:
			asm volatile (" msr     LR_abt, %0\n\t"
				      ::"r" (reg_val) :"memory", "cc");
			arm_priv(vcpu)->lr_abt = reg_val;
			break;
		case CPSR_MODE_UNDEFINED:
			asm volatile (" msr     LR_und, %0\n\t"
				      ::"r" (reg_val) :"memory", "cc");
			arm_priv(vcpu)->lr_und = reg_val;
			break;
		default:
			break;
		};
		break;
	case 15:
		regs->pc = reg_val;
		break;
	default:
		break;
	};
}

u32 cpu_vcpu_reg_read(struct vmm_vcpu *vcpu, 
		      arch_regs_t *regs, 
		      u32 reg_num) 
{
	return cpu_vcpu_regmode_read(vcpu, 
				     regs, 
				     regs->cpsr & CPSR_MODE_MASK, 
				     reg_num);
}

void cpu_vcpu_reg_write(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs, 
			u32 reg_num, 
			u32 reg_val) 
{
	cpu_vcpu_regmode_write(vcpu, 
			       regs, 
			       regs->cpsr & CPSR_MODE_MASK, 
			       reg_num, 
			       reg_val);
}

u32 cpu_vcpu_spsr_retrieve(struct vmm_vcpu *vcpu, u32 mode)
{
	u32 hwreg;
	if (vcpu != vmm_scheduler_current_vcpu()) {
		/* This function should only be called for current VCPU */
		vmm_panic("%d not called for current vcpu\n", __func__);
	}
	/* Find out correct SPSR */
	switch (mode) {
	case CPSR_MODE_ABORT:
		asm volatile (" mrs     %0, SPSR_abt\n\t" 
			      :"=r" (hwreg)::"memory", "cc");
		arm_priv(vcpu)->spsr_abt = hwreg;
		return hwreg;
	case CPSR_MODE_UNDEFINED:
		asm volatile (" mrs     %0, SPSR_und\n\t" 
			      :"=r" (hwreg)::"memory", "cc");
		arm_priv(vcpu)->spsr_und = hwreg;
		return hwreg;
	case CPSR_MODE_SUPERVISOR:
		asm volatile (" mrs     %0, SPSR_svc\n\t" 
			      :"=r" (hwreg)::"memory", "cc");
		arm_priv(vcpu)->spsr_svc = hwreg;
		return hwreg;
	case CPSR_MODE_IRQ:
		asm volatile (" mrs     %0, SPSR_irq\n\t" 
			      :"=r" (hwreg)::"memory", "cc");
		arm_priv(vcpu)->spsr_irq = hwreg;
		return hwreg;
	case CPSR_MODE_FIQ:
		asm volatile (" mrs     %0, SPSR_fiq\n\t" 
			      :"=r" (hwreg)::"memory", "cc");
		arm_priv(vcpu)->spsr_fiq = hwreg;
		return hwreg;
	default:
		break;
	};
	return 0;
}

int cpu_vcpu_spsr_update(struct vmm_vcpu *vcpu, 
			 u32 mode,
			 u32 new_spsr)
{
	/* Sanity check */
	if (!vcpu || !vcpu->is_normal) {
		return VMM_EFAIL;
	}
	if (vcpu != vmm_scheduler_current_vcpu()) {
		/* This function should only be called for current VCPU */
		vmm_panic("%d not called for current vcpu\n", __func__);
	}
	/* Update appropriate SPSR */
	switch (mode) {
	case CPSR_MODE_ABORT:
		asm volatile (" msr     SPSR_abt, %0\n\t"
			      ::"r" (new_spsr) :"memory", "cc");
		arm_priv(vcpu)->spsr_abt = new_spsr;
		break;
	case CPSR_MODE_UNDEFINED:
		asm volatile (" msr     SPSR_und, %0\n\t"
			      ::"r" (new_spsr) :"memory", "cc");
		arm_priv(vcpu)->spsr_und = new_spsr;
		break;
	case CPSR_MODE_SUPERVISOR:
		asm volatile (" msr     SPSR_svc, %0\n\t"
			      ::"r" (new_spsr) :"memory", "cc");
		arm_priv(vcpu)->spsr_svc = new_spsr;
		break;
	case CPSR_MODE_IRQ:
		asm volatile (" msr     SPSR_irq, %0\n\t"
			      ::"r" (new_spsr) :"memory", "cc");
		arm_priv(vcpu)->spsr_irq = new_spsr;
		break;
	case CPSR_MODE_FIQ:
		asm volatile (" msr     SPSR_fiq, %0\n\t"
			      ::"r" (new_spsr) :"memory", "cc");
		arm_priv(vcpu)->spsr_fiq = new_spsr;
		break;
	default:
		break;
	};
	/* Return success */
	return VMM_OK;
}

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
		vmm_panic("%d not called for current vcpu\n", __func__);
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
	asm volatile (" msr     SPSR_und, %0\n\t"
		      ::"r" (old_cpsr) :"memory", "cc");
	arm_priv(vcpu)->spsr_und = old_cpsr;
	regs->lr = regs->pc - ((old_cpsr & CPSR_THUMB_ENABLED) ? 2 : 4);
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
		vmm_panic("%d not called for current vcpu\n", __func__);
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
	asm volatile (" msr     SPSR_abt, %0\n\t"
		      ::"r" (old_cpsr) :"memory", "cc");
	arm_priv(vcpu)->spsr_und = old_cpsr;
	regs->lr = regs->pc - ((old_cpsr & CPSR_THUMB_ENABLED) ? 4 : 0);
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

int arch_guest_init(struct vmm_guest *guest)
{
	if (!guest->reset_count) {
		guest->arch_priv = vmm_malloc(sizeof(arm_guest_priv_t));
		if (!guest->arch_priv) {
			return VMM_ENOMEM;
		}

		arm_guest_priv(guest)->ttbl = mmu_lpae_ttbl_alloc(TTBL_STAGE2);
		if (!arm_guest_priv(guest)->ttbl) {
			vmm_free(guest->arch_priv);
			guest->arch_priv = NULL;
			return VMM_ENOMEM;
		}
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
	u32 ite, cpuid = 0;
	arm_priv_t *p;
	const char *attr;
	irq_flags_t flags;
	/* Initialize User Mode Registers */
	/* For both Orphan & Normal VCPUs */
	memset(arm_regs(vcpu), 0, sizeof(arch_regs_t));
	arm_regs(vcpu)->pc = vcpu->start_pc;
	arm_regs(vcpu)->sp = vcpu->stack_va + vcpu->stack_sz - 4;
	if (vcpu->is_normal) {
		arm_regs(vcpu)->cpsr  = CPSR_ZERO_MASK;
		arm_regs(vcpu)->cpsr |= CPSR_ASYNC_ABORT_DISABLED;
		arm_regs(vcpu)->cpsr |= CPSR_IRQ_DISABLED;
		arm_regs(vcpu)->cpsr |= CPSR_FIQ_DISABLED;
		arm_regs(vcpu)->cpsr |= CPSR_MODE_SUPERVISOR;
	} else {
		arm_regs(vcpu)->cpsr  = CPSR_ZERO_MASK;
		arm_regs(vcpu)->cpsr |= CPSR_ASYNC_ABORT_DISABLED;
		arm_regs(vcpu)->cpsr |= CPSR_MODE_HYPERVISOR;
	}
	/* Initialize Supervisor Mode Registers */
	/* For only Normal VCPUs */
	if (!vcpu->is_normal) {
		return VMM_OK;
	}
	attr = vmm_devtree_attrval(vcpu->node, 
				   VMM_DEVTREE_COMPATIBLE_ATTR_NAME);
	if (!attr) {
		return VMM_EFAIL;
	}

	if (strcmp(attr, "armv7a,cortex-a8") == 0) {
		cpuid = ARM_CPUID_CORTEXA8;
	} else if (strcmp(attr, "armv7a,cortex-a9") == 0) {
		cpuid = ARM_CPUID_CORTEXA9;
	} else if (strcmp(attr, "armv7a,cortex-a15") == 0) {
		cpuid = ARM_CPUID_CORTEXA15;
	} else {
		return VMM_EFAIL;
	}
	if (!vcpu->reset_count) {
		vcpu->arch_priv = vmm_zalloc(sizeof(arm_priv_t));
		if (!vcpu->arch_priv) {
			return VMM_EFAIL;
		}
		p = arm_priv(vcpu);
	} else {
		p = arm_priv(vcpu);
		for (ite = 0; ite < CPU_FIQ_GPR_COUNT; ite++) {
			p->gpr_fiq[ite] = 0x0;
		}
		p->sp_usr = 0x0;
		p->sp_svc = 0x0;
		p->lr_svc = 0x0;
		p->spsr_svc = 0x0;
		p->sp_abt = 0x0;
		p->lr_abt = 0x0;
		p->spsr_abt = 0x0;
		p->sp_und = 0x0;
		p->lr_und = 0x0;
		p->spsr_und = 0x0;
		p->sp_irq = 0x0;
		p->lr_irq = 0x0;
		p->spsr_irq = 0x0;
		p->sp_fiq = 0x0;
		p->lr_fiq = 0x0;
		p->spsr_fiq = 0x0;
	}
	p->last_hcpu = 0xFFFFFFFF;
	if (vcpu->reset_count == 0) {
		/* Initialize Hypervisor Configuration */
		INIT_SPIN_LOCK(&p->hcr_lock);
		p->hcr = (HCR_TAC_MASK |
				HCR_TSW_MASK |
				HCR_TIDCP_MASK |
				HCR_TSC_MASK |
				HCR_TWI_MASK |
				HCR_AMO_MASK |
				HCR_IMO_MASK |
				HCR_FMO_MASK |
				HCR_SWIO_MASK |
				HCR_VM_MASK);
		/* Initialize Hypervisor Coprocessor Trap Register */
		p->hcptr = (HCPTR_TCPAC_MASK |
				 HCPTR_TTA_MASK |
				 HCPTR_TASE_MASK |
				 HCPTR_TCP_MASK);
		/* Initialize Hypervisor System Trap Register */
		p->hstr = (HSTR_TJDBX_MASK |
				HSTR_TTEE_MASK |
				HSTR_T9_MASK |
				HSTR_T15_MASK);
		/* Initialize VCPU features */
		p->features = 0;
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
	} else {
		/* Clear virtual exception bits in HCR */
		vmm_spin_lock_irqsave(&p->hcr_lock, flags);
		p->hcr &= ~(HCR_VA_MASK | HCR_VI_MASK | HCR_VF_MASK);
		vmm_spin_unlock_irqrestore(&p->hcr_lock, flags);
	}
	/* Intialize Generic timer */
	if (arm_feature(vcpu, ARM_FEATURE_GENERIC_TIMER)) {
		attr = vmm_devtree_attrval(vcpu->node, "gentimer_phys_irq");
		arm_gentimer_context(vcpu)->phys_timer_irq = (attr) ? (*(u32 *)attr) : 0;
		attr = vmm_devtree_attrval(vcpu->node, "gentimer_virt_irq");
		arm_gentimer_context(vcpu)->virt_timer_irq = (attr) ? (*(u32 *)attr) : 0;
		generic_timer_vcpu_context_init(arm_gentimer_context(vcpu));
	}
	if (!vcpu->reset_count) {
		/* Cleanup VGIC context first time */
		arm_vgic_cleanup(vcpu);
	}
	return cpu_vcpu_cp15_init(vcpu, cpuid);
}

int arch_vcpu_deinit(struct vmm_vcpu *vcpu)
{
	int rc;

	/* For both Orphan & Normal VCPUs */
	memset(arm_regs(vcpu), 0, sizeof(arch_regs_t));

	/* For Orphan VCPUs do nothing else */
	if (!vcpu->is_normal) {
		return VMM_OK;
	}

	/* Cleanup CP15 */
	if ((rc = cpu_vcpu_cp15_deinit(vcpu))) {
		return rc;
	}

	/* Free super regs */
	vmm_free(vcpu->arch_priv);

	return VMM_OK;
}

static void cpu_vcpu_banked_regs_save(struct vmm_vcpu *vcpu)
{
	arm_priv_t *p = arm_priv(vcpu);

	asm volatile (" mrs     %0, SP_usr\n\t" 
		      :"=r" (p->sp_usr)::"memory", "cc");
	asm volatile (" mrs     %0, SP_svc\n\t" 
		      :"=r" (p->sp_svc)::"memory", "cc");
	asm volatile (" mrs     %0, LR_svc\n\t" 
		      :"=r" (p->lr_svc)::"memory", "cc");
	asm volatile (" mrs     %0, SPSR_svc\n\t" 
		      :"=r" (p->spsr_svc)::"memory", "cc");
	asm volatile (" mrs     %0, SP_abt\n\t" 
		      :"=r" (p->sp_abt)::"memory", "cc");
	asm volatile (" mrs     %0, LR_abt\n\t" 
		      :"=r" (p->lr_abt)::"memory", "cc");
	asm volatile (" mrs     %0, SPSR_abt\n\t" 
		      :"=r" (p->spsr_abt)::"memory", "cc");
	asm volatile (" mrs     %0, SP_und\n\t" 
		      :"=r" (p->sp_und)::"memory", "cc");
	asm volatile (" mrs     %0, LR_und\n\t" 
		      :"=r" (p->lr_und)::"memory", "cc");
	asm volatile (" mrs     %0, SPSR_und\n\t" 
		      :"=r" (p->spsr_und)::"memory", "cc");
	asm volatile (" mrs     %0, SP_irq\n\t" 
		      :"=r" (p->sp_irq)::"memory", "cc");
	asm volatile (" mrs     %0, LR_irq\n\t" 
		      :"=r" (p->lr_irq)::"memory", "cc");
	asm volatile (" mrs     %0, SPSR_irq\n\t" 
		      :"=r" (p->spsr_irq)::"memory", "cc");
	asm volatile (" mrs     %0, r8_fiq\n\t" 
		      :"=r" (p->gpr_fiq[0])::"memory", "cc");
	asm volatile (" mrs     %0, r9_fiq\n\t" 
		      :"=r" (p->gpr_fiq[1])::"memory", "cc");
	asm volatile (" mrs     %0, r10_fiq\n\t" 
		      :"=r" (p->gpr_fiq[2])::"memory", "cc");
	asm volatile (" mrs     %0, r11_fiq\n\t" 
		      :"=r" (p->gpr_fiq[3])::"memory", "cc");
	asm volatile (" mrs     %0, r12_fiq\n\t" 
		      :"=r" (p->gpr_fiq[4])::"memory", "cc");
	asm volatile (" mrs     %0, SP_fiq\n\t" 
		      :"=r" (p->sp_fiq)::"memory", "cc");
	asm volatile (" mrs     %0, LR_fiq\n\t" 
		      :"=r" (p->lr_fiq)::"memory", "cc");
	asm volatile (" mrs     %0, SPSR_fiq\n\t" 
		      :"=r" (p->spsr_fiq)::"memory", "cc");
}

static void cpu_vcpu_banked_regs_restore(struct vmm_vcpu *vcpu)
{
	arm_priv_t *p = arm_priv(vcpu);

	asm volatile (" msr     SP_usr, %0\n\t"
		      ::"r" (p->sp_usr) :"memory", "cc");
	asm volatile (" msr     SP_svc, %0\n\t"
		      ::"r" (p->sp_svc) :"memory", "cc");
	asm volatile (" msr     LR_svc, %0\n\t"
		      ::"r" (p->lr_svc) :"memory", "cc");
	asm volatile (" msr     SPSR_svc, %0\n\t"
		      ::"r" (p->spsr_svc) :"memory", "cc");
	asm volatile (" msr     SP_abt, %0\n\t"
		      ::"r" (p->sp_abt) :"memory", "cc");
	asm volatile (" msr     LR_abt, %0\n\t"
		      ::"r" (p->lr_abt) :"memory", "cc");
	asm volatile (" msr     SPSR_abt, %0\n\t"
		      ::"r" (p->spsr_abt) :"memory", "cc");
	asm volatile (" msr     SP_und, %0\n\t"
		      ::"r" (p->sp_und) :"memory", "cc");
	asm volatile (" msr     LR_und, %0\n\t"
		      ::"r" (p->lr_und) :"memory", "cc");
	asm volatile (" msr     SPSR_und, %0\n\t"
		      ::"r" (p->spsr_und) :"memory", "cc");
	asm volatile (" msr     SP_irq, %0\n\t"
		      ::"r" (p->sp_irq) :"memory", "cc");
	asm volatile (" msr     LR_irq, %0\n\t"
		      ::"r" (p->lr_irq) :"memory", "cc");
	asm volatile (" msr     SPSR_irq, %0\n\t"
		      ::"r" (p->spsr_irq) :"memory", "cc");
	asm volatile (" msr     r8_fiq, %0\n\t"
		      ::"r" (p->gpr_fiq[0]) :"memory", "cc");
	asm volatile (" msr     r9_fiq, %0\n\t"
		      ::"r" (p->gpr_fiq[1]) :"memory", "cc");
	asm volatile (" msr     r10_fiq, %0\n\t"
		      ::"r" (p->gpr_fiq[2]) :"memory", "cc");
	asm volatile (" msr     r11_fiq, %0\n\t"
		      ::"r" (p->gpr_fiq[3]) :"memory", "cc");
	asm volatile (" msr     r12_fiq, %0\n\t"
		      ::"r" (p->gpr_fiq[4]) :"memory", "cc");
	asm volatile (" msr     SP_fiq, %0\n\t"
		      ::"r" (p->sp_fiq) :"memory", "cc");
	asm volatile (" msr     LR_fiq, %0\n\t"
		      ::"r" (p->lr_fiq) :"memory", "cc");
	asm volatile (" msr     SPSR_fiq, %0\n\t"
		      ::"r" (p->spsr_fiq) :"memory", "cc");
}

void arch_vcpu_switch(struct vmm_vcpu *tvcpu, 
		      struct vmm_vcpu *vcpu, 
		      arch_regs_t *regs)
{
	u32 ite;
	irq_flags_t flags;
	
	if (tvcpu) {
		/* Save general purpose registers */
		arm_regs(tvcpu)->pc = regs->pc;
		arm_regs(tvcpu)->lr = regs->lr;
		arm_regs(tvcpu)->sp = regs->sp;
		for (ite = 0; ite < CPU_GPR_COUNT; ite++) {
			arm_regs(tvcpu)->gpr[ite] = regs->gpr[ite];
		}
		arm_regs(tvcpu)->cpsr = regs->cpsr;
		if (tvcpu->is_normal) {
			/* Save VGIC registers */
			arm_vgic_save(tvcpu);
			/* Save generic timer */
			if (arm_feature(tvcpu, ARM_FEATURE_GENERIC_TIMER)) {
				generic_timer_vcpu_context_save(arm_gentimer_context(tvcpu));
			}
			/* Save general purpose banked registers */
			cpu_vcpu_banked_regs_save(tvcpu);
			/* Update last host CPU */
			arm_priv(tvcpu)->last_hcpu = vmm_smp_processor_id();
		}
	}

	/* Switch CP15 context */
	cpu_vcpu_cp15_switch_context(tvcpu, vcpu);

	/* Restore general purpose registers */
	regs->pc = arm_regs(vcpu)->pc;
	regs->lr = arm_regs(vcpu)->lr;
	regs->sp = arm_regs(vcpu)->sp;
	for (ite = 0; ite < CPU_GPR_COUNT; ite++) {
		regs->gpr[ite] = arm_regs(vcpu)->gpr[ite];
	}
	regs->cpsr = arm_regs(vcpu)->cpsr;
	if (vcpu->is_normal) {
		if (arm_priv(vcpu)->last_hcpu != vmm_smp_processor_id()) {
			/* Invalidate all guest TLB enteries because
			 * we might have stale guest TLB enteries from
			 * our previous run on new_hcpu host CPU 
			 */
			inv_tlb_guest_allis();
			/* Invalidate i-cache due always fetch fresh
			 * code after moving to new_hcpu host CPU
			 */
			invalidate_icache();
			/* Ensure changes are visible */
			dsb();
			isb();
		}
		/* Restore hypervisor config */
		vmm_spin_lock_irqsave(&arm_priv(vcpu)->hcr_lock, flags);
		write_hcr(arm_priv(vcpu)->hcr);
		vmm_spin_unlock_irqrestore(&arm_priv(vcpu)->hcr_lock, flags);
		write_hcptr(arm_priv(vcpu)->hcptr);
		write_hstr(arm_priv(vcpu)->hstr);
		/* Restore general purpose banked registers */
		cpu_vcpu_banked_regs_restore(vcpu);
		/* Restore generic timer */
		if (arm_feature(vcpu, ARM_FEATURE_GENERIC_TIMER)) {
			generic_timer_vcpu_context_restore(arm_gentimer_context(vcpu));
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
	vmm_cprintf(cdev, "    SP=0x%08x       LR=0x%08x       PC=0x%08x\n",
		    regs->sp, regs->lr, regs->pc);
	vmm_cprintf(cdev, "    CPSR=0x%08x     \n", regs->cpsr);
	vmm_cprintf(cdev, "  General Purpose Registers");
	for (ite = 0; ite < CPU_GPR_COUNT; ite++) {
		if (ite % 3 == 0)
			vmm_cprintf(cdev, "\n");
		vmm_cprintf(cdev, "    R%02d=0x%08x  ", ite, regs->gpr[ite]);
	}
	vmm_cprintf(cdev, "\n");
}

void cpu_vcpu_dump_user_reg(arch_regs_t *regs)
{
	__cpu_vcpu_dump_user_reg(NULL, regs);
}

void arch_vcpu_regs_dump(struct vmm_chardev *cdev, struct vmm_vcpu *vcpu)
{
	u32 ite;
	arm_priv_t *p;
	/* For both Normal & Orphan VCPUs */
	__cpu_vcpu_dump_user_reg(cdev, arm_regs(vcpu));
	/* For only Normal VCPUs */
	if (!vcpu->is_normal) {
		return;
	}
	p = arm_priv(vcpu);
	vmm_cprintf(cdev, "  User Mode Registers (Banked)\n");
	vmm_cprintf(cdev, "    SP=0x%08x       LR=0x%08x\n",
		    p->sp_usr, arm_regs(vcpu)->lr);
	vmm_cprintf(cdev, "  Supervisor Mode Registers (Banked)\n");
	vmm_cprintf(cdev, "    SP=0x%08x       LR=0x%08x       SPSR=0x%08x\n",
		    p->sp_svc, p->lr_svc, p->spsr_svc);
	vmm_cprintf(cdev, "  Abort Mode Registers (Banked)\n");
	vmm_cprintf(cdev, "    SP=0x%08x       LR=0x%08x       SPSR=0x%08x\n",
		    p->sp_abt, p->lr_abt, p->spsr_abt);
	vmm_cprintf(cdev, "  Undefined Mode Registers (Banked)\n");
	vmm_cprintf(cdev, "    SP=0x%08x       LR=0x%08x       SPSR=0x%08x\n",
		    p->sp_und, p->lr_und, p->spsr_und);
	vmm_cprintf(cdev, "  IRQ Mode Registers (Banked)\n");
	vmm_cprintf(cdev, "    SP=0x%08x       LR=0x%08x       SPSR=0x%08x\n",
		    p->sp_irq, p->lr_irq, p->spsr_irq);
	vmm_cprintf(cdev, "  FIQ Mode Registers (Banked)\n");
	vmm_cprintf(cdev, "    SP=0x%08x       LR=0x%08x       SPSR=0x%08x",
		    p->sp_fiq, p->lr_fiq, p->spsr_fiq);
	for (ite = 0; ite < 5; ite++) {
		if (ite % 3 == 0)
			vmm_cprintf(cdev, "\n");
		vmm_cprintf(cdev, "    R%02d=0x%08x  ", 
			    (ite + 8), p->gpr_fiq[ite]);
	}
	vmm_cprintf(cdev, "\n");
}

void arch_vcpu_stat_dump(struct vmm_chardev *cdev, struct vmm_vcpu *vcpu)
{
	/* For now no arch specific stats */
}
