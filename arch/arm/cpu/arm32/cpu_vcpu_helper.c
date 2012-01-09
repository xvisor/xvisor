/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file cpu_vcpu_helper.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief source of VCPU helper functions
 */

#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_manager.h>
#include <cpu_defines.h>
#include <cpu_vcpu_cp15.h>
#include <cpu_vcpu_helper.h>
#include <vmm_math.h>

void cpu_vcpu_halt(vmm_vcpu_t * vcpu, vmm_user_regs_t * regs)
{
	if (vcpu->state != VMM_VCPU_STATE_HALTED) {
		vmm_printf("\n");
		cpu_vcpu_dump_user_reg(vcpu, regs);
		vmm_manager_vcpu_halt(vcpu);
	}
}

u32 cpu_vcpu_cpsr_retrieve(vmm_vcpu_t * vcpu,
			  vmm_user_regs_t * regs)
{
	if (!vcpu || !regs) {
		return 0;
	}
	if (vcpu->is_normal) {
		return (regs->cpsr & CPSR_USERBITS_MASK) |
			(arm_sregs(vcpu)->cpsr & ~CPSR_USERBITS_MASK);
	} else {
		return regs->cpsr;
	}
}

void cpu_vcpu_banked_regs_save(vmm_vcpu_t * vcpu, vmm_user_regs_t * src)
{
	if (!vcpu || !vcpu->is_normal || !src) {
		return;
	}
	switch (arm_sregs(vcpu)->cpsr & CPSR_MODE_MASK) {
	case CPSR_MODE_USER:
		arm_sregs(vcpu)->gpr_usr[0] = src->gpr[8];
		arm_sregs(vcpu)->gpr_usr[1] = src->gpr[9];
		arm_sregs(vcpu)->gpr_usr[2] = src->gpr[10];
		arm_sregs(vcpu)->gpr_usr[3] = src->gpr[11];
		arm_sregs(vcpu)->gpr_usr[4] = src->gpr[12];
		arm_sregs(vcpu)->sp_usr = src->sp;
		arm_sregs(vcpu)->lr_usr = src->lr;
		break;
	case CPSR_MODE_SYSTEM:
		arm_sregs(vcpu)->gpr_usr[0] = src->gpr[8];
		arm_sregs(vcpu)->gpr_usr[1] = src->gpr[9];
		arm_sregs(vcpu)->gpr_usr[2] = src->gpr[10];
		arm_sregs(vcpu)->gpr_usr[3] = src->gpr[11];
		arm_sregs(vcpu)->gpr_usr[4] = src->gpr[12];
		arm_sregs(vcpu)->sp_usr = src->sp;
		arm_sregs(vcpu)->lr_usr = src->lr;
		break;
	case CPSR_MODE_ABORT:
		arm_sregs(vcpu)->gpr_usr[0] = src->gpr[8];
		arm_sregs(vcpu)->gpr_usr[1] = src->gpr[9];
		arm_sregs(vcpu)->gpr_usr[2] = src->gpr[10];
		arm_sregs(vcpu)->gpr_usr[3] = src->gpr[11];
		arm_sregs(vcpu)->gpr_usr[4] = src->gpr[12];
		arm_sregs(vcpu)->sp_abt = src->sp;
		arm_sregs(vcpu)->lr_abt = src->lr;
		break;
	case CPSR_MODE_UNDEFINED:
		arm_sregs(vcpu)->gpr_usr[0] = src->gpr[8];
		arm_sregs(vcpu)->gpr_usr[1] = src->gpr[9];
		arm_sregs(vcpu)->gpr_usr[2] = src->gpr[10];
		arm_sregs(vcpu)->gpr_usr[3] = src->gpr[11];
		arm_sregs(vcpu)->gpr_usr[4] = src->gpr[12];
		arm_sregs(vcpu)->sp_und = src->sp;
		arm_sregs(vcpu)->lr_und = src->lr;
		break;
	case CPSR_MODE_MONITOR:
		arm_sregs(vcpu)->gpr_usr[0] = src->gpr[8];
		arm_sregs(vcpu)->gpr_usr[1] = src->gpr[9];
		arm_sregs(vcpu)->gpr_usr[2] = src->gpr[10];
		arm_sregs(vcpu)->gpr_usr[3] = src->gpr[11];
		arm_sregs(vcpu)->gpr_usr[4] = src->gpr[12];
		arm_sregs(vcpu)->sp_mon = src->sp;
		arm_sregs(vcpu)->lr_mon = src->lr;
		break;
	case CPSR_MODE_SUPERVISOR:
		arm_sregs(vcpu)->gpr_usr[0] = src->gpr[8];
		arm_sregs(vcpu)->gpr_usr[1] = src->gpr[9];
		arm_sregs(vcpu)->gpr_usr[2] = src->gpr[10];
		arm_sregs(vcpu)->gpr_usr[3] = src->gpr[11];
		arm_sregs(vcpu)->gpr_usr[4] = src->gpr[12];
		arm_sregs(vcpu)->sp_svc = src->sp;
		arm_sregs(vcpu)->lr_svc = src->lr;
		break;
	case CPSR_MODE_IRQ:
		arm_sregs(vcpu)->gpr_usr[0] = src->gpr[8];
		arm_sregs(vcpu)->gpr_usr[1] = src->gpr[9];
		arm_sregs(vcpu)->gpr_usr[2] = src->gpr[10];
		arm_sregs(vcpu)->gpr_usr[3] = src->gpr[11];
		arm_sregs(vcpu)->gpr_usr[4] = src->gpr[12];
		arm_sregs(vcpu)->sp_irq = src->sp;
		arm_sregs(vcpu)->lr_irq = src->lr;
		break;
	case CPSR_MODE_FIQ:
		arm_sregs(vcpu)->gpr_fiq[0] = src->gpr[8];
		arm_sregs(vcpu)->gpr_fiq[1] = src->gpr[9];
		arm_sregs(vcpu)->gpr_fiq[2] = src->gpr[10];
		arm_sregs(vcpu)->gpr_fiq[3] = src->gpr[11];
		arm_sregs(vcpu)->gpr_fiq[4] = src->gpr[12];
		arm_sregs(vcpu)->sp_fiq = src->sp;
		arm_sregs(vcpu)->lr_fiq = src->lr;
		break;
	default:
		break;
	};
}

