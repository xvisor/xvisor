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
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for vcpu interrupts
 */
#ifndef _VMM_VCPU_IRQ_H__
#define _VMM_VCPU_IRQ_H__

#include <vmm_types.h>
#include <vmm_manager.h>

/** Process interrupts for current vcpu */
void vmm_vcpu_irq_process(arch_regs_t * regs);

/** Assert an irq to given vcpu */
void vmm_vcpu_irq_assert(struct vmm_vcpu *vcpu, u32 irq_no, u32 reason);

/** Deassert active irq of given vcpu */
void vmm_vcpu_irq_deassert(struct vmm_vcpu *vcpu);

/** Wait for irq on given vcpu */
int vmm_vcpu_irq_wait(struct vmm_vcpu *vcpu);

/** Intialize interrupts for given vcpu */
int vmm_vcpu_irq_init(struct vmm_vcpu *vcpu);

#endif
