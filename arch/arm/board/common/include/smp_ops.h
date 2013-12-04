/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file smp_ops.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Common SMP operations interface
 */
#ifndef __SMP_OPS_H_
#define __SMP_OPS_H_

#include <vmm_devtree.h>

#define INVALID_HWID		ULONG_MAX
#if defined(CONFIG_ARM64)
#define MPIDR_HWID_BITMASK	0xFF00FFFFFF
#else
#define MPIDR_HWID_BITMASK	0xFFFFFF
#endif

extern u64 __smp_logical_map[];
#define smp_logical_map(cpu)	__smp_logical_map[(cpu)]

/**
 * Callback operations for SMP CPUs.
 *
 * @name:	Name of the property as appears in a devicetree cpu node's
 *		enable-method property.
 * @cpu_init:	Reads any data necessary for a specific enable-method from the
 *		devicetree, for a given cpu node and proposed logical id.
 * @cpu_prepare: Early one-time preparation step for a cpu. If there is a
 *		mechanism for doing so, tests whether it is possible to boot
 *		the given CPU.
 * @cpu_boot:	Boots a cpu into the kernel.
 * @cpu_postboot: Optionally, perform any post-boot cleanup or necesary
 *		synchronisation. Called from the cpu being booted.
 */
struct smp_operations {
	const char	*name;
	int		(*cpu_init)(struct vmm_devtree_node *, unsigned int);
	int		(*cpu_prepare)(unsigned int);
	int		(*cpu_boot)(unsigned int);
	void		(*cpu_postboot)(void);
};

#define SMP_OPS_DECLARE(name, ops) \
VMM_DEVTREE_NIDTBL_ENTRY(name, "smp_ops", "", "", "", ops)

void smp_write_pen_release(unsigned long val);
unsigned long smp_read_pen_release(void);

#endif
