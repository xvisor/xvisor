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
#include <arch_cpu_irq.h>
#include <cpu_barrier.h>

long __lock arch_cpu_atomic_read(atomic_t * atom)
{
	long ret = atom->counter;
	rmb();
	return ret;
}

void __lock arch_cpu_atomic_write(atomic_t * atom, long value)
{
	atom->counter = value;
	wmb();
}

void __lock arch_cpu_atomic_add(atomic_t * atom, long value)
{
	irq_flags_t flags;

	flags = arch_cpu_irq_save();
	atom->counter += value;
	arch_cpu_irq_restore(flags);
}

void __lock arch_cpu_atomic_sub(atomic_t * atom, long value)
{
	irq_flags_t flags;

	flags = arch_cpu_irq_save();
	atom->counter -= value;
	arch_cpu_irq_restore(flags);
}

bool __lock arch_cpu_atomic_testnset(atomic_t * atom, long test, long value)
{
	bool ret = FALSE;
	irq_flags_t flags;

	flags = arch_cpu_irq_save();
        if (atom->counter == test) {
		ret = TRUE;
                atom->counter = value;
	}
	arch_cpu_irq_restore(flags);

        return ret;
}

long __lock arch_cpu_atomic_add_return(atomic_t * atom, long value)
{
	long temp;
	irq_flags_t flags;

	flags = arch_cpu_irq_save();
	atom->counter += value;
	temp = atom->counter;
	arch_cpu_irq_restore(flags);

	return temp;
}

long __lock arch_cpu_atomic_sub_return(atomic_t * atom, long value)
{
	long temp;
	irq_flags_t flags;

	flags = arch_cpu_irq_save();
	atom->counter -= value;
	temp = atom->counter;
	arch_cpu_irq_restore(flags);

	return temp;
}
