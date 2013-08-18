/**
 * Copyright (c) 2013 Jean-Christophe Dubois
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
 * @file cpu_atomic64.c
 * @author Jean-Christophe Dubois <jcd@tribudubois.net>
 * @brief ARM specific 64 bits synchronization mechanisms.
 *
 * derived from linux/arch/arm/include/asm/atomic.h
 *
 *  Copyright (C) 1996 Russell King.
 *  Copyright (C) 2002 Deep Blue Solutions Ltd.
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_compiler.h>
#include <arch_cpu_irq.h>
#include <arch_barrier.h>
#include <arch_atomic64.h>

u64 __lock arch_atomic64_read(atomic64_t *atom)
{
	u64 result;

	__asm__ __volatile__("@ atomic64_read\n"
"	ldrexd    %0, %H0, [%1]"
	: "=&r" (result)
	: "r" (&atom->counter), "Qo" (atom->counter)
	);

	return result;
}

void __lock arch_atomic64_write(atomic64_t *atom, u64 value)
{
	u64 tmp;

	__asm__ __volatile__("@ atomic64_write\n"
"1:	ldrexd  %0, %H0, [%2]\n"
"	strexd  %0, %3, %H3, [%2]\n"
"	teq     %0, #0\n"
"	bne     1b"
	: "=&r" (tmp), "=Qo" (atom->counter)
	: "r" (&atom->counter), "r" (value)
	: "cc");
}

void __lock arch_atomic64_add(atomic64_t *atom, u64 value)
{
	u64 result;
	unsigned long tmp;

	__asm__ __volatile__("@ atomic64_add\n"
"1:	ldrexd  %0, %H0, [%3]\n"
"	adds    %0, %0, %4\n"
"	adc     %H0, %H0, %H4\n"
"	strexd  %1, %0, %H0, [%3]\n"
"	teq     %1, #0\n"
"	bne     1b"
	: "=&r" (result), "=&r" (tmp), "+Qo" (atom->counter)
	: "r" (&atom->counter), "r" (value)
	: "cc");
}

void __lock arch_atomic64_sub(atomic64_t * atom, u64 value)
{
	u64 result;
	unsigned long tmp;

	__asm__ __volatile__("@ atomic64_sub\n"
"1:	ldrexd  %0, %H0, [%3]\n"
"	subs    %0, %0, %4\n"
"	sbc     %H0, %H0, %H4\n"
"	strexd  %1, %0, %H0, [%3]\n"
"	teq     %1, #0\n"
"	bne     1b"
	: "=&r" (result), "=&r" (tmp), "+Qo" (atom->counter)
	: "r" (&atom->counter), "r" (value)
	: "cc");
}

bool __lock arch_atomic64_testnset(atomic64_t * atom, u64 test, u64 value)
{
	u64 oldval;
	unsigned long res;

	arch_smp_mb();

	do {
		__asm__ __volatile__("@ atomic64_testnset\n"
		"ldrexd		%1, %H1, [%3]\n"	/* load oldval */
		"mov		%0, #0\n"		/* init res with 0 */
		"teq		%1, %4\n"		/* compare test and oldval low bytes */
		"teqeq		%H1, %H4\n"		/* compare test and oldval high bytes */
		"strexdeq	%0, %5, %H5, [%3]"	/* store value in atomic if equal */
		: "=&r" (res), "=&r" (oldval), "+Qo" (atom->counter)
		: "r" (&atom->counter), "r" (test), "r" (value)
		: "cc");
	} while (res);

	arch_smp_mb();

	return oldval == test;
}

u64 __lock arch_atomic64_add_return(atomic64_t * atom, u64 value)
{
	u64 result;
	unsigned long tmp;

	arch_smp_mb();

	__asm__ __volatile__("@ atomic64_add_return\n"
"1:	ldrexd  %0, %H0, [%3]\n"
"	adds    %0, %0, %4\n"
"	adc     %H0, %H0, %H4\n"
"	strexd  %1, %0, %H0, [%3]\n"
"	teq     %1, #0\n"
"	bne     1b"
	: "=&r" (result), "=&r" (tmp), "+Qo" (atom->counter)
	: "r" (&atom->counter), "r" (value)
	: "cc");

	arch_smp_mb();

	return result;
}

u64 __lock arch_atomic64_sub_return(atomic64_t * atom, u64 value)
{
	u64 result;
	unsigned long tmp;

	arch_smp_mb();

	__asm__ __volatile__("@ atomic64_sub_return\n"
"1:	ldrexd  %0, %H0, [%3]\n"
"	subs    %0, %0, %4\n"
"	sbc     %H0, %H0, %H4\n"
"	strexd  %1, %0, %H0, [%3]\n"
"	teq     %1, #0\n"
"	bne     1b"
	: "=&r" (result), "=&r" (tmp), "+Qo" (atom->counter)
	: "r" (&atom->counter), "r" (value)
	: "cc");

	arch_smp_mb();

	return result;
}