void cpu_vcpu_banked_regs_restore(vmm_vcpu_t * vcpu, vmm_user_regs_t * dst)
{
	if (!vcpu || !vcpu->is_normal || !dst) {
		return;
	}
	switch (arm_sregs(vcpu)->cpsr & CPSR_MODE_MASK) {
	case CPSR_MODE_USER:
		dst->gpr[8] = arm_sregs(vcpu)->gpr_usr[0];
		dst->gpr[9] = arm_sregs(vcpu)->gpr_usr[1];
		dst->gpr[10] = arm_sregs(vcpu)->gpr_usr[2];
		dst->gpr[11] = arm_sregs(vcpu)->gpr_usr[3];
		dst->gpr[12] = arm_sregs(vcpu)->gpr_usr[4];
		dst->sp = arm_sregs(vcpu)->sp_usr;
		dst->lr = arm_sregs(vcpu)->lr_usr;
		break;
	case CPSR_MODE_SYSTEM:
		dst->gpr[8] = arm_sregs(vcpu)->gpr_usr[0];
		dst->gpr[9] = arm_sregs(vcpu)->gpr_usr[1];
		dst->gpr[10] = arm_sregs(vcpu)->gpr_usr[2];
		dst->gpr[11] = arm_sregs(vcpu)->gpr_usr[3];
		dst->gpr[12] = arm_sregs(vcpu)->gpr_usr[4];
		dst->sp = arm_sregs(vcpu)->sp_usr;
		dst->lr = arm_sregs(vcpu)->lr_usr;
		break;
	case CPSR_MODE_ABORT:
		dst->gpr[8] = arm_sregs(vcpu)->gpr_usr[0];
		dst->gpr[9] = arm_sregs(vcpu)->gpr_usr[1];
		dst->gpr[10] = arm_sregs(vcpu)->gpr_usr[2];
		dst->gpr[11] = arm_sregs(vcpu)->gpr_usr[3];
		dst->gpr[12] = arm_sregs(vcpu)->gpr_usr[4];
		dst->sp = arm_sregs(vcpu)->sp_abt;
		dst->lr = arm_sregs(vcpu)->lr_abt;
		break;
	case CPSR_MODE_UNDEFINED:
		dst->gpr[8] = arm_sregs(vcpu)->gpr_usr[0];
		dst->gpr[9] = arm_sregs(vcpu)->gpr_usr[1];
		dst->gpr[10] = arm_sregs(vcpu)->gpr_usr[2];
		dst->gpr[11] = arm_sregs(vcpu)->gpr_usr[3];
		dst->gpr[12] = arm_sregs(vcpu)->gpr_usr[4];
		dst->sp = arm_sregs(vcpu)->sp_und;
		dst->lr = arm_sregs(vcpu)->lr_und;
		break;
	case CPSR_MODE_MONITOR:
		dst->gpr[8] = arm_sregs(vcpu)->gpr_usr[0];
		dst->gpr[9] = arm_sregs(vcpu)->gpr_usr[1];
		dst->gpr[10] = arm_sregs(vcpu)->gpr_usr[2];
		dst->gpr[11] = arm_sregs(vcpu)->gpr_usr[3];
		dst->gpr[12] = arm_sregs(vcpu)->gpr_usr[4];
		dst->sp = arm_sregs(vcpu)->sp_mon;
		dst->lr = arm_sregs(vcpu)->lr_mon;
		break;
	case CPSR_MODE_SUPERVISOR:
		dst->gpr[8] = arm_sregs(vcpu)->gpr_usr[0];
		dst->gpr[9] = arm_sregs(vcpu)->gpr_usr[1];
		dst->gpr[10] = arm_sregs(vcpu)->gpr_usr[2];
		dst->gpr[11] = arm_sregs(vcpu)->gpr_usr[3];
		dst->gpr[12] = arm_sregs(vcpu)->gpr_usr[4];
		dst->sp = arm_sregs(vcpu)->sp_svc;
		dst->lr = arm_sregs(vcpu)->lr_svc;
		break;
	case CPSR_MODE_IRQ:
		dst->gpr[8] = arm_sregs(vcpu)->gpr_usr[0];
		dst->gpr[9] = arm_sregs(vcpu)->gpr_usr[1];
		dst->gpr[10] = arm_sregs(vcpu)->gpr_usr[2];
		dst->gpr[11] = arm_sregs(vcpu)->gpr_usr[3];
		dst->gpr[12] = arm_sregs(vcpu)->gpr_usr[4];
		dst->sp = arm_sregs(vcpu)->sp_irq;
		dst->lr = arm_sregs(vcpu)->lr_irq;
		break;
	case CPSR_MODE_FIQ:
		dst->gpr[8] = arm_sregs(vcpu)->gpr_fiq[0];
		dst->gpr[9] = arm_sregs(vcpu)->gpr_fiq[1];
		dst->gpr[10] = arm_sregs(vcpu)->gpr_fiq[2];
		dst->gpr[11] = arm_sregs(vcpu)->gpr_fiq[3];
		dst->gpr[12] = arm_sregs(vcpu)->gpr_fiq[4];
		dst->sp = arm_sregs(vcpu)->sp_fiq;
		dst->lr = arm_sregs(vcpu)->lr_fiq;
		break;
	default:
		break;
	};
}

