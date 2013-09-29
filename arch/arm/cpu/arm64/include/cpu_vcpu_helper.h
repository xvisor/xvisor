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
 * @file cpu_vcpu_helper.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header of VCPU helper functions 
 */
#ifndef _CPU_VCPU_HELPER_H__
#define _CPU_VCPU_HELPER_H__

#include <vmm_types.h>
#include <vmm_manager.h>

/* Function to halt VCPU */
void cpu_vcpu_halt(struct vmm_vcpu *vcpu, arch_regs_t *regs);

/* Function to read a 64bit VCPU register
 * Note: 0 <= reg_num <= 30
 * Note: For AArch32 mode, bits[63:32] of return value are always zero
 * Note: This function is required for HW-assisted emulation
 * of ARM instructions
 */
u64 cpu_vcpu_reg64_read(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs, 
			u32 reg_num);

/* Function to write a 64bit VCPU register
 * Note: 0 <= reg_num <= 30
 * Note: For AArch32 mode, bits[63:32] of register value are ignored
 * Note: This function is required for HW-assisted emulation
 * of ARM instructions
 */
void cpu_vcpu_reg64_write(struct vmm_vcpu *vcpu, 
			  arch_regs_t *regs, 
			  u32 reg_num, 
			  u64 reg_val);

/* Function to read a VCPU register based-on current mode
 * Note: For AArch32 mode, 0 <= reg_num <= 15
 * Note: For AArch64 mode, 0 <= reg_num <= 30
 * Note: This function is required for software emulation
 * of ARM instructions
 */
u64 cpu_vcpu_reg_read(struct vmm_vcpu *vcpu, 
		      arch_regs_t *regs, 
		      u32 reg_num);

/* Function to write a VCPU register based-on current mode
 * Note: For AArch32 mode, 0 <= reg_num <= 15
 * Note: For AArch64 mode, 0 <= reg_num <= 30
 * Note: This function is required for software emulation
 * of ARM instructions
 */
void cpu_vcpu_reg_write(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs, 
			u32 reg_num, 
			u64 reg_val);

/* Function to inject undef exception to a VCPU */
int cpu_vcpu_inject_undef(struct vmm_vcpu *vcpu,
			  arch_regs_t *regs);

/* Function to inject prefetch abort exception to a VCPU */
int cpu_vcpu_inject_pabt(struct vmm_vcpu *vcpu,
			 arch_regs_t *regs);

/* Function to inject data abort exception to a VCPU */
int cpu_vcpu_inject_dabt(struct vmm_vcpu *vcpu,
			 arch_regs_t *regs,
			 virtual_addr_t addr);

/* Function to dump user registers */
void cpu_vcpu_dump_user_reg(arch_regs_t *regs);

#endif
