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
 * @file arch_cpu.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief generic interface for arch specific CPU functions
 */
#ifndef _ARCH_CPU_H__
#define _ARCH_CPU_H__

#include <vmm_types.h>

struct vmm_chardev;

/** Print details of given CPU */
void arch_cpu_print(struct vmm_chardev *cdev, u32 cpu);

/** Print summary of all CPUs */
void arch_cpu_print_summary(struct vmm_chardev *cdev);

/**
 * CPU nascent init
 *
 * Only Host aspace, Heap, and Device tree available.
 */
int arch_cpu_nascent_init(void);

/**
 * CPU early init
 *
 * Only Host aspace, Heap, Device tree, Per-CPU areas, CPU hotplug,
 * and Host IRQ available.
 */
int arch_cpu_early_init(void);

/**
 * CPU final init
 *
 * Almost all initialization (including builtin module) done. Only
 * driver probing remains.
 */
int arch_cpu_final_init(void);

#endif
