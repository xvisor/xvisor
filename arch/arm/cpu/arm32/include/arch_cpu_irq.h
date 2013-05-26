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
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief interface for controlling CPU IRQs
 */
#ifndef _ARCH_CPU_IRQ_H__
#define _ARCH_CPU_IRQ_H__

#include <vmm_types.h>
#include <cpu_defines.h>
#include <cpu_proc.h>

/** Setup IRQ for CPU */
int arch_cpu_irq_setup(void);

/** Enable IRQ
 *  Prototype: void arch_cpu_irq_enable(void); 
 */
#if defined(CONFIG_ARMV5)
#define arch_cpu_irq_enable()	do { \
				unsigned long _tf; \
				asm volatile( \
				"mrs	%0, cpsr\n" \
				"bic	%0, %0, #128\n" \
				"msr	cpsr_c, %0" \
				: "=r" (_tf) : : "memory", "cc"); \
				} while (0)
#else
#define arch_cpu_irq_enable()	do { \
				asm volatile ("cpsie i"); \
				} while (0)
#endif

/** Disable IRQ
 *  Prototype: void arch_cpu_irq_disable(void); 
 */
#if defined(CONFIG_ARMV5)
#define arch_cpu_irq_disable()	do { \
				unsigned long _tf; \
				asm volatile( \
				"mrs	%0, cpsr\n" \
				"orr	%0, %0, #128\n" \
				"msr	cpsr_c, %0" \
				: "=r" (_tf) : : "memory", "cc"); \
				} while (0)
#else
#define arch_cpu_irq_disable()	do { \
				asm volatile ("cpsid i"); \
				} while (0)
#endif

/** Check whether IRQs are disabled
 *  Prototype: bool arch_cpu_irq_disabled(void); 
 */
#define arch_cpu_irq_disabled() ({ unsigned long _tf; \
				asm volatile ( \
				"mrs     %0, cpsr\n\t" \
				:"=r" (_tf) : : "memory", "cc"); \
				(_tf & CPSR_IRQ_DISABLED) ? TRUE : FALSE; \
				})

/** Save IRQ flags and disable IRQ
 *  Prototype: void arch_cpu_irq_save(irq_flags_t flags);
 */
#if defined(CONFIG_ARMV5)
#define arch_cpu_irq_save(flags)	do { unsigned long _tt; \
					asm volatile( \
					"mrs	%0, cpsr\n" \
					"orr	%1, %0, #128\n" \
					"msr	cpsr_c, %1" \
					: "=r" ((flags)), "=r" (_tt) \
					: : "memory", "cc"); \
					} while (0)
#else
#define arch_cpu_irq_save(flags)	do { \
					asm volatile ( \
					"mrs     %0, cpsr\n\t" \
					"cpsid   i\n\t" \
					:"=r" ((flags)) : : "memory", "cc"); \
					} while (0)
#endif

/** Restore IRQ flags
 *  Prototype: void arch_cpu_irq_restore(irq_flags_t flags);
 */
#define arch_cpu_irq_restore(flags)	do { \
					asm volatile ( \
					"msr     cpsr_c, %0" \
					: : "r" ((flags)) : "memory", "cc"); \
					} while (0)

/** Wait for IRQ
 *  Prototype: void arch_cpu_wait_for_irq(void);
 */
#define arch_cpu_wait_for_irq()		do { \
					irq_flags_t _ifl; \
					arch_cpu_irq_save(_ifl); \
					proc_do_idle(); \
					arch_cpu_irq_restore(_ifl); \
					} while (0)

#endif
