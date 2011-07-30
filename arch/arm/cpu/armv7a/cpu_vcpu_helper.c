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
#include <vmm_guest.h>
#include <vmm_scheduler.h>
#include <cpu_defines.h>
#include <cpu_vcpu_cp15.h>
#include <cpu_vcpu_helper.h>

void cpu_vcpu_halt(vmm_vcpu_t * vcpu, vmm_user_regs_t * regs)
{
	vmm_printf("\n");
	cpu_vcpu_dump_user_reg(vcpu, regs);
	vmm_scheduler_vcpu_halt(vcpu);
}

u32 cpu_vcpu_cpsr_retrive(vmm_vcpu_t * vcpu,
			  vmm_user_regs_t * regs)
{
	if (!vcpu || !regs) {
		return 0;
	}
	if (vcpu->guest) {
		return (((regs->cpsr & CPSR_USERBITS_MASK) |
			 (vcpu->sregs.cpsr & CPSR_PRIVBITS_MASK)) &
			CPSR_VALIDBITS_MASK);
	} else {
		return regs->cpsr & CPSR_VALIDBITS_MASK;
	}
}

void cpu_vcpu_banked_regs_save(vmm_vcpu_t * vcpu, vmm_user_regs_t * src)
{
	if (!vcpu || !vcpu->guest || !src) {
		return;
	}
	switch (vcpu->sregs.cpsr & CPSR_MODE_MASK) {
	case CPSR_MODE_USER:
		vcpu->sregs.gpr_usr[0] = src->gpr[8];
		vcpu->sregs.gpr_usr[1] = src->gpr[9];
		vcpu->sregs.gpr_usr[2] = src->gpr[10];
		vcpu->sregs.gpr_usr[3] = src->gpr[11];
		vcpu->sregs.gpr_usr[4] = src->gpr[12];
		vcpu->sregs.sp_usr = src->sp;
		vcpu->sregs.lr_usr = src->lr;
		break;
	case CPSR_MODE_SYSTEM:
		vcpu->sregs.gpr_usr[0] = src->gpr[8];
		vcpu->sregs.gpr_usr[1] = src->gpr[9];
		vcpu->sregs.gpr_usr[2] = src->gpr[10];
		vcpu->sregs.gpr_usr[3] = src->gpr[11];
		vcpu->sregs.gpr_usr[4] = src->gpr[12];
		vcpu->sregs.sp_usr = src->sp;
		vcpu->sregs.lr_usr = src->lr;
		break;
	case CPSR_MODE_ABORT:
		vcpu->sregs.gpr_usr[0] = src->gpr[8];
		vcpu->sregs.gpr_usr[1] = src->gpr[9];
		vcpu->sregs.gpr_usr[2] = src->gpr[10];
		vcpu->sregs.gpr_usr[3] = src->gpr[11];
		vcpu->sregs.gpr_usr[4] = src->gpr[12];
		vcpu->sregs.sp_abt = src->sp;
		vcpu->sregs.lr_abt = src->lr;
		break;
	case CPSR_MODE_UNDEFINED:
		vcpu->sregs.gpr_usr[0] = src->gpr[8];
		vcpu->sregs.gpr_usr[1] = src->gpr[9];
		vcpu->sregs.gpr_usr[2] = src->gpr[10];
		vcpu->sregs.gpr_usr[3] = src->gpr[11];
		vcpu->sregs.gpr_usr[4] = src->gpr[12];
		vcpu->sregs.sp_und = src->sp;
		vcpu->sregs.lr_und = src->lr;
		break;
	case CPSR_MODE_MONITOR:
		vcpu->sregs.gpr_usr[0] = src->gpr[8];
		vcpu->sregs.gpr_usr[1] = src->gpr[9];
		vcpu->sregs.gpr_usr[2] = src->gpr[10];
		vcpu->sregs.gpr_usr[3] = src->gpr[11];
		vcpu->sregs.gpr_usr[4] = src->gpr[12];
		vcpu->sregs.sp_mon = src->sp;
		vcpu->sregs.lr_mon = src->lr;
		break;
	case CPSR_MODE_SUPERVISOR:
		vcpu->sregs.gpr_usr[0] = src->gpr[8];
		vcpu->sregs.gpr_usr[1] = src->gpr[9];
		vcpu->sregs.gpr_usr[2] = src->gpr[10];
		vcpu->sregs.gpr_usr[3] = src->gpr[11];
		vcpu->sregs.gpr_usr[4] = src->gpr[12];
		vcpu->sregs.sp_svc = src->sp;
		vcpu->sregs.lr_svc = src->lr;
		break;
	case CPSR_MODE_IRQ:
		vcpu->sregs.gpr_usr[0] = src->gpr[8];
		vcpu->sregs.gpr_usr[1] = src->gpr[9];
		vcpu->sregs.gpr_usr[2] = src->gpr[10];
		vcpu->sregs.gpr_usr[3] = src->gpr[11];
		vcpu->sregs.gpr_usr[4] = src->gpr[12];
		vcpu->sregs.sp_irq = src->sp;
		vcpu->sregs.lr_irq = src->lr;
		break;
	case CPSR_MODE_FIQ:
		vcpu->sregs.gpr_fiq[0] = src->gpr[8];
		vcpu->sregs.gpr_fiq[1] = src->gpr[9];
		vcpu->sregs.gpr_fiq[2] = src->gpr[10];
		vcpu->sregs.gpr_fiq[3] = src->gpr[11];
		vcpu->sregs.gpr_fiq[4] = src->gpr[12];
		vcpu->sregs.sp_fiq = src->sp;
		vcpu->sregs.lr_fiq = src->lr;
		break;
	default:
		break;
	};
}

