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

/** Architecture specific VCPU Initialization */
int arch_vcpu_init(struct vmm_vcpu *vcpu);

/** Architecture specific VCPU De-initialization (or Cleanup) */
int arch_vcpu_deinit(struct vmm_vcpu *vcpu);

/** VCPU context switch function 
 *  NOTE: The tvcpu pointer is VCPU being switched out.
 *  NOTE: The vcpu pointer is VCPU being switched in.
 *  NOTE: The pointer to arch_regs_t represents register state
 *  saved by interrupt handlers or arch_vcpu_preempt_orphan().
 */
void arch_vcpu_switch(struct vmm_vcpu *tvcpu,
		      struct vmm_vcpu *vcpu, 
		      arch_regs_t *regs);

/** Forcefully preempt current Orphan VCPU (or current Thread) 
 *  NOTE: This arch function is optional.
 *  NOTE: This functions is always called with irqs saved
 *  on the stack of current Orphan VCPU.
 *  NOTE: The core code expects that this function will save
 *  context and call vmm_scheduler_preempt_orphan() with a
 *  pointer to saved arch_regs_t.
 *  NOTE: If arch implments this function then arch_config.h
 *  will define ARCH_HAS_VCPU_PREEMPT_ORPHAN feature.
 */
void arch_vcpu_preempt_orphan(void);

/** Print architecture specific registers of a VCPU */
void arch_vcpu_regs_dump(struct vmm_vcpu *vcpu);

/** Print architecture specific stats for a VCPU */
void arch_vcpu_stat_dump(struct vmm_vcpu *vcpu);

/** Get count of VCPU interrupts */
u32 arch_vcpu_irq_count(struct vmm_vcpu *vcpu);

/** Get priority for given VCPU interrupt number */
u32 arch_vcpu_irq_priority(struct vmm_vcpu *vcpu, u32 irq_no);

/** Assert VCPU interrupt 
 *  NOTE: This functions is called asynchronusly in any context.
 *  NOTE: This function is usually useful to architectures having
 *  hardware virtualization support.
 */
int arch_vcpu_irq_assert(struct vmm_vcpu *vcpu, u32 irq_no, u32 reason);

/** Execute VCPU interrupt 
 *  NOTE: This functions is always called in context of the VCPU (i.e.
 *  in Normal context).
 *  NOTE: This function is usually useful to architectures not having
 *  hardware virtualization support.
 */
int arch_vcpu_irq_execute(struct vmm_vcpu *vcpu, 
			  arch_regs_t *regs,
			  u32 irq_no, u32 reason);

/** Deassert VCPU interrupt 
 *  NOTE: This functions is called asynchronusly in any context.
 *  NOTE: This function is usually useful to architectures having
 *  hardware virtualization support.
 */
int arch_vcpu_irq_deassert(struct vmm_vcpu * vcpu, u32 irq_no, u32 reason);

#endif
