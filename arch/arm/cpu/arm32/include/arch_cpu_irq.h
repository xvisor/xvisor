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

/** Setup IRQ for primary CPU */
int arch_cpu_irq_primary_setup(void);

/** Setup IRQ for secondary CPU */
int arch_cpu_irq_secondary_setup(void);

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
#if defined(CONFIG_ARMV5)
#define arch_cpu_wait_for_irq()		do { \
					unsigned long _tr0, _tr1, _tr2, _tr3, _tip; \
					asm volatile ( \
					"mov     %0, #0\n" \
					"mrc     p15, 0, %1, c1, c0, 0   @ Read control register\n" \
					"mcr     p15, 0, %0, c7, c10, 4  @ Drain write buffer\n" \
					"bic     %2, %1, #1 << 12\n" \
					"mrs     %3, cpsr                @ Disable FIQs while Icache\n" \
					"orr     %4, %3, #0x00000040     @ is disabled\n" \
					"msr     cpsr_c, %4\n" \
					"mcr     p15, 0, %2, c1, c0, 0   @ Disable I cache\n" \
					"mcr     p15, 0, %0, c7, c0, 4   @ Wait for interrupt\n" \
					"mcr     p15, 0, %1, c1, c0, 0   @ Restore ICache enable\n" \
					"msr     cpsr_c, %3              @ Restore FIQ state" \
					:"=r" (_tr0), "=r" (_tr1), "=r" (_tr2), "=r" (_tr3), "=r" (_tip)::"memory", "cc" ); \
					} while (0)
#else
#define arch_cpu_wait_for_irq()		do { \
					asm volatile (" wfi "); \
					} while (0)
#endif

#endif