void cpu_vcpu_banked_regs_restore(vmm_vcpu_t * vcpu, vmm_user_regs_t * dst)
{
	if (!vcpu || !vcpu->guest || !dst) {
		return;
	}
	switch (vcpu->sregs.cpsr & CPSR_MODE_MASK) {
	case CPSR_MODE_USER:
		dst->gpr[8] = vcpu->sregs.gpr_usr[0];
		dst->gpr[9] = vcpu->sregs.gpr_usr[1];
		dst->gpr[10] = vcpu->sregs.gpr_usr[2];
		dst->gpr[11] = vcpu->sregs.gpr_usr[3];
		dst->gpr[12] = vcpu->sregs.gpr_usr[4];
		dst->sp = vcpu->sregs.sp_usr;
		dst->lr = vcpu->sregs.lr_usr;
		break;
	case CPSR_MODE_SYSTEM:
		dst->gpr[8] = vcpu->sregs.gpr_usr[0];
		dst->gpr[9] = vcpu->sregs.gpr_usr[1];
		dst->gpr[10] = vcpu->sregs.gpr_usr[2];
		dst->gpr[11] = vcpu->sregs.gpr_usr[3];
		dst->gpr[12] = vcpu->sregs.gpr_usr[4];
		dst->sp = vcpu->sregs.sp_usr;
		dst->lr = vcpu->sregs.lr_usr;
		break;
	case CPSR_MODE_ABORT:
		dst->gpr[8] = vcpu->sregs.gpr_usr[0];
		dst->gpr[9] = vcpu->sregs.gpr_usr[1];
		dst->gpr[10] = vcpu->sregs.gpr_usr[2];
		dst->gpr[11] = vcpu->sregs.gpr_usr[3];
		dst->gpr[12] = vcpu->sregs.gpr_usr[4];
		dst->sp = vcpu->sregs.sp_abt;
		dst->lr = vcpu->sregs.lr_abt;
		break;
	case CPSR_MODE_UNDEFINED:
		dst->gpr[8] = vcpu->sregs.gpr_usr[0];
		dst->gpr[9] = vcpu->sregs.gpr_usr[1];
		dst->gpr[10] = vcpu->sregs.gpr_usr[2];
		dst->gpr[11] = vcpu->sregs.gpr_usr[3];
		dst->gpr[12] = vcpu->sregs.gpr_usr[4];
		dst->sp = vcpu->sregs.sp_und;
		dst->lr = vcpu->sregs.lr_und;
		break;
	case CPSR_MODE_MONITOR:
		dst->gpr[8] = vcpu->sregs.gpr_usr[0];
		dst->gpr[9] = vcpu->sregs.gpr_usr[1];
		dst->gpr[10] = vcpu->sregs.gpr_usr[2];
		dst->gpr[11] = vcpu->sregs.gpr_usr[3];
		dst->gpr[12] = vcpu->sregs.gpr_usr[4];
		dst->sp = vcpu->sregs.sp_mon;
		dst->lr = vcpu->sregs.lr_mon;
		break;
	case CPSR_MODE_SUPERVISOR:
		dst->gpr[8] = vcpu->sregs.gpr_usr[0];
		dst->gpr[9] = vcpu->sregs.gpr_usr[1];
		dst->gpr[10] = vcpu->sregs.gpr_usr[2];
		dst->gpr[11] = vcpu->sregs.gpr_usr[3];
		dst->gpr[12] = vcpu->sregs.gpr_usr[4];
		dst->sp = vcpu->sregs.sp_svc;
		dst->lr = vcpu->sregs.lr_svc;
		break;
	case CPSR_MODE_IRQ:
		dst->gpr[8] = vcpu->sregs.gpr_usr[0];
		dst->gpr[9] = vcpu->sregs.gpr_usr[1];
		dst->gpr[10] = vcpu->sregs.gpr_usr[2];
		dst->gpr[11] = vcpu->sregs.gpr_usr[3];
		dst->gpr[12] = vcpu->sregs.gpr_usr[4];
		dst->sp = vcpu->sregs.sp_irq;
		dst->lr = vcpu->sregs.lr_irq;
		break;
	case CPSR_MODE_FIQ:
		dst->gpr[8] = vcpu->sregs.gpr_fiq[0];
		dst->gpr[9] = vcpu->sregs.gpr_fiq[1];
		dst->gpr[10] = vcpu->sregs.gpr_fiq[2];
		dst->gpr[11] = vcpu->sregs.gpr_fiq[3];
		dst->gpr[12] = vcpu->sregs.gpr_fiq[4];
		dst->sp = vcpu->sregs.sp_fiq;
		dst->lr = vcpu->sregs.lr_fiq;
		break;
	default:
		break;
	};
}

