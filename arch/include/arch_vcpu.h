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
 * @file arch_vcpu.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief generic interface for arch specific VCPU operations
 */
#ifndef _ARCH_VCPU_H__
#define _ARCH_VCPU_H__

#include <vmm_types.h>
#include <vmm_manager.h>

/** Register functions required by VMM core */
int arch_vcpu_regs_init(struct vmm_vcpu * vcpu);
int arch_vcpu_regs_deinit(struct vmm_vcpu * vcpu);
void arch_vcpu_regs_switch(struct vmm_vcpu * tvcpu,
			  struct vmm_vcpu * vcpu, 
			  arch_regs_t * regs);
void arch_vcpu_regs_dump(struct vmm_vcpu * vcpu);
void arch_vcpu_stat_dump(struct vmm_vcpu * vcpu);

/** VCPU Interrupt functions required by VMM core */
u32 arch_vcpu_irq_count(struct vmm_vcpu * vcpu);
u32 arch_vcpu_irq_priority(struct vmm_vcpu * vcpu, u32 irq_no);
int arch_vcpu_irq_assert(struct vmm_vcpu * vcpu, u32 irq_no, u32 reason);
int arch_vcpu_irq_execute(struct vmm_vcpu * vcpu, 
			 arch_regs_t * regs,
			 u32 irq_no, u32 reason);
int arch_vcpu_irq_deassert(struct vmm_vcpu * vcpu, u32 irq_no, u32 reason);

#endif
