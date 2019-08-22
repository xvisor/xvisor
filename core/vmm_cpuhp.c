/**
 * Copyright (c) 2019 Anup Patel.
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
 * @file vmm_cpuhp.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Interface for CPU hotplug notifiers
 */

#include <vmm_error.h>
#include <vmm_compiler.h>
#include <vmm_smp.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>
#include <vmm_cpumask.h>
#include <vmm_percpu.h>
#include <vmm_cpuhp.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)		vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

struct cpuhp_state {
	vmm_rwlock_t lock;
	u32 state;
};

static DEFINE_PER_CPU(struct cpuhp_state, chpstate);
static DEFINE_RWLOCK(notify_lock);
static LIST_HEAD(notify_list);

u32 vmm_cpuhp_get_state(u32 cpu)
{
	u32 ret;
	struct cpuhp_state *chps;

	if (!vmm_cpu_possible(cpu))
		return VMM_CPUHP_STATE_OFFLINE;
	chps = &per_cpu(chpstate, cpu);

	vmm_read_lock_lite(&chps->lock);
	ret = chps->state;
	vmm_read_unlock_lite(&chps->lock);

	return ret;
}

int vmm_cpuhp_set_state(u32 state)
{
	int ret = VMM_OK;
	bool teardown = FALSE;
	u32 cpu = vmm_smp_processor_id();
	struct vmm_cpuhp_notify *chpn = NULL;
	struct cpuhp_state *chps = &per_cpu(chpstate, cpu);

	vmm_read_lock_lite(&notify_lock);

	vmm_write_lock_lite(&chps->lock);

	if (chps->state < state) {
		list_for_each_entry(chpn, &notify_list, head) {
			if (chpn->startup &&
			    (chps->state < chpn->state) &&
			    (chpn->state <= state)) {
				DPRINTF("CPU%d: state=%d notifier=%s %s()\n",
					cpu, chpn->state, chpn->name,
					"startup");
				ret = chpn->startup(chpn, cpu);
				if (ret) {
					break;
				}
			}
		}

	} else if (chps->state > state) {
		teardown = TRUE;
		list_for_each_entry_reverse(chpn, &notify_list, head) {
			if (chpn->teardown &&
			    (state < chpn->state) &&
			    (chpn->state <= chps->state)) {
				DPRINTF("CPU%d: state=%d notifier=%s %s()\n",
					cpu, chpn->state, chpn->name,
					"teardown");
				ret = chpn->teardown(chpn, cpu);
				if (ret) {
					break;
				}
			}
		}
	}

	chps->state = state;

	vmm_write_unlock_lite(&chps->lock);

	vmm_read_unlock_lite(&notify_lock);

	if (ret && chpn) {
		vmm_printf("CPU%d: hotplug state=%d notifier=%s %s() failed "
			   "(error %d)\n", cpu, chpn->state, chpn->name,
			   (teardown) ? "teardown" : "startup", ret);
	}

	return ret;
}

static void cpuhp_register_sync(void *arg1, void *arg2, void *arg3)
{
	u32 cpu = vmm_smp_processor_id();
	struct vmm_cpuhp_notify *cpuhp = arg1;
	struct cpuhp_state *chps = &per_cpu(chpstate, cpu);

	vmm_read_lock_lite(&chps->lock);
	if (cpuhp->startup && (cpuhp->state <= chps->state))
		cpuhp->startup(cpuhp, cpu);
	vmm_read_unlock_lite(&chps->lock);
}

int vmm_cpuhp_register(struct vmm_cpuhp_notify *cpuhp, bool invoke_startup)
{
	u32 cpu, curr_cpu;
	bool found = FALSE;
	struct cpuhp_state *chps = NULL;
	struct vmm_cpuhp_notify *chpn = NULL;

	if (!cpuhp)
		return VMM_EINVALID;
	if (cpuhp->state <= VMM_CPUHP_STATE_OFFLINE)
		return VMM_EINVALID;

	vmm_write_lock_lite(&notify_lock);

	list_for_each_entry(chpn, &notify_list, head) {
		if (chpn == cpuhp) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		vmm_write_unlock_lite(&notify_lock);
		return VMM_EEXIST;
	}

	found = FALSE;
	list_for_each_entry(chpn, &notify_list, head) {
		if (cpuhp->state < chpn->state) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		list_add_tail(&cpuhp->head, &chpn->head);
	} else {
		list_add_tail(&cpuhp->head, &notify_list);
	}

	vmm_write_unlock_lite(&notify_lock);

	if (!invoke_startup || !cpuhp->startup)
		goto done;

	curr_cpu = vmm_smp_processor_id();
	chps = &per_cpu(chpstate, curr_cpu);
	vmm_read_lock_lite(&chps->lock);
	if (cpuhp->state <= chps->state) {
		cpuhp->startup(cpuhp, curr_cpu);
	}
	vmm_read_unlock_lite(&chps->lock);

	for_each_online_cpu(cpu) {
		if (cpu == curr_cpu)
			continue;
		chps = &per_cpu(chpstate, cpu);
		vmm_read_lock_lite(&chps->lock);
		if (cpuhp->state <= chps->state) {
			vmm_smp_ipi_async_call(vmm_cpumask_of(cpu),
						cpuhp_register_sync,
						cpuhp, NULL, NULL);
		}
		vmm_read_unlock_lite(&chps->lock);
	}

done:
	return VMM_OK;
}

int vmm_cpuhp_unregister(struct vmm_cpuhp_notify *cpuhp)
{
	bool found = FALSE;
	struct vmm_cpuhp_notify *chpn;

	if (!cpuhp)
		return VMM_EINVALID;

	vmm_write_lock_lite(&notify_lock);

	list_for_each_entry(chpn, &notify_list, head) {
		if (chpn == cpuhp) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		vmm_write_unlock_lite(&notify_lock);
		return VMM_ENOTAVAIL;
	}

	list_del(&cpuhp->head);

	vmm_write_unlock_lite(&notify_lock);

	return VMM_OK;
}

int __init vmm_cpuhp_init(void)
{
	u32 cpu;
	struct cpuhp_state *chps;

	for_each_possible_cpu(cpu) {
		chps = &per_cpu(chpstate, cpu);
		INIT_RW_LOCK(&chps->lock);
		chps->state = VMM_CPUHP_STATE_OFFLINE;
	}

	return VMM_OK;
}
