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
 * @file arch_smp.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief generic interface for arch specific SMP functions
 */
#ifndef _ARCH_SMP_H__
#define _ARCH_SMP_H__

#include <vmm_types.h>
#include <vmm_cpumask.h>

/** Retrive current processor id 
 *  Note: This function is called from any CPU at runtime
 */
u32 arch_smp_id(void);

/** Initalize secondary CPUs 
 *  Note: This function is called from primary CPU only at boot time
 *  Note: This function is supposed to inform about possible CPUs using
 *  vmm_set_cpu_possible() API
 */
int arch_smp_init_cpus(void);

/** Prepare possible secondary CPUs 
 *  Note: This function is called from primary CPU only at boot time
 *  Note: This function is supposed to inform about present CPUs using
 *  vmm_set_cpu_present() API
 */
int arch_smp_prepare_cpus(unsigned int max_cpus);

/** Start a present secondary CPU
 *  Note: This function is called from primary CPU only at boot time
 */
int arch_smp_start_cpu(u32 cpu);

/** Trigger inter-processor interrupt
 *  Note: This function is called on any CPU at runtime
 */
void arch_smp_ipi_trigger(const struct vmm_cpumask *dest);

/** Initialize inter-processor interrupts
 *  Note: This function is called on all CPUs at boot time
 */
int arch_smp_ipi_init(void);

#endif
