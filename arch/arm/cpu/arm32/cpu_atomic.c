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
#include <vmm_compiler.h>
#include <arch_cpu_irq.h>
#include <arch_barrier.h>

#if defined(CONFIG_ARMV5)

long __lock arch_atomic_read(atomic_t * atom)
{
	long ret = atom->counter;
	arch_rmb();
	return ret;
}

void __lock arch_atomic_write(atomic_t * atom, long value)
{
	atom->counter = value;
	arch_wmb();
}

void __lock arch_atomic_add(atomic_t * atom, long value)
{
	irq_flags_t flags;

	arch_cpu_irq_save(flags);
	atom->counter += value;
	arch_cpu_irq_restore(flags);
}

void __lock arch_atomic_sub(atomic_t * atom, long value)
{
	irq_flags_t flags;

	arch_cpu_irq_save(flags);
	atom->counter -= value;
	arch_cpu_irq_restore(flags);
}

bool __lock arch_atomic_testnset(atomic_t * atom, long test, long value)
{
	bool ret = FALSE;
	irq_flags_t flags;

	arch_cpu_irq_save(flags);
        if (atom->counter == test) {
		ret = TRUE;
                atom->counter = value;
	}
	arch_cpu_irq_restore(flags);

        return ret;
}

long __lock arch_atomic_add_return(atomic_t * atom, long value)
{
	long temp;
	irq_flags_t flags;

	arch_cpu_irq_save(flags);
	atom->counter += value;
	temp = atom->counter;
	arch_cpu_irq_restore(flags);

	return temp;
}

long __lock arch_atomic_sub_return(atomic_t * atom, long value)
{
	long temp;
	irq_flags_t flags;

	arch_cpu_irq_save(flags);
	atom->counter -= value;
	temp = atom->counter;
	arch_cpu_irq_restore(flags);

	return temp;
}

#else

long __lock arch_atomic_read(atomic_t * atom)
{
	long ret = atom->counter;
	arch_rmb();
	return ret;
}

void __lock arch_atomic_write(atomic_t * atom, long value)
{
	atom->counter = value;
	arch_wmb();
}

void __lock arch_atomic_add(atomic_t * atom, long value)
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
	:"=&r"(result), "=&r"(tmp), "+Qo"(atom->counter)
	:"r"(&atom->counter), "Ir"(value)
	:"cc");
}

void __lock arch_atomic_sub(atomic_t * atom, long value)
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
	:"=&r"(result), "=&r"(tmp), "+Qo"(atom->counter)
	:"r"(&atom->counter), "Ir"(value)
	:"cc");
}

bool __lock arch_atomic_testnset(atomic_t * atom, long test, long value)
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
	:"=&r"(tmp), "=&r"(previous), "+Qo"(atom->counter)
	:"r"(&atom->counter), "Ir"(test), "r"(value)
	:"cc");

	return (previous == test);
}

long __lock arch_atomic_add_return(atomic_t * atom, long value)
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
	:"=&r"(result), "=&r"(tmp), "+Qo"(atom->counter)
	:"r"(&atom->counter), "Ir"(value)
	:"cc");

	return result;
}

long __lock arch_atomic_sub_return(atomic_t * atom, long value)
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
	:"=&r"(result), "=&r"(tmp), "+Qo"(atom->counter)
	:"r"(&atom->counter), "Ir"(value)
	:"cc");

	return result;
}

#endif
