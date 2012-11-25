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
#include <cpu_asm_macros.h>

#ifdef CONFIG_I8259
#include <pics/i8259.h>
#endif

/** Setup IRQ for primary CPU */
int arch_cpu_irq_primary_setup(void);

/** Setup IRQ for secondary CPU */
int arch_cpu_irq_secondary_setup(void);

/** Enable IRQ
 *  Prototype: void arch_cpu_irq_enable(void); 
 */
#ifdef CONFIG_I8259
#define arch_cpu_irq_enable()	do { \
				__asm__ __volatile__("ei $0\n\t"); \
				i8259_enable_int(-1); \
				} while (0)
#else
#define arch_cpu_irq_enable()	do { \
				__asm__ __volatile__("ei $0\n\t"); \
				} while (0)
#endif

/** Disable IRQ
 *  Prototype: void arch_cpu_irq_disable(void); 
 */
#ifdef CONFIG_I8259
#define arch_cpu_irq_disable()	do { \
				__asm__ __volatile__("di $0\n\t"); \
				i8259_disable_int(-1); \
				} while (0)
#else
#define arch_cpu_irq_disable()	do { \
				__asm__ __volatile__("di $0\n\t"); \
				} while (0)
#endif

/** FIXME: Check whether IRQs are disabled
 *  Prototype: bool arch_cpu_irq_disabled(void); 
 */
#define arch_cpu_irq_disabled()	FALSE

/** FIXME: Save IRQ flags and disable IRQ
 *  Prototype: void arch_cpu_irq_save(irq_flags_t flags);
 */
#define arch_cpu_irq_save(flags)	do { \
					__asm__ __volatile__("di %0\n\t" \
					:"=r"((flags))); \
					flags &= (0x0000FF00UL); \
					} while (0)

/** FIXME: Restore IRQ flags
 *  Prototype: void arch_cpu_irq_restore(irq_flags_t flags);
 */
#define arch_cpu_irq_restore(flags)	do { irq_flags_t temp; \
					write_c0_status(read_c0_status() | (flags)); \
					__asm__ __volatile__("ei %0\n\t" \
					:"=r"(temp)); \
					} while (0)

/** FIXME: Wait for IRQ
 *  Prototype: void arch_cpu_wait_for_irq(void);
 */
#define arch_cpu_wait_for_irq()

#endif
