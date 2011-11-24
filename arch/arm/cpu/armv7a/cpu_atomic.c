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
 * @version 0.01
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief ARM specific synchronization mechanisms.
 */

#include <vmm_error.h>
#include <cpu_atomic.h>

void __lock __cpu_atomic_inc(atomic_t * atom)
{

	unsigned int tmp;
	unsigned int result;

	__asm__ __volatile__("@ atomic_add\n" "1:     ldrex   %0, [%3]\n"	/* Load atom->counter([%3]) to tmp (%0) */
			     "       add     %0, %0, #1\n"	/* Add 1 to tmp */
			     "       strex   %1, %0, [%2]\n"	/* Save tmp (%0) to atom->counter
								 * Result of this operation will be in result i.e. 
								 * if store operation success result will be 0 or else 1
								 */
			     "       teq     %1, #0\n"	/* Compare result with 0 */
			     "       bne     1b"	/* If fails go back to 1 and retry else return */
			     :"=&r"(tmp), "=&r"(result), "=&r"(atom->counter)
			     :"r"(&atom->counter)
			     :"cc");
}

void __lock __cpu_atomic_dec(atomic_t * atom)
{
	unsigned int tmp;
	unsigned int result;

	__asm__ __volatile__("@ atomic_add\n" "1:     ldrex   %0, [%3]\n"	/* Load atom->counter([%3]) to tmp (%0) */
			     "       sub     %0, %0, #1\n"	/* Sub to tmp   */
			     "       strex   %1, %0, [%2]\n"	/* Save tmp (%0) to atom->counter
								 * Result of this operation will be in result i.e. 
								 * if store operation success result will be 0 or else 1
								 */
			     "       teq     %1, #0\n"	/* Compare result with 0 */
			     "       bne     1b"	/* If fails go back to 1 and retry else return */
			     :"=&r"(tmp), "=&r"(result), "=&r"(atom->counter)
			     :"r"(&atom->counter)
			     :"cc");

}

void __lock vmm_cpu_atomic_inc(atomic_t * atom)
{
	__cpu_atomic_inc(atom);
}

void __lock vmm_cpu_atomic_dec(atomic_t * atom)
{
	__cpu_atomic_dec(atom);
}
