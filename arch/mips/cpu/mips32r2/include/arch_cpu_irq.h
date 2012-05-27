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
 * @file arch_cpu_irq.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief interface for controlling CPU IRQs
 */
#ifndef _ARCH_CPU_IRQ_H__
#define _ARCH_CPU_IRQ_H__

#include <vmm_types.h>

/** CPU IRQ functions required by VMM core */
int arch_cpu_irq_setup(void);
void arch_cpu_irq_enable(void);
void arch_cpu_irq_disable(void);
irq_flags_t arch_cpu_irq_save(void);
void arch_cpu_irq_restore(irq_flags_t flags);
void arch_cpu_wait_for_irq(void);

#endif