void cpu_vcpu_cpsr_update(vmm_vcpu_t * vcpu, 
			  vmm_user_regs_t * regs,
			  u32 new_cpsr,
			  u32 new_cpsr_mask)
{
	bool mode_change;
	/* Sanity check */
	if (!vcpu && !vcpu->is_normal) {
		return;
	}
	new_cpsr &= new_cpsr_mask;
	/* Determine if mode is changing */
	mode_change = FALSE;
	if ((new_cpsr_mask & CPSR_MODE_MASK) &&
	    ((arm_sregs(vcpu)->cpsr & CPSR_MODE_MASK) != 
					(new_cpsr & CPSR_MODE_MASK))) {
		mode_change = TRUE;
		/* Save banked registers for old CPSR */
		cpu_vcpu_banked_regs_save(vcpu, regs);
	}
	/* Set the new priviledged bits of CPSR */
	arm_sregs(vcpu)->cpsr &= (~CPSR_PRIVBITS_MASK | ~new_cpsr_mask);
	arm_sregs(vcpu)->cpsr |= new_cpsr & CPSR_PRIVBITS_MASK & new_cpsr_mask;
	/* Set the new user bits of CPSR */
	regs->cpsr &= (~CPSR_USERBITS_MASK | ~new_cpsr_mask);
	regs->cpsr |= new_cpsr & CPSR_USERBITS_MASK & new_cpsr_mask;
	/* If mode is changing then */
	if (mode_change) {
		/* Restore values of banked registers for new CPSR */
		cpu_vcpu_banked_regs_restore(vcpu, regs);
		/* Synchronize CP15 state to change in mode */
		cpu_vcpu_cp15_sync_cpsr(vcpu);
	}
	return;
}