void cpu_vcpu_cpsr_update(vmm_vcpu_t * vcpu, 
			  vmm_user_regs_t * regs,
			  u32 new_cpsr)
{
	u32 old_cpsr = cpu_vcpu_cpsr_retrive(vcpu, regs);
	new_cpsr &= CPSR_VALIDBITS_MASK;
	/* Sanity check */
	if (!vcpu) {
		return;
	}
	if (vcpu->guest == NULL) {
		return;
	}
	/* If mode is changing then */
	if ((old_cpsr & CPSR_MODE_MASK) != (new_cpsr & CPSR_MODE_MASK)) {
		/* Save banked registers for old CPSR */
		cpu_vcpu_banked_regs_save(vcpu, regs);
	}
	/* Set the new priviledged bits of CPSR */
	vcpu->sregs.cpsr &= ~CPSR_PRIVBITS_MASK;
	vcpu->sregs.cpsr |= new_cpsr & CPSR_PRIVBITS_MASK;
	/* Set the new user bits of CPSR */
	regs->cpsr &= ~CPSR_USERBITS_MASK;
	regs->cpsr |= new_cpsr & CPSR_USERBITS_MASK;
	/* If mode is changing then */
	if ((old_cpsr & CPSR_MODE_MASK) != (new_cpsr & CPSR_MODE_MASK)) {
		/* Restore values of banked registers for new CPSR */
		cpu_vcpu_banked_regs_restore(vcpu, regs);
		/* Synchronize CP15 state to change in mode */
		cpu_vcpu_cp15_sync_cpsr(vcpu);
	}
	return;
}

u32 cpu_vcpu_spsr_retrive(vmm_vcpu_t * vcpu)
{
	u32 retval = 0x0;
	/* Find out correct SPSR */
	switch (vcpu->sregs.cpsr & CPSR_MODE_MASK) {
	case CPSR_MODE_ABORT:
		retval = vcpu->sregs.spsr_abt;
		break;
	case CPSR_MODE_UNDEFINED:
		retval = vcpu->sregs.spsr_und;
		break;
	case CPSR_MODE_MONITOR:
		retval = vcpu->sregs.spsr_mon;
		break;
	case CPSR_MODE_SUPERVISOR:
		retval = vcpu->sregs.spsr_svc;
		break;
	case CPSR_MODE_IRQ:
		retval = vcpu->sregs.spsr_irq;
		break;
	case CPSR_MODE_FIQ:
		retval = vcpu->sregs.spsr_fiq;
		break;
	default:
		break;
	};
	return retval;
}

