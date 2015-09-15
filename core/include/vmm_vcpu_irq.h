/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_vcpu_irq.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for vcpu interrupts
 */
#ifndef _VMM_VCPU_IRQ_H__
#define _VMM_VCPU_IRQ_H__

#include <vmm_types.h>
#include <vmm_manager.h>

/** Process interrupts for current vcpu 
 *  Note: Don't call this function directly it's meant to be called
 *  from vmm_scheduler only.
 */
void vmm_vcpu_irq_process(struct vmm_vcpu *vcpu, arch_regs_t *regs);

/** Assert an irq to given vcpu */
void vmm_vcpu_irq_assert(struct vmm_vcpu *vcpu, u32 irq_no, u64 reason);

/** Deassert active irq of given vcpu */
void vmm_vcpu_irq_deassert(struct vmm_vcpu *vcpu, u32 irq_no);

/** Forcefully resume given VCPU if waiting for irq */
int vmm_vcpu_irq_wait_resume(struct vmm_vcpu *vcpu, bool use_async_ipi);

/** Wait for irq on given vcpu with some timeout */
int vmm_vcpu_irq_wait_timeout(struct vmm_vcpu *vcpu, u64 nsecs);

/** Wait for irq on given vcpu indefinetly (no timeout) */
#define vmm_vcpu_irq_wait(vcpu)	vmm_vcpu_irq_wait_timeout(vcpu, 0)

/** Current state of Wait for irq on given vcpu */
bool vmm_vcpu_irq_wait_state(struct vmm_vcpu *vcpu);

/** Initialize interrupts for given vcpu */
int vmm_vcpu_irq_init(struct vmm_vcpu *vcpu);

/** Deinitialize interrupts for given vcpu */
int vmm_vcpu_irq_deinit(struct vmm_vcpu *vcpu);

#endif