u32 cpu_vcpu_spsr_retrieve(vmm_vcpu_t * vcpu)
{
	/* Find out correct SPSR */
	switch (arm_sregs(vcpu)->cpsr & CPSR_MODE_MASK) {
	case CPSR_MODE_ABORT:
		return arm_sregs(vcpu)->spsr_abt;
		break;
	case CPSR_MODE_UNDEFINED:
		return arm_sregs(vcpu)->spsr_und;
		break;
	case CPSR_MODE_MONITOR:
		return arm_sregs(vcpu)->spsr_mon;
		break;
	case CPSR_MODE_SUPERVISOR:
		return arm_sregs(vcpu)->spsr_svc;
		break;
	case CPSR_MODE_IRQ:
		return arm_sregs(vcpu)->spsr_irq;
		break;
	case CPSR_MODE_FIQ:
		return arm_sregs(vcpu)->spsr_fiq;
		break;
	default:
		break;
	};
	return 0;
}

int cpu_vcpu_spsr_update(vmm_vcpu_t * vcpu, 
			 u32 new_spsr,
			 u32 new_spsr_mask)
{
	/* Sanity check */
	if (!vcpu && !vcpu->is_normal) {
		return VMM_EFAIL;
	}
	if ((arm_sregs(vcpu)->cpsr & CPSR_MODE_MASK) == CPSR_MODE_USER) {
		return VMM_EFAIL;
	}
	new_spsr &= new_spsr_mask;
	/* Update appropriate SPSR */
	switch (arm_sregs(vcpu)->cpsr & CPSR_MODE_MASK) {
	case CPSR_MODE_ABORT:
		arm_sregs(vcpu)->spsr_abt &= ~new_spsr_mask;
		arm_sregs(vcpu)->spsr_abt |= new_spsr;
		break;
	case CPSR_MODE_UNDEFINED:
		arm_sregs(vcpu)->spsr_und &= ~new_spsr_mask;
		arm_sregs(vcpu)->spsr_und |= new_spsr;
		break;
	case CPSR_MODE_MONITOR:
		arm_sregs(vcpu)->spsr_mon &= ~new_spsr_mask;
		arm_sregs(vcpu)->spsr_mon |= new_spsr;
		break;
	case CPSR_MODE_SUPERVISOR:
		arm_sregs(vcpu)->spsr_svc &= ~new_spsr_mask;
		arm_sregs(vcpu)->spsr_svc |= new_spsr;
		break;
	case CPSR_MODE_IRQ:
		arm_sregs(vcpu)->spsr_irq &= ~new_spsr_mask;
		arm_sregs(vcpu)->spsr_irq |= new_spsr;
		break;
	case CPSR_MODE_FIQ:
		arm_sregs(vcpu)->spsr_fiq &= ~new_spsr_mask;
		arm_sregs(vcpu)->spsr_fiq |= new_spsr;
		break;
	default:
		break;
	};
	/* Return success */
	return VMM_OK;
}

u32 cpu_vcpu_reg_read(vmm_vcpu_t * vcpu, 
		      vmm_user_regs_t * regs, 
		      u32 reg_num) 
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
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
		return regs->gpr[reg_num];
		break;
	case 13:
		return regs->sp;
		break;
	case 14:
		return regs->lr;
		break;
	case 15:
		return regs->pc;
		break;
	default:
		break;
	};
	return 0x0;
}

