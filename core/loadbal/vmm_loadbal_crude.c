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
 * This is a very simple and lazy "load balancer". It will balance
 * VCPUs based on host CPU utilization. It does not consider the
 * VCPU nature (i.e. IO-bound or CPU-bound) and rather treats all
 * ready VCPUs equally when balancing. For newly created VCPUs, it
 * will try to provide host CPU with least number of READY, RUNNING,
 * and PAUSED VCPUs.
 */

#include <vmm_error.h>
#include <vmm_limits.h>
#include <vmm_heap.h>
#include <vmm_timer.h>
#include <vmm_stdio.h>
#include <vmm_manager.h>
#include <vmm_scheduler.h>
#include <vmm_modules.h>
#include <vmm_loadbal.h>
#include <libs/mathlib.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)			vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define MODULE_DESC			"Crude Load Balancer"
#define MODULE_AUTHOR			"Jean-Christophe Dubois"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			crude_init
#define	MODULE_EXIT			crude_exit

struct crude_control {
	u32 alive_count[CONFIG_CPU_COUNT][VMM_VCPU_MAX_PRIORITY+1];
	u32 active_count[CONFIG_CPU_COUNT][VMM_VCPU_MAX_PRIORITY+1];
	u64 idle_ns[CONFIG_CPU_COUNT];
	u64 idle_period_ns[CONFIG_CPU_COUNT];
	u32 idle_percent[CONFIG_CPU_COUNT];
};

static int crude_analyze_count_iter(struct vmm_vcpu *vcpu, void *priv)
{
	u32 hcpu, state;
	struct crude_control *crude = priv;

	state = vmm_manager_vcpu_get_state(vcpu);
	if (state != VMM_VCPU_STATE_READY &&
	    state != VMM_VCPU_STATE_RUNNING &&
	    state != VMM_VCPU_STATE_PAUSED) {
		return VMM_OK;
	}

	vmm_manager_vcpu_get_hcpu(vcpu, &hcpu);

	crude->alive_count[hcpu][vcpu->priority]++;
	if (state != VMM_VCPU_STATE_PAUSED) {
		crude->active_count[hcpu][vcpu->priority]++;
	}

	return VMM_OK;
}

static void crude_analyze_count(struct crude_control *crude)
{
	memset(crude->alive_count, 0, sizeof(crude->alive_count));
	memset(crude->active_count, 0, sizeof(crude->active_count));

	vmm_manager_vcpu_iterate(crude_analyze_count_iter, crude);
}

static void crude_analyze_idle(struct crude_control *crude)
{
	u32 hcpu;

	memset(crude->idle_ns, 0, sizeof(crude->idle_ns));
	memset(crude->idle_period_ns, 0, sizeof(crude->idle_period_ns));
	memset(crude->idle_percent, 0, sizeof(crude->idle_percent));

	for_each_online_cpu(hcpu) {
		crude->idle_ns[hcpu] = vmm_scheduler_idle_time(hcpu);
		crude->idle_period_ns[hcpu] =
				vmm_scheduler_get_sample_period(hcpu);
		crude->idle_percent[hcpu] = udiv64(crude->idle_ns[hcpu] * 100,
						crude->idle_period_ns[hcpu]);
	}
}

static u32 crude_best_count_hcpu(struct crude_control *crude, u8 priority)
{
	u32 hcpu, best_hcpu, best_hcpu_count;

	best_hcpu = vmm_smp_processor_id();
	best_hcpu_count = crude->alive_count[best_hcpu][priority];

	for_each_online_cpu(hcpu) {
		if (crude->alive_count[hcpu][priority] < best_hcpu_count) {
			best_hcpu = hcpu;
			best_hcpu_count = crude->alive_count[hcpu][priority];
		}
	}

	return best_hcpu;
}

/**
 * Find out best idle hcpu.
 *
 * A best idle hcpu is a hcpu who spends maximum time in idle.
 */
static u32 crude_best_idle_hcpu(struct crude_control *crude)
{
	u32 hcpu, idle, best_hcpu, best_hcpu_idle;

	best_hcpu = vmm_smp_processor_id();
	best_hcpu_idle = crude->idle_percent[best_hcpu];

	for_each_online_cpu(hcpu) {
		idle = crude->idle_percent[hcpu];
		if (idle > best_hcpu_idle) {
			best_hcpu = hcpu;
			best_hcpu_idle = idle;
		}
	}

	return best_hcpu;
}

/**
 * Find out worst idle hcpu.
 *
 * A worst idle hcpu is a hcpu who spends least time in idle.
 *
 * If two hcpus have same idle time then hcpu with more number
 * of active VCPUs is considered worst among the two hcpus.
 */
static u32 crude_worst_idle_hcpu(struct crude_control *crude)
{
	u32 p, hcpu, count, idle;
	u32 worst_hcpu, worst_hcpu_count, worst_hcpu_idle;

	worst_hcpu = vmm_smp_processor_id();
	worst_hcpu_idle = crude->idle_percent[worst_hcpu];
	count = 1;
	for (p = VMM_VCPU_MIN_PRIORITY; p <= VMM_VCPU_MAX_PRIORITY; p++) {
		count += crude->active_count[worst_hcpu][p];
	}
	worst_hcpu_count = count;

	for_each_online_cpu(hcpu) {
		idle = crude->idle_percent[hcpu];
		count = 1;
		for (p = VMM_VCPU_MIN_PRIORITY;
		     p <= VMM_VCPU_MAX_PRIORITY; p++) {
			count += crude->active_count[hcpu][p];
		}
		if ((idle < worst_hcpu_idle) ||
		    ((idle == worst_hcpu_idle) &&
		     (count > worst_hcpu_count))) {
			worst_hcpu = hcpu;
			worst_hcpu_idle = idle;
			worst_hcpu_count = count;
		}
	}

	return worst_hcpu;
}

