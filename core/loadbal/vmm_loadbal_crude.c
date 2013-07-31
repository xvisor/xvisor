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

#include <vmm_smp.h>
#include <vmm_timer.h>
#include <vmm_manager.h>
#include <vmm_completion.h>
#include <vmm_loadbal.h>

#define LOADBAL_VCPU_STACK_SZ 		CONFIG_THREAD_STACK_SIZE
#define LOADBAL_VCPU_PRIORITY 		VMM_VCPU_MAX_PRIORITY
#define LOADBAL_VCPU_TIMESLICE 		VMM_VCPU_DEF_TIME_SLICE
#define LOADBAL_VCPU_PERIOD		(5000000000ULL)

struct vmm_loadbal_ctrl {
	struct vmm_completion loadbal_cmpl;
	struct vmm_vcpu *loadbal_vcpu;
};

static struct vmm_loadbal_ctrl lbctrl;

static u32 loadbal_get_next_hcpu(struct vmm_vcpu *vcpu, 
				 u32 old_hcpu, const struct vmm_cpumask *mask)
{
	u32 cpu = old_hcpu + 1;

	while (1) {
		if (cpu >= CONFIG_CPU_COUNT) {
			cpu = 0;
		}

		if (vmm_cpumask_test_cpu(cpu, mask)) {
			break;
		}

		cpu++;
	}

	return cpu;
}

static int loadbal_iter(struct vmm_vcpu *vcpu, void *priv)
{
	int rc;
	u64 tstamp;
	u32 new_hcpu, old_hcpu;
	const struct vmm_cpumask *mask;

	/* FIXME: randomly change hcpu of VCPU */
	tstamp = vmm_timer_timestamp();
	if (tstamp & 0x1000) {
		return VMM_OK;
	}

	rc = vmm_manager_vcpu_get_hcpu(vcpu, &old_hcpu);
	if (rc) {
		return rc;
	}

	mask = vmm_manager_vcpu_get_affinity(vcpu);
	if (!mask) {
		return VMM_EFAIL;
	}

	new_hcpu = loadbal_get_next_hcpu(vcpu, old_hcpu, mask);
	if (new_hcpu == old_hcpu) {
		return VMM_OK;
	}

	rc = vmm_manager_vcpu_set_hcpu(vcpu, new_hcpu);
	if (rc) {
		return rc;
	}

//	vmm_printf("%s: migrated vcpu=%s from CPU%d to CPU%d",
//		   __func__, vcpu->name, old_hcpu, next_hcpu);

	return VMM_OK;
}

static void loadbal_main(void)
{
	u64 tstamp;

	while (1) {
		tstamp = LOADBAL_VCPU_PERIOD;
		vmm_completion_wait_timeout(&lbctrl.loadbal_cmpl, &tstamp);

		vmm_manager_vcpu_iterate(loadbal_iter, NULL);
	}
}

int __init vmm_loadbal_init(void)
{
	int rc;

	/* Initalize loadbal completion */
	INIT_COMPLETION(&lbctrl.loadbal_cmpl);

	/* Create loadbal orphan vcpu with default time slice */
	lbctrl.loadbal_vcpu = vmm_manager_vcpu_orphan_create("loadbal",
						(virtual_addr_t)&loadbal_main,
						LOADBAL_VCPU_STACK_SZ,
						LOADBAL_VCPU_PRIORITY, 
						LOADBAL_VCPU_TIMESLICE);
	if (!lbctrl.loadbal_vcpu) {
		return VMM_EFAIL;
	}

	/* The loadbal vcpu need to stay on this cpu */
	if ((rc = vmm_manager_vcpu_set_affinity(lbctrl.loadbal_vcpu,
				vmm_cpumask_of(vmm_smp_processor_id())))) {
		return rc;
	}

	/* Kick loadbal orphan vcpu */
	if ((rc = vmm_manager_vcpu_kick(lbctrl.loadbal_vcpu))) {
		return rc;
	}

	return VMM_OK;
}

