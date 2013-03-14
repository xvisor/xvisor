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
#include <vmm_stdio.h>
#include <vmm_manager.h>
#include <vmm_scheduler.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <cpu_defines.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_cp15.h>
#include <cpu_vcpu_helper.h>
#include <generic_timer.h>

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
			return arm_priv(vcpu)->gpr_fiq[reg_num - 8];
		} else {
			return regs->gpr[reg_num];
		}
	case 9:
		if (mode == CPSR_MODE_FIQ) {
			asm volatile (" mrs     %0, r9_fiq\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->gpr_fiq[reg_num - 8] = hwreg;
			return arm_priv(vcpu)->gpr_fiq[reg_num - 8];
		} else {
			return regs->gpr[reg_num];
		}
	case 10:
		if (mode == CPSR_MODE_FIQ) {
			asm volatile (" mrs     %0, r10_fiq\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->gpr_fiq[reg_num - 8] = hwreg;
			return arm_priv(vcpu)->gpr_fiq[reg_num - 8];
		} else {
			return regs->gpr[reg_num];
		}
	case 11:
		if (mode == CPSR_MODE_FIQ) {
			asm volatile (" mrs     %0, r11_fiq\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->gpr_fiq[reg_num - 8] = hwreg;
			return arm_priv(vcpu)->gpr_fiq[reg_num - 8];
		} else {
			return regs->gpr[reg_num];
		}
	case 12:
		if (mode == CPSR_MODE_FIQ) {
			asm volatile (" mrs     %0, r12_fiq\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->gpr_fiq[reg_num - 8] = hwreg;
			return arm_priv(vcpu)->gpr_fiq[reg_num - 8];
		} else {
			return regs->gpr[reg_num];
		}
	case 13:
		switch (mode) {
		case CPSR_MODE_USER:
		case CPSR_MODE_SYSTEM:
			asm volatile (" mrs     %0, sp_usr\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->sp_usr = hwreg;
			return arm_priv(vcpu)->sp_usr;
		case CPSR_MODE_FIQ:
			return arm_priv(vcpu)->sp_fiq;
		case CPSR_MODE_IRQ:
			asm volatile (" mrs     %0, sp_irq\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->sp_irq = hwreg;
			return arm_priv(vcpu)->sp_irq;
		case CPSR_MODE_SUPERVISOR:
			asm volatile (" mrs     %0, sp_svc\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->sp_svc = hwreg;
			return arm_priv(vcpu)->sp_svc;
		case CPSR_MODE_ABORT:
			asm volatile (" mrs     %0, sp_abt\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->sp_abt = hwreg;
			return arm_priv(vcpu)->sp_abt;
		case CPSR_MODE_UNDEFINED:
			asm volatile (" mrs     %0, sp_und\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->sp_und = hwreg;
			return arm_priv(vcpu)->sp_und;
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
			asm volatile (" mrs     %0, lr_fiq\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->lr_fiq = hwreg;
			return arm_priv(vcpu)->lr_fiq;
		case CPSR_MODE_IRQ:
			asm volatile (" mrs     %0, lr_irq\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->lr_irq = hwreg;
			return arm_priv(vcpu)->lr_irq;
		case CPSR_MODE_SUPERVISOR:
			asm volatile (" mrs     %0, lr_svc\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->lr_svc = hwreg;
			return arm_priv(vcpu)->lr_svc;
		case CPSR_MODE_ABORT:
			asm volatile (" mrs     %0, lr_abt\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->lr_abt = hwreg;
			return arm_priv(vcpu)->lr_abt;
		case CPSR_MODE_UNDEFINED:
			asm volatile (" mrs     %0, lr_und\n\t" 
				      :"=r" (hwreg)::"memory", "cc");
			arm_priv(vcpu)->lr_und = hwreg;
			return arm_priv(vcpu)->lr_und;
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
			asm volatile (" msr     sp_usr, %0\n\t"
				      ::"r" (reg_val) :"memory", "cc");
			arm_priv(vcpu)->sp_usr = reg_val;
			break;
		case CPSR_MODE_FIQ:
			/* FIXME:
			asm volatile (" msr     sp_fiq, %0\n\t"
				      ::"r" (reg_val) :"memory", "cc"); */
			arm_priv(vcpu)->sp_fiq = reg_val;
			break;
		case CPSR_MODE_IRQ:
			asm volatile (" msr     sp_irq, %0\n\t"
				      ::"r" (reg_val) :"memory", "cc");
			arm_priv(vcpu)->sp_irq = reg_val;
			break;
		case CPSR_MODE_SUPERVISOR:
			asm volatile (" msr     sp_svc, %0\n\t"
				      ::"r" (reg_val) :"memory", "cc");
			arm_priv(vcpu)->sp_svc = reg_val;
			break;
		case CPSR_MODE_ABORT:
			asm volatile (" msr     sp_abt, %0\n\t"
				      ::"r" (reg_val) :"memory", "cc");
			arm_priv(vcpu)->sp_abt = reg_val;
			break;
		case CPSR_MODE_UNDEFINED:
			asm volatile (" msr     sp_und, %0\n\t"
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
			asm volatile (" msr     lr_fiq, %0\n\t"
				      ::"r" (reg_val) :"memory", "cc");
			arm_priv(vcpu)->lr_fiq = reg_val;
			break;
		case CPSR_MODE_IRQ:
			asm volatile (" msr     lr_irq, %0\n\t"
				      ::"r" (reg_val) :"memory", "cc");
			arm_priv(vcpu)->lr_irq = reg_val;
			break;
		case CPSR_MODE_SUPERVISOR:
			asm volatile (" msr     lr_svc, %0\n\t"
				      ::"r" (reg_val) :"memory", "cc");
			arm_priv(vcpu)->lr_svc = reg_val;
			break;
		case CPSR_MODE_ABORT:
			asm volatile (" msr     lr_abt, %0\n\t"
				      ::"r" (reg_val) :"memory", "cc");
			arm_priv(vcpu)->lr_abt = reg_val;
			break;
		case CPSR_MODE_UNDEFINED:
			asm volatile (" msr     lr_und, %0\n\t"
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
		asm volatile (" mrs     %0, spsr_abt\n\t" 
			      :"=r" (hwreg)::"memory", "cc");
		arm_priv(vcpu)->spsr_abt = hwreg;
		return arm_priv(vcpu)->spsr_abt;
	case CPSR_MODE_UNDEFINED:
		asm volatile (" mrs     %0, spsr_und\n\t" 
			      :"=r" (hwreg)::"memory", "cc");
		arm_priv(vcpu)->spsr_und = hwreg;
		return arm_priv(vcpu)->spsr_und;
	case CPSR_MODE_SUPERVISOR:
		asm volatile (" mrs     %0, spsr_svc\n\t" 
			      :"=r" (hwreg)::"memory", "cc");
		arm_priv(vcpu)->spsr_svc = hwreg;
		return arm_priv(vcpu)->spsr_svc;
	case CPSR_MODE_IRQ:
		asm volatile (" mrs     %0, spsr_irq\n\t" 
			      :"=r" (hwreg)::"memory", "cc");
		arm_priv(vcpu)->spsr_irq = hwreg;
		return arm_priv(vcpu)->spsr_irq;
	case CPSR_MODE_FIQ:
		asm volatile (" mrs     %0, spsr_fiq\n\t" 
			      :"=r" (hwreg)::"memory", "cc");
		arm_priv(vcpu)->spsr_fiq = hwreg;
		return arm_priv(vcpu)->spsr_fiq;
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
	if (!vcpu && !vcpu->is_normal) {
		return VMM_EFAIL;
	}
	if (vcpu != vmm_scheduler_current_vcpu()) {
		/* This function should only be called for current VCPU */
		vmm_panic("%d not called for current vcpu\n", __func__);
	}
	/* Update appropriate SPSR */
	switch (mode) {
	case CPSR_MODE_ABORT:
		asm volatile (" msr     spsr_abt, %0\n\t"
			      ::"r" (new_spsr) :"memory", "cc");
		arm_priv(vcpu)->spsr_abt = new_spsr;
		break;
	case CPSR_MODE_UNDEFINED:
		asm volatile (" msr     spsr_und, %0\n\t"
			      ::"r" (new_spsr) :"memory", "cc");
		arm_priv(vcpu)->spsr_und = new_spsr;
		break;
	case CPSR_MODE_SUPERVISOR:
		asm volatile (" msr     spsr_svc, %0\n\t"
			      ::"r" (new_spsr) :"memory", "cc");
		arm_priv(vcpu)->spsr_svc = new_spsr;
		break;
	case CPSR_MODE_IRQ:
		asm volatile (" msr     spsr_irq, %0\n\t"
			      ::"r" (new_spsr) :"memory", "cc");
		arm_priv(vcpu)->spsr_irq = new_spsr;
		break;
	case CPSR_MODE_FIQ:
		asm volatile (" msr     spsr_fiq, %0\n\t"
			      ::"r" (new_spsr) :"memory", "cc");
		arm_priv(vcpu)->spsr_fiq = new_spsr;
		break;
	default:
		break;
	};
	/* Return success */
	return VMM_OK;
}

int arch_guest_init(struct vmm_guest *guest)
{
	if (!guest->reset_count) {
		guest->arch_priv = vmm_malloc(sizeof(arm_guest_priv_t));
		if (!guest->arch_priv) {
			return VMM_EFAIL;
		}
		INIT_SPIN_LOCK(&arm_guest_priv(guest)->ttbl_lock);
		arm_guest_priv(guest)->ttbl = cpu_mmu_ttbl_alloc(TTBL_STAGE2);
	}
	return VMM_OK;
}

int arch_guest_deinit(struct vmm_guest *guest)
{
	int rc;

	if (guest->arch_priv) {
		if ((rc = cpu_mmu_ttbl_free(arm_guest_priv(guest)->ttbl))) {
			return rc;
		}

		vmm_free(guest->arch_priv);
	}

	return VMM_OK;
}

int arch_vcpu_init(struct vmm_vcpu *vcpu)
{
	u32 ite, cpuid = 0;
	const char *attr;
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
	if (strcmp(attr, "ARMv7a,cortex-a8") == 0) {
		cpuid = ARM_CPUID_CORTEXA8;
	} else if (strcmp(attr, "ARMv7a,cortex-a9") == 0) {
		cpuid = ARM_CPUID_CORTEXA9;
	} else if (strcmp(attr, "ARMv7a,cortex-a15") == 0) {
		cpuid = ARM_CPUID_CORTEXA15;
	} else {
		return VMM_EFAIL;
	}
	if (!vcpu->reset_count) {
		vcpu->arch_priv = vmm_zalloc(sizeof(arm_priv_t));
		if (!vcpu->arch_priv) {
			return VMM_EFAIL;
		}
	} else {
		for (ite = 0; ite < CPU_FIQ_GPR_COUNT; ite++) {
			arm_priv(vcpu)->gpr_fiq[ite] = 0x0;
		}
		arm_priv(vcpu)->sp_usr = 0x0;
		arm_priv(vcpu)->sp_svc = 0x0;
		arm_priv(vcpu)->lr_svc = 0x0;
		arm_priv(vcpu)->spsr_svc = 0x0;
		arm_priv(vcpu)->sp_abt = 0x0;
		arm_priv(vcpu)->lr_abt = 0x0;
		arm_priv(vcpu)->spsr_abt = 0x0;
		arm_priv(vcpu)->sp_und = 0x0;
		arm_priv(vcpu)->lr_und = 0x0;
		arm_priv(vcpu)->spsr_und = 0x0;
		arm_priv(vcpu)->sp_irq = 0x0;
		arm_priv(vcpu)->lr_irq = 0x0;
		arm_priv(vcpu)->spsr_irq = 0x0;
		arm_priv(vcpu)->sp_fiq = 0x0;
		arm_priv(vcpu)->lr_fiq = 0x0;
		arm_priv(vcpu)->spsr_fiq = 0x0;
	}
	if (!vcpu->reset_count) {
		/* Initialize Hypervisor Configuration */
		arm_priv(vcpu)->hcr = (HCR_TAC_MASK |
					HCR_TIDCP_MASK |
					HCR_TSC_MASK |
					HCR_TWI_MASK |
					HCR_AMO_MASK |
					HCR_IMO_MASK |
					HCR_FMO_MASK |
					HCR_SWIO_MASK |
					HCR_VM_MASK);
		/* Initialize Hypervisor Coprocessor Trap Register */
		arm_priv(vcpu)->hcptr = (HCPTR_TCPAC_MASK |
					 HCPTR_TTA_MASK |
					 HCPTR_TASE_MASK |
					 HCPTR_TCP_MASK);
		/* Initialize Hypervisor System Trap Register */
		arm_priv(vcpu)->hstr = (HSTR_TJDBX_MASK |
					HSTR_TTEE_MASK |
					HSTR_T9_MASK);
		/* Initialize VCPU features */
		arm_priv(vcpu)->features = 0;
		switch (cpuid) {
		case ARM_CPUID_CORTEXA8:
			arm_set_feature(vcpu, ARM_FEATURE_V4T);
			arm_set_feature(vcpu, ARM_FEATURE_V5);
			arm_set_feature(vcpu, ARM_FEATURE_V6);
			arm_set_feature(vcpu, ARM_FEATURE_V6K);
			arm_set_feature(vcpu, ARM_FEATURE_V7);
			arm_set_feature(vcpu, ARM_FEATURE_AUXCR);
			arm_set_feature(vcpu, ARM_FEATURE_THUMB2);
			arm_set_feature(vcpu, ARM_FEATURE_VFP);
			arm_set_feature(vcpu, ARM_FEATURE_VFP3);
			arm_set_feature(vcpu, ARM_FEATURE_NEON);
			arm_set_feature(vcpu, ARM_FEATURE_THUMB2EE);
			arm_set_feature(vcpu, ARM_FEATURE_TRUSTZONE);
			break;
		case ARM_CPUID_CORTEXA9:
			arm_set_feature(vcpu, ARM_FEATURE_V4T);
			arm_set_feature(vcpu, ARM_FEATURE_V5);
			arm_set_feature(vcpu, ARM_FEATURE_V6);
			arm_set_feature(vcpu, ARM_FEATURE_V6K);
			arm_set_feature(vcpu, ARM_FEATURE_V7);
			arm_set_feature(vcpu, ARM_FEATURE_AUXCR);
			arm_set_feature(vcpu, ARM_FEATURE_THUMB2);
			arm_set_feature(vcpu, ARM_FEATURE_VFP);
			arm_set_feature(vcpu, ARM_FEATURE_VFP3);
			arm_set_feature(vcpu, ARM_FEATURE_VFP_FP16);
			arm_set_feature(vcpu, ARM_FEATURE_NEON);
			arm_set_feature(vcpu, ARM_FEATURE_THUMB2EE);
			arm_set_feature(vcpu, ARM_FEATURE_V7MP);
			arm_set_feature(vcpu, ARM_FEATURE_TRUSTZONE);
			break;
		case ARM_CPUID_CORTEXA15:
			arm_set_feature(vcpu, ARM_FEATURE_V4T);
			arm_set_feature(vcpu, ARM_FEATURE_V5);
			arm_set_feature(vcpu, ARM_FEATURE_V6);
			arm_set_feature(vcpu, ARM_FEATURE_V6K);
			arm_set_feature(vcpu, ARM_FEATURE_V7);
			arm_set_feature(vcpu, ARM_FEATURE_V7MP);
			arm_set_feature(vcpu, ARM_FEATURE_AUXCR);
			arm_set_feature(vcpu, ARM_FEATURE_THUMB2);
			arm_set_feature(vcpu, ARM_FEATURE_THUMB2EE);
			arm_set_feature(vcpu, ARM_FEATURE_DIV);
			arm_set_feature(vcpu, ARM_FEATURE_VFP);
			arm_set_feature(vcpu, ARM_FEATURE_VFP4);
			arm_set_feature(vcpu, ARM_FEATURE_VFP_FP16);
			arm_set_feature(vcpu, ARM_FEATURE_NEON);
			arm_set_feature(vcpu, ARM_FEATURE_LPAE);
			arm_set_feature(vcpu, ARM_FEATURE_GENTIMER);
			arm_set_feature(vcpu, ARM_FEATURE_TRUSTZONE);
			break;
		default:
			break;
		};
	} else {
		/* Clear virtual exception bits in HCR */
		arm_priv(vcpu)->hcr &= ~(HCR_VA_MASK | 
					 HCR_VI_MASK | 
					 HCR_VF_MASK);
	}
	/* Intialize Generic timer */
	if (arm_feature(vcpu, ARM_FEATURE_GENTIMER)) {
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
	asm volatile (" mrs     %0, sp_usr\n\t" 
		      :"=r" (arm_priv(vcpu)->sp_usr)::"memory", "cc");
	asm volatile (" mrs     %0, sp_svc\n\t" 
		      :"=r" (arm_priv(vcpu)->sp_svc)::"memory", "cc");
	asm volatile (" mrs     %0, lr_svc\n\t" 
		      :"=r" (arm_priv(vcpu)->lr_svc)::"memory", "cc");
	asm volatile (" mrs     %0, spsr_svc\n\t" 
		      :"=r" (arm_priv(vcpu)->spsr_svc)::"memory", "cc");
	asm volatile (" mrs     %0, sp_abt\n\t" 
		      :"=r" (arm_priv(vcpu)->sp_abt)::"memory", "cc");
	asm volatile (" mrs     %0, lr_abt\n\t" 
		      :"=r" (arm_priv(vcpu)->lr_abt)::"memory", "cc");
	asm volatile (" mrs     %0, spsr_abt\n\t" 
		      :"=r" (arm_priv(vcpu)->spsr_abt)::"memory", "cc");
	asm volatile (" mrs     %0, sp_und\n\t" 
		      :"=r" (arm_priv(vcpu)->sp_und)::"memory", "cc");
	asm volatile (" mrs     %0, lr_und\n\t" 
		      :"=r" (arm_priv(vcpu)->lr_und)::"memory", "cc");
	asm volatile (" mrs     %0, spsr_und\n\t" 
		      :"=r" (arm_priv(vcpu)->spsr_und)::"memory", "cc");
	asm volatile (" mrs     %0, sp_irq\n\t" 
		      :"=r" (arm_priv(vcpu)->sp_irq)::"memory", "cc");
	asm volatile (" mrs     %0, lr_irq\n\t" 
		      :"=r" (arm_priv(vcpu)->lr_irq)::"memory", "cc");
	asm volatile (" mrs     %0, spsr_irq\n\t" 
		      :"=r" (arm_priv(vcpu)->spsr_irq)::"memory", "cc");
	/* FIXME: asm volatile (" mrs     %0, sp_fiq\n\t" 
		      :"=r" (arm_priv(vcpu)->sp_fiq)::"memory", "cc"); */
	asm volatile (" mrs     %0, lr_fiq\n\t" 
		      :"=r" (arm_priv(vcpu)->lr_fiq)::"memory", "cc");
	asm volatile (" mrs     %0, spsr_fiq\n\t" 
		      :"=r" (arm_priv(vcpu)->spsr_fiq)::"memory", "cc");
}

static void cpu_vcpu_banked_regs_restore(struct vmm_vcpu *vcpu)
{
	asm volatile (" msr     sp_usr, %0\n\t"
		      ::"r" (arm_priv(vcpu)->sp_usr) :"memory", "cc");
	asm volatile (" msr     sp_svc, %0\n\t"
		      ::"r" (arm_priv(vcpu)->sp_svc) :"memory", "cc");
	asm volatile (" msr     lr_svc, %0\n\t"
		      ::"r" (arm_priv(vcpu)->lr_svc) :"memory", "cc");
	asm volatile (" msr     spsr_svc, %0\n\t"
		      ::"r" (arm_priv(vcpu)->spsr_svc) :"memory", "cc");
	asm volatile (" msr     sp_abt, %0\n\t"
		      ::"r" (arm_priv(vcpu)->sp_abt) :"memory", "cc");
	asm volatile (" msr     lr_abt, %0\n\t"
		      ::"r" (arm_priv(vcpu)->lr_abt) :"memory", "cc");
	asm volatile (" msr     spsr_abt, %0\n\t"
		      ::"r" (arm_priv(vcpu)->spsr_abt) :"memory", "cc");
	asm volatile (" msr     sp_und, %0\n\t"
		      ::"r" (arm_priv(vcpu)->sp_und) :"memory", "cc");
	asm volatile (" msr     lr_und, %0\n\t"
		      ::"r" (arm_priv(vcpu)->lr_und) :"memory", "cc");
	asm volatile (" msr     spsr_und, %0\n\t"
		      ::"r" (arm_priv(vcpu)->spsr_und) :"memory", "cc");
	asm volatile (" msr     sp_irq, %0\n\t"
		      ::"r" (arm_priv(vcpu)->sp_irq) :"memory", "cc");
	asm volatile (" msr     lr_irq, %0\n\t"
		      ::"r" (arm_priv(vcpu)->lr_irq) :"memory", "cc");
	asm volatile (" msr     spsr_irq, %0\n\t"
		      ::"r" (arm_priv(vcpu)->spsr_irq) :"memory", "cc");
	asm volatile (" msr     r8_fiq, %0\n\t"
		      ::"r" (arm_priv(vcpu)->gpr_fiq[0]) :"memory", "cc");
	asm volatile (" msr     r9_fiq, %0\n\t"
		      ::"r" (arm_priv(vcpu)->gpr_fiq[1]) :"memory", "cc");
	asm volatile (" msr     r10_fiq, %0\n\t"
		      ::"r" (arm_priv(vcpu)->gpr_fiq[2]) :"memory", "cc");
	asm volatile (" msr     r11_fiq, %0\n\t"
		      ::"r" (arm_priv(vcpu)->gpr_fiq[3]) :"memory", "cc");
	asm volatile (" msr     r12_fiq, %0\n\t"
		      ::"r" (arm_priv(vcpu)->gpr_fiq[4]) :"memory", "cc");
	/* FIXME: asm volatile (" msr     sp_fiq, %0\n\t"
		      ::"r" (arm_priv(vcpu)->sp_fiq) :"memory", "cc");*/
	asm volatile (" msr     lr_fiq, %0\n\t"
		      ::"r" (arm_priv(vcpu)->lr_fiq) :"memory", "cc");
	asm volatile (" msr     spsr_fiq, %0\n\t"
		      ::"r" (arm_priv(vcpu)->spsr_fiq) :"memory", "cc");
}

void arch_vcpu_switch(struct vmm_vcpu *tvcpu, 
		      struct vmm_vcpu *vcpu, 
		      arch_regs_t *regs)
{
	u32 ite;
	
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
			if (arm_feature(tvcpu, ARM_FEATURE_GENTIMER)) {
				generic_timer_vcpu_context_save(arm_gentimer_context(tvcpu));
			}
			/* Save general purpose banked registers */
			cpu_vcpu_banked_regs_save(tvcpu);
			/* Save hypervisor config */
			arm_priv(tvcpu)->hcr = read_hcr();
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
		/* Restore hypervisor config */
		write_hcr(arm_priv(vcpu)->hcr);
		write_hcptr(arm_priv(vcpu)->hcptr);
		write_hstr(arm_priv(vcpu)->hstr);
		/* Restore general purpose banked registers */
		cpu_vcpu_banked_regs_restore(vcpu);
		/* Restore generic timer */
		if (arm_feature(vcpu, ARM_FEATURE_GENTIMER)) {
			generic_timer_vcpu_context_restore(arm_gentimer_context(vcpu));
		}
		/* Restore VGIC registers */
		arm_vgic_restore(vcpu);
	}

	/* Clear exclusive monitor */
	clrex();
}

void cpu_vcpu_dump_user_reg(arch_regs_t *regs)
{
	u32 ite;
	vmm_printf("  Core Registers\n");
	vmm_printf("    SP=0x%08x       LR=0x%08x       PC=0x%08x\n",
		   regs->sp, regs->lr, regs->pc);
	vmm_printf("    CPSR=0x%08x     \n", regs->cpsr);
	vmm_printf("  General Purpose Registers");
	for (ite = 0; ite < CPU_GPR_COUNT; ite++) {
		if (ite % 3 == 0)
			vmm_printf("\n");
		vmm_printf("    R%02d=0x%08x  ", ite, regs->gpr[ite]);
	}
	vmm_printf("\n");
}

void arch_vcpu_regs_dump(struct vmm_vcpu *vcpu)
{
	u32 ite;
	/* For both Normal & Orphan VCPUs */
	cpu_vcpu_dump_user_reg(arm_regs(vcpu));
	/* For only Normal VCPUs */
	if (!vcpu->is_normal) {
		return;
	}
	vmm_printf("  User Mode Registers (Banked)\n");
	vmm_printf("    SP=0x%08x       LR=0x%08x\n",
		   arm_priv(vcpu)->sp_usr, arm_regs(vcpu)->lr);
	vmm_printf("  Supervisor Mode Registers (Banked)\n");
	vmm_printf("    SP=0x%08x       LR=0x%08x       SPSR=0x%08x\n",
		   arm_priv(vcpu)->sp_svc, arm_priv(vcpu)->lr_svc,
		   arm_priv(vcpu)->spsr_svc);
	vmm_printf("  Abort Mode Registers (Banked)\n");
	vmm_printf("    SP=0x%08x       LR=0x%08x       SPSR=0x%08x\n",
		   arm_priv(vcpu)->sp_abt, arm_priv(vcpu)->lr_abt,
		   arm_priv(vcpu)->spsr_abt);
	vmm_printf("  Undefined Mode Registers (Banked)\n");
	vmm_printf("    SP=0x%08x       LR=0x%08x       SPSR=0x%08x\n",
		   arm_priv(vcpu)->sp_und, arm_priv(vcpu)->lr_und,
		   arm_priv(vcpu)->spsr_und);
	vmm_printf("  IRQ Mode Registers (Banked)\n");
	vmm_printf("    SP=0x%08x       LR=0x%08x       SPSR=0x%08x\n",
		   arm_priv(vcpu)->sp_irq, arm_priv(vcpu)->lr_irq,
		   arm_priv(vcpu)->spsr_irq);
	vmm_printf("  FIQ Mode Registers (Banked)\n");
	vmm_printf("    SP=0x%08x       LR=0x%08x       SPSR=0x%08x",
		   arm_priv(vcpu)->sp_fiq, arm_priv(vcpu)->lr_fiq,
		   arm_priv(vcpu)->spsr_fiq);
	for (ite = 0; ite < 5; ite++) {
		if (ite % 3 == 0)
			vmm_printf("\n");
		vmm_printf("    R%02d=0x%08x  ", (ite + 8),
			   arm_priv(vcpu)->gpr_fiq[ite]);
	}
	vmm_printf("\n");
}

void arch_vcpu_stat_dump(struct vmm_vcpu *vcpu)
{
	vmm_printf("No VCPU stats available\n");
}
