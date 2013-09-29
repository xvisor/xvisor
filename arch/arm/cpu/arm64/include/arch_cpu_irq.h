/**
 * Copyright (c) 2013 Sukanto Ghosh.
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
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief interface for controlling CPU IRQs
 */
#ifndef _ARCH_CPU_IRQ_H__
#define _ARCH_CPU_IRQ_H__

#include <vmm_types.h>
#include <cpu_defines.h>

/** Setup IRQ for CPU */
int arch_cpu_irq_setup(void);

/** Enable IRQ
 *  Prototype: void arch_cpu_irq_enable(void); 
 */
#define arch_cpu_irq_enable()	do { \
				asm volatile("msr daifclr, #2":::"memory"); \
				} while (0)

/** Disable IRQ
 *  Prototype: void arch_cpu_irq_disable(void); 
 */
#define arch_cpu_irq_disable()	do { \
				asm volatile("msr daifset, #2":::"memory"); \
				} while (0)

/** Check whether IRQs are disabled
 *  Prototype: bool arch_cpu_irq_disabled(void); 
 */
#define arch_cpu_irq_disabled()	({ unsigned long __flgs;	\
				   asm volatile (" mrs %0, daif" \
			           :"=r" (__flgs)::"memory", "cc"); \
				   (__flgs & PSR_IRQ_DISABLED) ? TRUE : FALSE; \
				})


/** Save IRQ flags and disable IRQ
 *  Prototype: void arch_cpu_irq_save(irq_flags_t flags);
 */
#define arch_cpu_irq_save(flags)	do { \
					asm volatile ( \
					"mrs  %0, daif\n\t" \
					"msr  daifset, #2" \
					:"=r" (flags)::"memory", "cc"); \
					} while(0)

/** Restore IRQ flags
 *  Prototype: void arch_cpu_irq_restore(irq_flags_t flags);
 */
#define arch_cpu_irq_restore(flags)	do { \
					asm volatile ( \
					"msr  daif, %0"::"r" (flags) \
					:"memory", "cc"); \
					} while(0)

/** Wait for IRQ
 *  Prototype: void arch_cpu_wait_for_irq(void);
 */
#define arch_cpu_wait_for_irq()		do { \
					asm volatile ("wfi"); \
					} while (0)

#endif
