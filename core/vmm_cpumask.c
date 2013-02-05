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
 * @file vmm_cpumask.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implemention of interface for managing set of CPUs
 *
 * The source has been largely adapted from:
 * linux-xxx/kernel/cpu.c
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_cpumask.h>

/* Number of possible processor count 
 * Note: SMP secondary core init will update this count
 */
int vmm_cpu_count = CONFIG_CPU_COUNT;

/*
 * cpu_bit_bitmap[] is a special, "compressed" data structure that
 * represents all CONFIG_CPU_COUNT bits binary values of 1<<nr.
 *
 * It is used by vmm_cpumask_of() to get a constant address to a CPU
 * mask value that has a single bit set only.
 */

/* cpu_bit_bitmap[0] is empty - so we can back into it */
#define MASK_DECLARE_1(x)	[x+1][0] = (1UL << (x))
#define MASK_DECLARE_2(x)	MASK_DECLARE_1(x), MASK_DECLARE_1(x+1)
#define MASK_DECLARE_4(x)	MASK_DECLARE_2(x), MASK_DECLARE_2(x+2)
#define MASK_DECLARE_8(x)	MASK_DECLARE_4(x), MASK_DECLARE_4(x+4)

const unsigned long cpu_bit_bitmap[BITS_PER_LONG+1][BITS_TO_LONGS(CONFIG_CPU_COUNT)] = {

	MASK_DECLARE_8(0),	MASK_DECLARE_8(8),
	MASK_DECLARE_8(16),	MASK_DECLARE_8(24),
#if BITS_PER_LONG > 32
	MASK_DECLARE_8(32),	MASK_DECLARE_8(40),
	MASK_DECLARE_8(48),	MASK_DECLARE_8(56),
#endif
};

const DEFINE_BITMAP(cpu_all_bits, CONFIG_CPU_COUNT) = VMM_CPU_BITS_ALL;

static DEFINE_BITMAP(cpu_possible_bits, CONFIG_CPU_COUNT) __read_mostly;
const struct vmm_cpumask *const cpu_possible_mask = to_cpumask(cpu_possible_bits);

static DEFINE_BITMAP(cpu_online_bits, CONFIG_CPU_COUNT) __read_mostly;
const struct vmm_cpumask *const cpu_online_mask = to_cpumask(cpu_online_bits);

static DEFINE_BITMAP(cpu_present_bits, CONFIG_CPU_COUNT) __read_mostly;
const struct vmm_cpumask *const cpu_present_mask = to_cpumask(cpu_present_bits);

static DEFINE_BITMAP(cpu_active_bits, CONFIG_CPU_COUNT) __read_mostly;
const struct vmm_cpumask *const cpu_active_mask = to_cpumask(cpu_active_bits);

void vmm_set_cpu_possible(unsigned int cpu, bool possible)
{
	if (possible)
		vmm_cpumask_set_cpu(cpu, to_cpumask(cpu_possible_bits));
	else
		vmm_cpumask_clear_cpu(cpu, to_cpumask(cpu_possible_bits));
}

void vmm_set_cpu_present(unsigned int cpu, bool present)
{
	if (present)
		vmm_cpumask_set_cpu(cpu, to_cpumask(cpu_present_bits));
	else
		vmm_cpumask_clear_cpu(cpu, to_cpumask(cpu_present_bits));
}

void vmm_set_cpu_online(unsigned int cpu, bool online)
{
	if (online)
		vmm_cpumask_set_cpu(cpu, to_cpumask(cpu_online_bits));
	else
		vmm_cpumask_clear_cpu(cpu, to_cpumask(cpu_online_bits));
}

void vmm_set_cpu_active(unsigned int cpu, bool active)
{
	if (active)
		vmm_cpumask_set_cpu(cpu, to_cpumask(cpu_active_bits));
	else
		vmm_cpumask_clear_cpu(cpu, to_cpumask(cpu_active_bits));
}

void vmm_init_cpu_present(const struct vmm_cpumask *src)
{
	vmm_cpumask_copy(to_cpumask(cpu_present_bits), src);
}

void vmm_init_cpu_possible(const struct vmm_cpumask *src)
{
	vmm_cpumask_copy(to_cpumask(cpu_possible_bits), src);
}

void vmm_init_cpu_online(const struct vmm_cpumask *src)
{
	vmm_cpumask_copy(to_cpumask(cpu_online_bits), src);
}