int cpu_vcpu_spsr_update(vmm_vcpu_t * vcpu, 
			 u32 new_spsr)
{
	new_spsr &= CPSR_VALIDBITS_MASK;
	/* Sanity check */
	if (!vcpu) {
		return VMM_EFAIL;
	}
	if (((vcpu->sregs.cpsr & CPSR_MODE_MASK) == CPSR_MODE_USER) || 
	    (vcpu->guest == NULL)) {
		return VMM_EFAIL;
	}
	/* Update appropriate SPSR */
	switch (vcpu->sregs.cpsr & CPSR_MODE_MASK) {
	case CPSR_MODE_ABORT:
		vcpu->sregs.spsr_abt = new_spsr;
		break;
	case CPSR_MODE_UNDEFINED:
		vcpu->sregs.spsr_und = new_spsr;
		break;
	case CPSR_MODE_MONITOR:
		vcpu->sregs.spsr_mon = new_spsr;
		break;
	case CPSR_MODE_SUPERVISOR:
		vcpu->sregs.spsr_svc = new_spsr;
		break;
	case CPSR_MODE_IRQ:
		vcpu->sregs.spsr_irq = new_spsr;
		break;
	case CPSR_MODE_FIQ:
		vcpu->sregs.spsr_fiq = new_spsr;
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
	u32 curmode = vcpu->sregs.cpsr & CPSR_MODE_MASK;
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
			vcpu->sregs.gpr_fiq[reg_num - 8] = reg_val;
		} else {
			vcpu->sregs.gpr_usr[reg_num - 8] = reg_val;
		}
		break;
	case 13:
		regs->sp = reg_val;
		switch (curmode) {
		case CPSR_MODE_USER:
		case CPSR_MODE_SYSTEM:
			vcpu->sregs.sp_usr = reg_val;
			break;
		case CPSR_MODE_FIQ:
			vcpu->sregs.sp_fiq = reg_val;
			break;
		case CPSR_MODE_IRQ:
			vcpu->sregs.sp_irq = reg_val;
			break;
		case CPSR_MODE_SUPERVISOR:
			vcpu->sregs.sp_svc = reg_val;
			break;
		case CPSR_MODE_ABORT:
			vcpu->sregs.sp_abt = reg_val;
			break;
		case CPSR_MODE_UNDEFINED:
			vcpu->sregs.sp_und = reg_val;
			break;
		case CPSR_MODE_MONITOR:
			vcpu->sregs.sp_mon = reg_val;
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
			vcpu->sregs.lr_usr = reg_val;
			break;
		case CPSR_MODE_FIQ:
			vcpu->sregs.lr_fiq = reg_val;
			break;
		case CPSR_MODE_IRQ:
			vcpu->sregs.lr_irq = reg_val;
			break;
		case CPSR_MODE_SUPERVISOR:
			vcpu->sregs.lr_svc = reg_val;
			break;
		case CPSR_MODE_ABORT:
			vcpu->sregs.lr_abt = reg_val;
			break;
		case CPSR_MODE_UNDEFINED:
			vcpu->sregs.lr_und = reg_val;
			break;
		case CPSR_MODE_MONITOR:
			vcpu->sregs.lr_mon = reg_val;
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
	u32 curmode = vcpu->sregs.cpsr & CPSR_MODE_MASK;
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
				return vcpu->sregs.gpr_usr[reg_num - 8];
			} else {
				if (mode == CPSR_MODE_FIQ) {
					return vcpu->sregs.
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
				return vcpu->sregs.sp_usr;
				break;
			case CPSR_MODE_FIQ:
				return vcpu->sregs.sp_fiq;
				break;
			case CPSR_MODE_IRQ:
				return vcpu->sregs.sp_irq;
				break;
			case CPSR_MODE_SUPERVISOR:
				return vcpu->sregs.sp_svc;
				break;
			case CPSR_MODE_ABORT:
				return vcpu->sregs.sp_abt;
				break;
			case CPSR_MODE_UNDEFINED:
				return vcpu->sregs.sp_und;
				break;
			case CPSR_MODE_MONITOR:
				return vcpu->sregs.sp_mon;
				break;
			default:
				break;
			};
			break;
		case 14:
			switch (mode) {
			case CPSR_MODE_USER:
			case CPSR_MODE_SYSTEM:
				return vcpu->sregs.lr_usr;
				break;
			case CPSR_MODE_FIQ:
				return vcpu->sregs.lr_fiq;
				break;
			case CPSR_MODE_IRQ:
				return vcpu->sregs.lr_irq;
				break;
			case CPSR_MODE_SUPERVISOR:
				return vcpu->sregs.lr_svc;
				break;
			case CPSR_MODE_ABORT:
				return vcpu->sregs.lr_abt;
				break;
			case CPSR_MODE_UNDEFINED:
				return vcpu->sregs.lr_und;
				break;
			case CPSR_MODE_MONITOR:
				return vcpu->sregs.lr_mon;
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
	u32 curmode = vcpu->sregs.cpsr & CPSR_MODE_MASK;
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
				vcpu->sregs.gpr_usr[reg_num - 8] = reg_val;
			} else {
				if (mode == CPSR_MODE_FIQ) {
					vcpu->sregs.gpr_fiq[reg_num - 8] = 
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
				vcpu->sregs.sp_usr = reg_val;
				break;
			case CPSR_MODE_FIQ:
				vcpu->sregs.sp_fiq = reg_val;
				break;
			case CPSR_MODE_IRQ:
				vcpu->sregs.sp_irq = reg_val;
				break;
			case CPSR_MODE_SUPERVISOR:
				vcpu->sregs.sp_svc = reg_val;
				break;
			case CPSR_MODE_ABORT:
				vcpu->sregs.sp_abt = reg_val;
				break;
			case CPSR_MODE_UNDEFINED:
				vcpu->sregs.sp_und = reg_val;
				break;
			case CPSR_MODE_MONITOR:
				vcpu->sregs.sp_mon = reg_val;
				break;
			default:
				break;
			};
			break;
		case 14:
			switch (mode) {
			case CPSR_MODE_USER:
			case CPSR_MODE_SYSTEM:
				vcpu->sregs.lr_usr = reg_val;
				break;
			case CPSR_MODE_FIQ:
				vcpu->sregs.lr_fiq = reg_val;
				break;
			case CPSR_MODE_IRQ:
				vcpu->sregs.lr_irq = reg_val;
				break;
			case CPSR_MODE_SUPERVISOR:
				vcpu->sregs.lr_svc = reg_val;
				break;
			case CPSR_MODE_ABORT:
				vcpu->sregs.lr_abt = reg_val;
				break;
			case CPSR_MODE_UNDEFINED:
				vcpu->sregs.lr_und = reg_val;
				break;
			case CPSR_MODE_MONITOR:
				vcpu->sregs.lr_mon = reg_val;
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
	vmm_memset(&vcpu->uregs, 0, sizeof(vmm_user_regs_t));
	vcpu->uregs.pc = vcpu->start_pc;
	if (vcpu->guest) {
		vcpu->uregs.cpsr = CPSR_COND_ZERO_MASK |
		    CPSR_ASYNC_ABORT_DISABLED | CPSR_MODE_USER;
	} else {
		vcpu->uregs.cpsr = CPSR_COND_ZERO_MASK |
		    CPSR_ASYNC_ABORT_DISABLED | CPSR_MODE_SUPERVISOR;
		vcpu->uregs.sp = (u32) vmm_malloc(0x800) + 0x800 - 0x100;
	}
	/* Initialize Supervisor Mode Registers */
	/* For only Normal VCPUs */
	if (!vcpu->guest) {
		return VMM_OK;
	}
	if (!vcpu->reset_count) {
		vmm_memset(&vcpu->sregs, 0, sizeof(vmm_super_regs_t));
		vcpu->sregs.cpsr = CPSR_ASYNC_ABORT_DISABLED | 
				   CPSR_IRQ_DISABLED |
				   CPSR_FIQ_DISABLED | 
				   CPSR_MODE_SUPERVISOR;
	} else {
		for (ite = 0; ite < CPU_FIQ_GPR_COUNT; ite++) {
			vcpu->sregs.gpr_usr[ite] = 0x0;
			vcpu->sregs.gpr_fiq[ite] = 0x0;
		}
		vcpu->sregs.sp_usr = 0x0;
		vcpu->sregs.lr_usr = 0x0;
		vcpu->sregs.sp_svc = 0x0;
		vcpu->sregs.lr_svc = 0x0;
		vcpu->sregs.spsr_svc = 0x0;
		vcpu->sregs.sp_mon = 0x0;
		vcpu->sregs.lr_mon = 0x0;
		vcpu->sregs.spsr_mon = 0x0;
		vcpu->sregs.sp_abt = 0x0;
		vcpu->sregs.lr_abt = 0x0;
		vcpu->sregs.spsr_abt = 0x0;
		vcpu->sregs.sp_und = 0x0;
		vcpu->sregs.lr_und = 0x0;
		vcpu->sregs.spsr_und = 0x0;
		vcpu->sregs.sp_irq = 0x0;
		vcpu->sregs.lr_irq = 0x0;
		vcpu->sregs.spsr_irq = 0x0;
		vcpu->sregs.sp_fiq = 0x0;
		vcpu->sregs.lr_fiq = 0x0;
		vcpu->sregs.spsr_fiq = 0x0;
		cpu_vcpu_cpsr_update(vcpu, &vcpu->uregs, (CPSR_COND_ZERO_MASK |
						  CPSR_ASYNC_ABORT_DISABLED | 
						  CPSR_IRQ_DISABLED |
						  CPSR_FIQ_DISABLED | 
						  CPSR_MODE_SUPERVISOR));
	}
	if (!vcpu->reset_count) {
		vcpu->sregs.features = 0;
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
	return cpu_vcpu_cp15_init(vcpu, cpuid);
}

void vmm_vcpu_regs_switch(vmm_vcpu_t * tvcpu,
			  vmm_vcpu_t * vcpu, vmm_user_regs_t * regs)
{
	u32 ite;
	/* Save user registers & banked registers */
	if (tvcpu) {
		tvcpu->uregs.pc = regs->pc;
		tvcpu->uregs.lr = regs->lr;
		tvcpu->uregs.sp = regs->sp;
		for (ite = 0; ite < CPU_GPR_COUNT; ite++) {
			tvcpu->uregs.gpr[ite] = regs->gpr[ite];
		}
		tvcpu->uregs.cpsr = regs->cpsr;
		if(tvcpu->guest) {
			cpu_vcpu_banked_regs_save(tvcpu, regs);
		}
	}
	if (vcpu->guest) {
		/* Switch CP15 context */
		cpu_vcpu_cp15_context_switch(tvcpu, vcpu, regs);
	}
	/* Restore user registers & banked registers */
	regs->pc = vcpu->uregs.pc;
	regs->lr = vcpu->uregs.lr;
	regs->sp = vcpu->uregs.sp;
	for (ite = 0; ite < CPU_GPR_COUNT; ite++) {
		regs->gpr[ite] = vcpu->uregs.gpr[ite];
	}
	regs->cpsr = vcpu->uregs.cpsr;
	if (vcpu->guest) {
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
				cpu_vcpu_cpsr_retrive(vcpu, regs));
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
	cpu_vcpu_dump_user_reg(vcpu, &vcpu->uregs);
	/* For only Normal VCPUs */
	if (!vcpu->guest) {
		return;
	}
	vmm_printf("  User Mode Registers (Banked)\n");
	vmm_printf("    SP=0x%08x       LR=0x%08x\n",
		   vcpu->sregs.sp_usr, vcpu->sregs.lr_usr);
	vmm_printf("  Supervisor Mode Registers (Banked)\n");
	vmm_printf("    SP=0x%08x       LR=0x%08x       SPSR=0x%08x\n",
		   vcpu->sregs.sp_svc, vcpu->sregs.lr_svc,
		   vcpu->sregs.spsr_svc);
	vmm_printf("  Monitor Mode Registers (Banked)\n");
	vmm_printf("    SP=0x%08x       LR=0x%08x       SPSR=0x%08x\n",
		   vcpu->sregs.sp_mon, vcpu->sregs.lr_mon,
		   vcpu->sregs.spsr_mon);
	vmm_printf("  Abort Mode Registers (Banked)\n");
	vmm_printf("    SP=0x%08x       LR=0x%08x       SPSR=0x%08x\n",
		   vcpu->sregs.sp_abt, vcpu->sregs.lr_abt,
		   vcpu->sregs.spsr_abt);
	vmm_printf("  Undefined Mode Registers (Banked)\n");
	vmm_printf("    SP=0x%08x       LR=0x%08x       SPSR=0x%08x\n",
		   vcpu->sregs.sp_und, vcpu->sregs.lr_und,
		   vcpu->sregs.spsr_und);
	vmm_printf("  IRQ Mode Registers (Banked)\n");
	vmm_printf("    SP=0x%08x       LR=0x%08x       SPSR=0x%08x\n",
		   vcpu->sregs.sp_irq, vcpu->sregs.lr_irq,
		   vcpu->sregs.spsr_irq);
	vmm_printf("  FIQ Mode Registers (Banked)\n");
	vmm_printf("    SP=0x%08x       LR=0x%08x       SPSR=0x%08x",
		   vcpu->sregs.sp_fiq, vcpu->sregs.lr_fiq,
		   vcpu->sregs.spsr_fiq);
	for (ite = 0; ite < 5; ite++) {
		if (ite % 3 == 0)
			vmm_printf("\n");
		vmm_printf("    R%02d=0x%08x  ", (ite + 8),
			   vcpu->sregs.gpr_fiq[ite]);
	}
	vmm_printf("\n");
}

