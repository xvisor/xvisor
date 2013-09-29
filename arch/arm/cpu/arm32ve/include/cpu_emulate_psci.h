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
 * @file cpu_emulate_psci.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief CPU specific functions for PSCI Emulation
 */

#ifndef __CPU_EMULATE_PSCI_H__
#define __CPU_EMULATE_PSCI_H__

#include <vmm_types.h>
#include <cpu_defines.h>
#include <cpu_vcpu_helper.h>

static inline unsigned long emulate_psci_get_reg(struct vmm_vcpu *vcpu,
						 arch_regs_t *regs, u32 reg)
{
	return (unsigned long)cpu_vcpu_reg_read(vcpu, regs, reg);
}

static inline void emulate_psci_set_reg(struct vmm_vcpu *vcpu,
					arch_regs_t *regs, 
					u32 reg, unsigned long val)
{
	cpu_vcpu_reg_write(vcpu, regs, reg, (u32)val);
}

static inline void emulate_psci_set_pc(struct vmm_vcpu *vcpu,
					arch_regs_t *regs, unsigned long val)
{
	/* Handle Thumb2 entry point */
	if (val & 1) {
		val &= ~((unsigned long) 1);
		regs->cpsr |= CPSR_THUMB_ENABLED;
	}

	regs->pc = (u32)val;
}

#endif	/* __CPU_EMULATE_PSCI_H__ */