struct crude_balance_hcpu {
	struct crude_control *crude;
	u8 prio;
	u32 state;
	u32 old_hcpu;
	u32 new_hcpu;
	u32 done;
};

static int crude_balance_hcpu_iter(struct vmm_vcpu *vcpu, void *priv)
{
	int rc;
	u32 hcpu, state;
	const struct vmm_cpumask *aff;
	struct crude_balance_hcpu *crude_bhp = priv;

	if (crude_bhp->done) {
		return VMM_OK;
	}

	if (crude_bhp->prio != vcpu->priority) {
		return VMM_OK;
	}

	vmm_manager_vcpu_get_hcpu(vcpu, &hcpu);
	if (hcpu != crude_bhp->old_hcpu) {
		return VMM_OK;
	}

	state = vmm_manager_vcpu_get_state(vcpu);
	if (state != crude_bhp->state) {
		return VMM_OK;
	}

	aff = vmm_manager_vcpu_get_affinity(vcpu);
	if (vmm_cpumask_weight(aff) < 2) {
		return VMM_OK;
	}

	if (!vmm_cpumask_test_cpu(crude_bhp->new_hcpu, aff)) {
		return VMM_OK;
	}

	DPRINTF("%s: vcpu=%s old_hcpu=%d new_hcpu=%d\n",
		__func__, vcpu->name,
		crude_bhp->old_hcpu, crude_bhp->new_hcpu);

	rc = vmm_manager_vcpu_set_hcpu(vcpu, crude_bhp->new_hcpu);
	if (rc) {
		return rc;
	}

	crude_bhp->done = 1;

	return VMM_OK;
}

static u32 crude_good_hcpu(struct vmm_loadbal_algo *algo, u8 priority)
{
	u32 ret;
	struct crude_control *crude = vmm_loadbal_get_algo_priv(algo);

	if (!crude ||
	    (VMM_VCPU_MAX_PRIORITY < priority)) {
		return vmm_smp_processor_id();
	}

	crude_analyze_count(crude);

	ret = crude_best_count_hcpu(crude, priority);

	DPRINTF("%s: good_hcpu=%d priority=%d\n",
		__func__, ret, priority);

	return ret;
}

static void crude_balance(struct vmm_loadbal_algo *algo)
{
	u8 prio;
	u32 best_hcpu, best_hcpu_idle;
	u32 worst_hcpu, worst_hcpu_idle;
	struct crude_balance_hcpu crude_bhp;
	struct crude_control *crude = vmm_loadbal_get_algo_priv(algo);

	if (!crude) {
		return;
	}

	crude_analyze_count(crude);
	crude_analyze_idle(crude);

	best_hcpu = crude_best_idle_hcpu(crude);
	best_hcpu_idle = crude->idle_percent[best_hcpu];
	worst_hcpu = crude_worst_idle_hcpu(crude);
	worst_hcpu_idle = crude->idle_percent[worst_hcpu];

	DPRINTF("%s: best_hcpu=%d best_hcpu_idle=%d\n",
		__func__, best_hcpu, best_hcpu_idle);
	DPRINTF("%s: worst_hcpu=%d worst_hcpu_idle=%d\n",
		__func__, worst_hcpu, worst_hcpu_idle);

	if ((best_hcpu == worst_hcpu) ||
	    (worst_hcpu_idle > 50) ||
	    ((best_hcpu_idle - worst_hcpu_idle) < 10)) {
		return;
	}

	crude_bhp.crude = crude;
	crude_bhp.state = VMM_VCPU_STATE_READY;
	crude_bhp.old_hcpu = worst_hcpu;
	crude_bhp.new_hcpu = best_hcpu;
	crude_bhp.done = 0;

	for (prio = VMM_VCPU_MIN_PRIORITY;
	     prio <= VMM_VCPU_MAX_PRIORITY; prio++) {
		if (!vmm_scheduler_ready_count(worst_hcpu, prio)) {
			continue;
		}
		DPRINTF("%s: balance worst_hcpu=%d best_hcpu=%d prio=%d\n",
			__func__, worst_hcpu, best_hcpu, prio);
		crude_bhp.prio = prio;
		vmm_manager_vcpu_iterate(crude_balance_hcpu_iter, &crude_bhp);
	}
}

static int crude_start(struct vmm_loadbal_algo *algo)
{
	struct crude_control *crude;

	crude = vmm_zalloc(sizeof(*crude));
	if (!crude) {
		return VMM_ENOMEM;
	}

	vmm_loadbal_set_algo_priv(algo, crude);

	return VMM_OK;
}

static void crude_stop(struct vmm_loadbal_algo *algo)
{
	struct crude_control *crude = vmm_loadbal_get_algo_priv(algo);

	if (!crude) {
		return;
	}

	vmm_loadbal_set_algo_priv(algo, NULL);
	vmm_free(crude);
}

static struct vmm_loadbal_algo crude = {
	.name = "Crude Load Balancer",
	.rating = 1,
	.good_hcpu = crude_good_hcpu,
	.balance = crude_balance,
	.start = crude_start,
	.stop = crude_stop,
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