void cpu_vcpu_reg_write(vmm_vcpu_t * vcpu, 
			vmm_user_regs_t * regs, 
			u32 reg_num, 
			u32 reg_val) 
{
	u32 curmode = arm_sregs(vcpu)->cpsr & CPSR_MODE_MASK;
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
	case 9:
	case 10:
	case 11:
	case 12:
		regs->gpr[reg_num] = reg_val;
		if (curmode == CPSR_MODE_FIQ) {
			arm_sregs(vcpu)->gpr_fiq[reg_num - 8] = reg_val;
		} else {
			arm_sregs(vcpu)->gpr_usr[reg_num - 8] = reg_val;
		}
		break;
	case 13:
		regs->sp = reg_val;
		switch (curmode) {
		case CPSR_MODE_USER:
		case CPSR_MODE_SYSTEM:
			arm_sregs(vcpu)->sp_usr = reg_val;
			break;
		case CPSR_MODE_FIQ:
			arm_sregs(vcpu)->sp_fiq = reg_val;
			break;
		case CPSR_MODE_IRQ:
			arm_sregs(vcpu)->sp_irq = reg_val;
			break;
		case CPSR_MODE_SUPERVISOR:
			arm_sregs(vcpu)->sp_svc = reg_val;
			break;
		case CPSR_MODE_ABORT:
			arm_sregs(vcpu)->sp_abt = reg_val;
			break;
		case CPSR_MODE_UNDEFINED:
			arm_sregs(vcpu)->sp_und = reg_val;
			break;
		case CPSR_MODE_MONITOR:
			arm_sregs(vcpu)->sp_mon = reg_val;
			break;
		default:
			break;
		};
		break;
	case 14:
		regs->lr = reg_val;
		switch (curmode) {
		case CPSR_MODE_USER:
		case CPSR_MODE_SYSTEM:
			arm_sregs(vcpu)->lr_usr = reg_val;
			break;
		case CPSR_MODE_FIQ:
			arm_sregs(vcpu)->lr_fiq = reg_val;
			break;
		case CPSR_MODE_IRQ:
			arm_sregs(vcpu)->lr_irq = reg_val;
			break;
		case CPSR_MODE_SUPERVISOR:
			arm_sregs(vcpu)->lr_svc = reg_val;
			break;
		case CPSR_MODE_ABORT:
			arm_sregs(vcpu)->lr_abt = reg_val;
			break;
		case CPSR_MODE_UNDEFINED:
			arm_sregs(vcpu)->lr_und = reg_val;
			break;
		case CPSR_MODE_MONITOR:
			arm_sregs(vcpu)->lr_mon = reg_val;
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

u32 cpu_vcpu_regmode_read(vmm_vcpu_t * vcpu, 
			  vmm_user_regs_t * regs, 
			  u32 mode,
			  u32 reg_num)
{
	u32 curmode = arm_sregs(vcpu)->cpsr & CPSR_MODE_MASK;
	if (mode == curmode) {
		return cpu_vcpu_reg_read(vcpu, regs, reg_num);
	} else {
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
			break;
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
			if (curmode == CPSR_MODE_FIQ) {
				return arm_sregs(vcpu)->gpr_usr[reg_num - 8];
			} else {
				if (mode == CPSR_MODE_FIQ) {
					return arm_sregs(vcpu)->
							gpr_fiq[reg_num - 8];
				} else {
					return regs->gpr[reg_num];
				}
			}
			break;
		case 13:
			switch (mode) {
			case CPSR_MODE_USER:
			case CPSR_MODE_SYSTEM:
				return arm_sregs(vcpu)->sp_usr;
				break;
			case CPSR_MODE_FIQ:
				return arm_sregs(vcpu)->sp_fiq;
				break;
			case CPSR_MODE_IRQ:
				return arm_sregs(vcpu)->sp_irq;
				break;
			case CPSR_MODE_SUPERVISOR:
				return arm_sregs(vcpu)->sp_svc;
				break;
			case CPSR_MODE_ABORT:
				return arm_sregs(vcpu)->sp_abt;
				break;
			case CPSR_MODE_UNDEFINED:
				return arm_sregs(vcpu)->sp_und;
				break;
			case CPSR_MODE_MONITOR:
				return arm_sregs(vcpu)->sp_mon;
				break;
			default:
				break;
			};
			break;
		case 14:
			switch (mode) {
			case CPSR_MODE_USER:
			case CPSR_MODE_SYSTEM:
				return arm_sregs(vcpu)->lr_usr;
				break;
			case CPSR_MODE_FIQ:
				return arm_sregs(vcpu)->lr_fiq;
				break;
			case CPSR_MODE_IRQ:
				return arm_sregs(vcpu)->lr_irq;
				break;
			case CPSR_MODE_SUPERVISOR:
				return arm_sregs(vcpu)->lr_svc;
				break;
			case CPSR_MODE_ABORT:
				return arm_sregs(vcpu)->lr_abt;
				break;
			case CPSR_MODE_UNDEFINED:
				return arm_sregs(vcpu)->lr_und;
				break;
			case CPSR_MODE_MONITOR:
				return arm_sregs(vcpu)->lr_mon;
				break;
			default:
				break;
			};
			break;
		case 15:
			return regs->pc;
			break;
		default:
			break;
		};
	}
	return 0x0;
}

void cpu_vcpu_regmode_write(vmm_vcpu_t * vcpu, 
			    vmm_user_regs_t * regs, 
			    u32 mode,
			    u32 reg_num,
			    u32 reg_val)
{
	u32 curmode = arm_sregs(vcpu)->cpsr & CPSR_MODE_MASK;
	if (mode == curmode) {
		cpu_vcpu_reg_write(vcpu, regs, reg_num, reg_val);
	} else {
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
		case 9:
		case 10:
		case 11:
		case 12:
			if (curmode == CPSR_MODE_FIQ) {
				arm_sregs(vcpu)->gpr_usr[reg_num - 8] = reg_val;
			} else {
				if (mode == CPSR_MODE_FIQ) {
					arm_sregs(vcpu)->gpr_fiq[reg_num - 8] = 
								reg_val;
				} else {
					regs->gpr[reg_num] = reg_val;
				}
			}
			break;
		case 13:
			switch (mode) {
			case CPSR_MODE_USER:
			case CPSR_MODE_SYSTEM:
				arm_sregs(vcpu)->sp_usr = reg_val;
				break;
			case CPSR_MODE_FIQ:
				arm_sregs(vcpu)->sp_fiq = reg_val;
				break;
			case CPSR_MODE_IRQ:
				arm_sregs(vcpu)->sp_irq = reg_val;
				break;
			case CPSR_MODE_SUPERVISOR:
				arm_sregs(vcpu)->sp_svc = reg_val;
				break;
			case CPSR_MODE_ABORT:
				arm_sregs(vcpu)->sp_abt = reg_val;
				break;
			case CPSR_MODE_UNDEFINED:
				arm_sregs(vcpu)->sp_und = reg_val;
				break;
			case CPSR_MODE_MONITOR:
				arm_sregs(vcpu)->sp_mon = reg_val;
				break;
			default:
				break;
			};
			break;
		case 14:
			switch (mode) {
			case CPSR_MODE_USER:
			case CPSR_MODE_SYSTEM:
				arm_sregs(vcpu)->lr_usr = reg_val;
				break;
			case CPSR_MODE_FIQ:
				arm_sregs(vcpu)->lr_fiq = reg_val;
				break;
			case CPSR_MODE_IRQ:
				arm_sregs(vcpu)->lr_irq = reg_val;
				break;
			case CPSR_MODE_SUPERVISOR:
				arm_sregs(vcpu)->lr_svc = reg_val;
				break;
			case CPSR_MODE_ABORT:
				arm_sregs(vcpu)->lr_abt = reg_val;
				break;
			case CPSR_MODE_UNDEFINED:
				arm_sregs(vcpu)->lr_und = reg_val;
				break;
			case CPSR_MODE_MONITOR:
				arm_sregs(vcpu)->lr_mon = reg_val;
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
}

int vmm_vcpu_regs_init(vmm_vcpu_t * vcpu)
{
	u32 ite, cpuid = ARM_CPUID_CORTEXA8;
	/* Initialize User Mode Registers */
	/* For both Orphan & Normal VCPUs */
	vmm_memset(arm_uregs(vcpu), 0, sizeof(vmm_user_regs_t));
	arm_uregs(vcpu)->pc = vcpu->start_pc;
	if (vcpu->is_normal) {
		arm_uregs(vcpu)->cpsr  = CPSR_ZERO_MASK;
		arm_uregs(vcpu)->cpsr |= CPSR_ASYNC_ABORT_DISABLED;
		arm_uregs(vcpu)->cpsr |= CPSR_MODE_USER;
	} else {
		arm_uregs(vcpu)->cpsr  = CPSR_ZERO_MASK;
		arm_uregs(vcpu)->cpsr |= CPSR_ASYNC_ABORT_DISABLED;
		arm_uregs(vcpu)->cpsr |= CPSR_MODE_SUPERVISOR;
		arm_uregs(vcpu)->sp = vcpu->start_sp;
	}
	/* Initialize Supervisor Mode Registers */
	/* For only Normal VCPUs */
	if (!vcpu->is_normal) {
		return VMM_OK;
	}
	if (!vcpu->reset_count) {
		vcpu->sregs_priv = vmm_malloc(sizeof(arm_super_regs_t));
		vmm_memset(arm_sregs(vcpu), 0, sizeof(arm_super_regs_t));
		arm_sregs(vcpu)->cpsr = CPSR_ASYNC_ABORT_DISABLED | 
				   CPSR_IRQ_DISABLED |
				   CPSR_FIQ_DISABLED | 
				   CPSR_MODE_SUPERVISOR;
	} else {
		for (ite = 0; ite < CPU_FIQ_GPR_COUNT; ite++) {
			arm_sregs(vcpu)->gpr_usr[ite] = 0x0;
			arm_sregs(vcpu)->gpr_fiq[ite] = 0x0;
		}
		arm_sregs(vcpu)->sp_usr = 0x0;
		arm_sregs(vcpu)->lr_usr = 0x0;
		arm_sregs(vcpu)->sp_svc = 0x0;
		arm_sregs(vcpu)->lr_svc = 0x0;
		arm_sregs(vcpu)->spsr_svc = 0x0;
		arm_sregs(vcpu)->sp_mon = 0x0;
		arm_sregs(vcpu)->lr_mon = 0x0;
		arm_sregs(vcpu)->spsr_mon = 0x0;
		arm_sregs(vcpu)->sp_abt = 0x0;
		arm_sregs(vcpu)->lr_abt = 0x0;
		arm_sregs(vcpu)->spsr_abt = 0x0;
		arm_sregs(vcpu)->sp_und = 0x0;
		arm_sregs(vcpu)->lr_und = 0x0;
		arm_sregs(vcpu)->spsr_und = 0x0;
		arm_sregs(vcpu)->sp_irq = 0x0;
		arm_sregs(vcpu)->lr_irq = 0x0;
		arm_sregs(vcpu)->spsr_irq = 0x0;
		arm_sregs(vcpu)->sp_fiq = 0x0;
		arm_sregs(vcpu)->lr_fiq = 0x0;
		arm_sregs(vcpu)->spsr_fiq = 0x0;
		cpu_vcpu_cpsr_update(vcpu, 
				     arm_uregs(vcpu), 
				     (CPSR_ZERO_MASK |
					CPSR_ASYNC_ABORT_DISABLED | 
					CPSR_IRQ_DISABLED |
					CPSR_FIQ_DISABLED | 
					CPSR_MODE_SUPERVISOR),
				     CPSR_ALLBITS_MASK);
	}
	if (!vcpu->reset_count) {
		arm_sregs(vcpu)->features = 0;
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
			break;
		default:
			break;
		};
	}
#ifdef CONFIG_ARM32_FUNCSTATS
	for (ite=0; ite < ARM_FUNCSTAT_MAX; ite++) {
		arm_sregs(vcpu)->funcstat[ite].function_name = NULL;
		arm_sregs(vcpu)->funcstat[ite].entry_count = 0;
		arm_sregs(vcpu)->funcstat[ite].exit_count = 0;
		arm_sregs(vcpu)->funcstat[ite].time = 0;
	}
#endif
	return cpu_vcpu_cp15_init(vcpu, cpuid);
}

void vmm_vcpu_regs_switch(vmm_vcpu_t * tvcpu,
			  vmm_vcpu_t * vcpu, vmm_user_regs_t * regs)
{
	u32 ite;
	/* Save user registers & banked registers */
	if (tvcpu) {
		arm_uregs(tvcpu)->pc = regs->pc;
		arm_uregs(tvcpu)->lr = regs->lr;
		arm_uregs(tvcpu)->sp = regs->sp;
		for (ite = 0; ite < CPU_GPR_COUNT; ite++) {
			arm_uregs(tvcpu)->gpr[ite] = regs->gpr[ite];
		}
		arm_uregs(tvcpu)->cpsr = regs->cpsr;
		if(tvcpu->is_normal) {
			cpu_vcpu_banked_regs_save(tvcpu, regs);
		}
	}
	/* Switch CP15 context */
	cpu_vcpu_cp15_switch_context(tvcpu, vcpu);
	/* Restore user registers & banked registers */
	regs->pc = arm_uregs(vcpu)->pc;
	regs->lr = arm_uregs(vcpu)->lr;
	regs->sp = arm_uregs(vcpu)->sp;
	for (ite = 0; ite < CPU_GPR_COUNT; ite++) {
		regs->gpr[ite] = arm_uregs(vcpu)->gpr[ite];
	}
	regs->cpsr = arm_uregs(vcpu)->cpsr;
	if (vcpu->is_normal) {
		cpu_vcpu_banked_regs_restore(vcpu, regs);
	}
}

void cpu_vcpu_dump_user_reg(vmm_vcpu_t * vcpu, vmm_user_regs_t * regs)
{
	u32 ite;
	vmm_printf("  Core Registers\n");
	vmm_printf("    SP=0x%08x       LR=0x%08x       PC=0x%08x\n",
		   regs->sp, regs->lr, regs->pc);
	vmm_printf("    CPSR=0x%08x     \n", 
				cpu_vcpu_cpsr_retrieve(vcpu, regs));
	vmm_printf("  General Purpose Registers");
	for (ite = 0; ite < CPU_GPR_COUNT; ite++) {
		if (ite % 3 == 0)
			vmm_printf("\n");
		vmm_printf("    R%02d=0x%08x  ", ite, regs->gpr[ite]);
	}
	vmm_printf("\n");
}

void vmm_vcpu_regs_dump(vmm_vcpu_t * vcpu)
{
	u32 ite;
	/* For both Normal & Orphan VCPUs */
	cpu_vcpu_dump_user_reg(vcpu, arm_uregs(vcpu));
	/* For only Normal VCPUs */
	if (!vcpu->is_normal) {
		return;
	}
	vmm_printf("  User Mode Registers (Banked)\n");
	vmm_printf("    SP=0x%08x       LR=0x%08x\n",
		   arm_sregs(vcpu)->sp_usr, arm_sregs(vcpu)->lr_usr);
	vmm_printf("  Supervisor Mode Registers (Banked)\n");
	vmm_printf("    SP=0x%08x       LR=0x%08x       SPSR=0x%08x\n",
		   arm_sregs(vcpu)->sp_svc, arm_sregs(vcpu)->lr_svc,
		   arm_sregs(vcpu)->spsr_svc);
	vmm_printf("  Monitor Mode Registers (Banked)\n");
	vmm_printf("    SP=0x%08x       LR=0x%08x       SPSR=0x%08x\n",
		   arm_sregs(vcpu)->sp_mon, arm_sregs(vcpu)->lr_mon,
		   arm_sregs(vcpu)->spsr_mon);
	vmm_printf("  Abort Mode Registers (Banked)\n");
	vmm_printf("    SP=0x%08x       LR=0x%08x       SPSR=0x%08x\n",
		   arm_sregs(vcpu)->sp_abt, arm_sregs(vcpu)->lr_abt,
		   arm_sregs(vcpu)->spsr_abt);
	vmm_printf("  Undefined Mode Registers (Banked)\n");
	vmm_printf("    SP=0x%08x       LR=0x%08x       SPSR=0x%08x\n",
		   arm_sregs(vcpu)->sp_und, arm_sregs(vcpu)->lr_und,
		   arm_sregs(vcpu)->spsr_und);
	vmm_printf("  IRQ Mode Registers (Banked)\n");
	vmm_printf("    SP=0x%08x       LR=0x%08x       SPSR=0x%08x\n",
		   arm_sregs(vcpu)->sp_irq, arm_sregs(vcpu)->lr_irq,
		   arm_sregs(vcpu)->spsr_irq);
	vmm_printf("  FIQ Mode Registers (Banked)\n");
	vmm_printf("    SP=0x%08x       LR=0x%08x       SPSR=0x%08x",
		   arm_sregs(vcpu)->sp_fiq, arm_sregs(vcpu)->lr_fiq,
		   arm_sregs(vcpu)->spsr_fiq);
	for (ite = 0; ite < 5; ite++) {
		if (ite % 3 == 0)
			vmm_printf("\n");
		vmm_printf("    R%02d=0x%08x  ", (ite + 8),
			   arm_sregs(vcpu)->gpr_fiq[ite]);
	}
	vmm_printf("\n");
}

void vmm_vcpu_stat_dump(vmm_vcpu_t * vcpu)
{
#ifdef CONFIG_ARM32_FUNCSTATS
	int index;

	if (!vcpu || !arm_sregs(vcpu)) {
		return;
	}

	vmm_printf("%-30s %-10s %s\n", "Function Name","Time/Call", "# Calls");

	for (index=0; index < ARM_FUNCSTAT_MAX; index++) {
		if (arm_sregs(vcpu)->funcstat[index].exit_count) { 
			vmm_printf("%-30s %-10u %u\n", 
			arm_sregs(vcpu)->funcstat[index].function_name, 
			(u32)vmm_udiv64(arm_sregs(vcpu)->funcstat[index].time, 
			arm_sregs(vcpu)->funcstat[index].exit_count), 
			arm_sregs(vcpu)->funcstat[index].exit_count); 
		} 
	} 
#else
	vmm_printf("Not selected in Xvisor config\n");
#endif
}
