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
 * @file vmm_host_irq.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for host interrupts
 */
#ifndef _VMM_HOST_IRQ_H__
#define _VMM_HOST_IRQ_H__

#include <vmm_types.h>
#include <arch_regs.h>

typedef int (*vmm_host_irq_handler_t) (u32 irq_no, 
					arch_regs_t * regs,
					void *dev);

/** Execute host interrupts */
int vmm_host_irq_exec(u32 cpu_irq_no, arch_regs_t * regs);

/** Check if a host irq is enabled */
bool vmm_host_irq_isenabled(u32 hirq_no);

/** Enable a host irq (by default all irqs are enabled) */
int vmm_host_irq_enable(u32 hirq_no);

/** Disable a host irq */
int vmm_host_irq_disable(u32 hirq_no);

/** Register handler for given irq */
int vmm_host_irq_register(u32 hirq_no, 
			  vmm_host_irq_handler_t handler,
			  void *dev);

/** Unregister handler for given irq */
int vmm_host_irq_unregister(u32 hirq_no, 
			    vmm_host_irq_handler_t handler);

/** Interrupts initialization function */
int vmm_host_irq_init(void);

#endif
