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
 * @file vmm_loadbal.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief source file for hypervisor load balancer
 *
 * This is a very simple and crude "load balancer". For now it will
 * switch running vcpu from CPU to CPU depending on affinity setting.
 *
 * So this is not the bet for performance but it helps us to verify
 * that everything is working ok and that we can switch vcpus from
 * CPU to CPU.
 */

#include <vmm_loadbal.h>

u32 vmm_loadbal_get_next_hcpu(struct vmm_vcpu *vcpu)
{
#ifdef CONFIG_SMP
	u32 cpu = vcpu->hcpu + 1;

	while (1) {
		if (cpu >= CONFIG_CPU_COUNT) {
			cpu = 0;
		}

		if (vmm_cpumask_test_cpu(cpu, vcpu->cpu_affinity)) {
			return cpu;
		}

		cpu++;
	}
#else
	/* For uniprocessor we always run on CPU 0 */
	return 0;
#endif
}

u32 vmm_loadbal_get_new_hcpu(struct vmm_vcpu *vcpu)
{
	/* assign the CPU we are running on at the time */
	return vmm_smp_processor_id();
}
