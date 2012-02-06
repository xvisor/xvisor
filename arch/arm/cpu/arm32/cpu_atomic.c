/**
 * Copyright (c) 2011 Pranav Sawargaonkar.
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
 * @file cpu_atomic.c
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @author Jean-Christophe Dubois <jcd@tribudubois.net>
 * @brief ARM specific synchronization mechanisms.
 */

#include <vmm_error.h>
#include <vmm_types.h>

/* FIXME: Need memory barrier for this. */
long __lock arch_cpu_atomic_read(atomic_t * atom)
{
	return atom->counter;
}

/* FIXME: Need memory barrier for this. */
void __lock arch_cpu_atomic_write(atomic_t * atom, long value)
{
	atom->counter = value;
}

static void __lock __cpu_atomic_add(atomic_t * atom, long value)
{

	unsigned int tmp;
	long result;

	__asm__ __volatile__("@ atomic_add\n"
"1:     ldrex   %0, [%3]\n"	/* Load atom->counter(%3) to result (%0) */
"	add     %0, %0, %4\n"	/* Add value (%4) to result */
"	strex   %1, %0, [%3]\n"	/* Save result (%0) to atom->counter (%3)
				 * Result of this operation will be in tmp (%1) 
				 * if store operation success tmp is 0 or else 1
				 */
"	teq     %1, #0\n"	/* Compare tmp (%1) result with 0 */
"	bne     1b"	/* If fails go back to 1 and retry else return */
	:"=&r"(result), "=&r"(tmp), "=&r"(atom->counter)
	:"r"(&atom->counter), "Ir"(value)
	:"cc");
}

static void __lock __cpu_atomic_sub(atomic_t * atom, long value)
{
	unsigned int tmp;
	long result;

	__asm__ __volatile__("@ atomic_sub\n"
"1:     ldrex   %0, [%3]\n"	/* Load atom->counter(%3) to result (%0) */
"	sub     %0, %0, %4\n"	/* Substract value (%4) to result (%0) */
"	strex   %1, %0, [%3]\n"	/* Save result (%0) to atom->counter (%3)
				 * Result of this operation will be in tmp (%1) 
				 * if store operation success tmp (%1) is 0
				 */
"	teq     %1, #0\n"	/* Compare tmp (%1) result with 0 */
"	bne     1b"	/* If fails go back to 1 and retry else return */
	:"=&r"(result), "=&r"(tmp), "=&r"(atom->counter)
	:"r"(&atom->counter), "Ir"(value)
	:"cc");
}

static bool __lock __cpu_atomic_testnset(atomic_t * atom, long test, long value)
{
	unsigned int tmp;
	long previous;

	__asm__ __volatile__("@ atomic_testnset\n"
"1:	ldrex  %1, [%3]\n"	/* load atom->counter(%3) to previous (%1) */
"	mov	%0, #0\n"	/* load 0 to tmp (%0) */
"	teq	%1, %4\n"	/* compare previous (%1) and test (%4) */
"	strexeq %0, %5, [%3]\n"	/* if equal, store value (%5) to atom->counter
				 * result of the operation is in tmp (%0)
				 */
"	teq     %0, #0\n"	/* Compare tmp (%0) result with 0 */
"	bne     1b"		/* If fails go back to 1 and retry */
	:"=&r"(tmp), "=&r"(previous), "=&r"(atom->counter)
	:"r"(&atom->counter), "Ir"(test), "r"(value)
	:"cc");

	return (previous == test);
}

static long __lock __cpu_atomic_add_return(atomic_t * atom, long value)
{
	unsigned int tmp;
	long result;

	__asm__ __volatile__("@ atomic_add_return\n"
"1:     ldrex   %0, [%3]\n"	/* Load atom->counter(%3) to result (%0) */
"	add     %0, %0, %4\n"	/* Add value (%4) to result */
"	strex   %1, %0, [%3]\n"	/* Save result (%0) to atom->counter (%3)
				 * Result of this operation will be in tmp (%1) 
				 * if store operation success tmp is 0 or else 1
				 */
"	teq     %1, #0\n"	/* Compare tmp (%1) result with 0 */
"	bne     1b"	/* If fails go back to 1 and retry else return */
	:"=&r"(result), "=&r"(tmp), "=&r"(atom->counter)
	:"r"(&atom->counter), "Ir"(value)
	:"cc");

	return result;
}

static long __lock __cpu_atomic_sub_return(atomic_t * atom, long value)
{
	unsigned int tmp;
	long result;

	__asm__ __volatile__("@ atomic_sub_return\n"
"1:     ldrex   %0, [%3]\n"	/* Load atom->counter(%3) to result (%0) */
"	sub     %0, %0, %4\n"	/* Substract value (%4) to result (%0) */
"	strex   %1, %0, [%3]\n"	/* Save result (%0) to atom->counter (%3)
				 * Result of this operation will be in tmp (%1) 
				 * if store operation success tmp is 0 or else 1
				 */
"	teq     %1, #0\n"	/* Compare tmp (%1) result with 0 */
"	bne     1b"	/* If fails go back to 1 and retry else return */
	:"=&r"(result), "=&r"(tmp), "=&r"(atom->counter)
	:"r"(&atom->counter), "Ir"(value)
	:"cc");

	return result;
}

void __lock arch_cpu_atomic_add(atomic_t * atom, long value)
{
	__cpu_atomic_add(atom, value);
}

void __lock arch_cpu_atomic_sub(atomic_t * atom, long value)
{
	__cpu_atomic_sub(atom, value);
}

long __lock arch_cpu_atomic_add_return(atomic_t * atom, long value)
{
	return __cpu_atomic_add_return(atom, value);
}

long __lock arch_cpu_atomic_sub_return(atomic_t * atom, long value)
{
	return __cpu_atomic_sub_return(atom, value);
}

bool __lock arch_cpu_atomic_testnset(atomic_t * atom, long test, long value)
{
	return __cpu_atomic_testnset(atom, test, value);
}
