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
 * @file vmm_loadbal_crude.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for crude load balancing algo
 *
 * This is a very simple and crude "load balancer". For now it will
 * switch running vcpu from CPU to CPU depending on affinity setting.
 *
 * So this is not the bet for performance but it helps us to verify
 * that everything is working ok and that we can switch vcpus from
 * CPU to CPU.
 */

#include <vmm_error.h>
#include <vmm_manager.h>
#include <vmm_timer.h>
#include <vmm_modules.h>
#include <vmm_loadbal.h>

#define MODULE_DESC			"Crude Load Balancer"
#define MODULE_AUTHOR			"Jean-Christophe Dubois"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			crude_init
#define	MODULE_EXIT			crude_exit

static u32 crude_next_hcpu(struct vmm_vcpu *vcpu, 
			   u32 old_hcpu, const struct vmm_cpumask *mask)
{
	u32 cpu;
	u64 tstamp;

	/* FIXME: randomly change hcpu of VCPU */
	tstamp = vmm_timer_timestamp();
	if (tstamp & 0x1000) {
		return old_hcpu;
	}

	cpu = old_hcpu + 1;
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

static int crude_iter(struct vmm_vcpu *vcpu, void *priv)
{
	int rc;
	u32 new_hcpu, old_hcpu;
	const struct vmm_cpumask *mask;

	rc = vmm_manager_vcpu_get_hcpu(vcpu, &old_hcpu);
	if (rc) {
		return rc;
	}

	mask = vmm_manager_vcpu_get_affinity(vcpu);
	if (!mask) {
		return VMM_EFAIL;
	}

	new_hcpu = crude_next_hcpu(vcpu, old_hcpu, mask);
	if (new_hcpu == old_hcpu) {
		return VMM_OK;
	}

	rc = vmm_manager_vcpu_set_hcpu(vcpu, new_hcpu);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

static void crude_balance(struct vmm_loadbal_algo *algo)
{
	u32 i, count;
	struct vmm_vcpu *vcpu;

	count = vmm_manager_vcpu_count();
	for (i = 0; i < count; i++) {
		vcpu = vmm_manager_vcpu(i);
		if (!vcpu) {
			continue;
		}
		crude_iter(vcpu, NULL);
	}
}

static struct vmm_loadbal_algo crude = {
	.name = "Crude Load Balancer",
	.rating = 1,
	.balance = crude_balance,
};

static int __init crude_init(void)
{
	return vmm_loadbal_register_algo(&crude);
}

static void __exit crude_exit(void)
{
	vmm_loadbal_unregister_algo(&crude);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
