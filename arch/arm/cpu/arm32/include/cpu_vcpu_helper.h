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
 * @file cpu_vcpu_helper.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief header of VCPU helper functions 
 */
#ifndef _CPU_VCPU_HELPER_H__
#define _CPU_VCPU_HELPER_H__

#include <vmm_types.h>
#include <vmm_manager.h>

/* Function to halt VCPU */
void cpu_vcpu_halt(struct vmm_vcpu * vcpu, arch_regs_t * regs);

/* Function to retrieve CPSR of a VCPU */
u32 cpu_vcpu_cpsr_retrieve(struct vmm_vcpu * vcpu,
			  arch_regs_t * regs);

/* Function to update CPSR of a VCPU */
void cpu_vcpu_cpsr_update(struct vmm_vcpu * vcpu, 
			  arch_regs_t * regs,
			  u32 new_cpsr,
			  u32 new_cpsr_mask);

/* Function to save banked registers of a VCPU */
void cpu_vcpu_banked_regs_save(struct vmm_vcpu * vcpu, 
			       arch_regs_t * src);

/* Function to restore banked registers of a VCPU */
void cpu_vcpu_banked_regs_restore(struct vmm_vcpu * vcpu, 
				  arch_regs_t * dst);

/* Function to retrieve SPSR of a VCPU */
u32 cpu_vcpu_spsr_retrieve(struct vmm_vcpu * vcpu);

/* Function to update SPSR of a VCPU */
int cpu_vcpu_spsr_update(struct vmm_vcpu * vcpu, 
			 u32 new_spsr,
			 u32 new_spsr_update);

/* Function to read a VCPU register of current mode */
u32 cpu_vcpu_reg_read(struct vmm_vcpu * vcpu, 
		      arch_regs_t * regs, 
		      u32 reg_num);

/* Function to write a VCPU register of current mode */
void cpu_vcpu_reg_write(struct vmm_vcpu * vcpu, 
			arch_regs_t * regs, 
			u32 reg_num, 
			u32 reg_val);

/* Function to read a VCPU register of given mode */
u32 cpu_vcpu_regmode_read(struct vmm_vcpu * vcpu, 
			  arch_regs_t * regs, 
			  u32 mode,
			  u32 reg_num);

/* Function to write a VCPU register of given mode */
void cpu_vcpu_regmode_write(struct vmm_vcpu * vcpu, 
			     arch_regs_t * regs, 
			     u32 mode,
			     u32 reg_num,
			     u32 reg_val);

/* Function to dump user registers */
void cpu_vcpu_dump_user_reg(struct vmm_vcpu * vcpu, arch_regs_t * regs);

#endif
