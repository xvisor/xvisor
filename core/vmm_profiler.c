/**
 * Copyright (c) 2011 Jean-Christophe Dubois
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
 * @file vmm_profiler.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief source file of hypervisor profiler (profiling support).
 */

#include <vmm_profiler.h>
#include <vmm_heap.h>
#include <vmm_error.h>
#include <vmm_timer.h>
#include <vmm_stdio.h>
#include <vmm_smp.h>
#include <arch_cpu.h>
#include <arch_atomic.h>
#include <arch_atomic64.h>
#include <libs/stringlib.h>
#include <libs/kallsyms.h>

typedef void (*vmm_profile_callback_t) (void *, void *);

struct vmm_profiler_ctrl {
	bool is_active;
	bool is_in_trace[CONFIG_CPU_COUNT];
	struct vmm_profiler_stat *stat;
};

static struct vmm_profiler_ctrl pctrl;

static __notrace void vmm_profile_none(void *ip, void *parent_ip)
{
	// Default NULL function
}

static vmm_profile_callback_t _vmm_profile_enter = vmm_profile_none;
static vmm_profile_callback_t _vmm_profile_exit = vmm_profile_none;

void __notrace __cyg_profile_func_enter(void *ip, void *parent_ip)
{
	(*_vmm_profile_enter) (ip, parent_ip);
}

void __notrace __cyg_profile_func_exit(void *ip, void *parent_ip)
{
	(*_vmm_profile_exit) (ip, parent_ip);
}

static void __notrace vmm_profile_enter(void *ip, void *parent_ip)
{
	int index, parent_index, i;
	struct vmm_profiler_counter *ptr;
	int cpu_id = vmm_smp_processor_id();

	if (pctrl.is_in_trace[cpu_id])
		return;

	pctrl.is_in_trace[cpu_id] = TRUE;

	index = kallsyms_get_symbol_pos((long unsigned int)ip, NULL, NULL);
	parent_index =
	    kallsyms_get_symbol_pos((long unsigned int)parent_ip, NULL, NULL);

 retry:
	i = 0;

	while (pctrl.stat[index].counter[i].parent_index
	       && (i < (VMM_PROFILE_OTHER_INDEX))) {
		if (pctrl.stat[index].counter[i].parent_index == parent_index) {
			break;
		}
		i++;
	}

	if (i < VMM_PROFILE_OTHER_INDEX) {
		if (pctrl.stat[index].counter[i].parent_index == 0) {
			pctrl.stat[index].counter[i].parent_index =
			    parent_index;
			goto retry;
		} else {
			ptr = &pctrl.stat[index].counter[i];
		}
	} else {
		ptr = &pctrl.stat[index].counter[VMM_PROFILE_OTHER_INDEX];
	}

	arch_atomic_add(&ptr->count, 1);
	/*
	 * we use time_per_call as a temporary variable, it will be
	 * filled in later on with a meaningfull value.
	 */
	arch_atomic64_add(&ptr->time_per_call, vmm_timer_timestamp_for_profile());

	pctrl.is_in_trace[cpu_id] = FALSE;
}

static void __notrace vmm_profile_exit(void *ip, void *parent_ip)
{
	int index, parent_index, i;
	u64 time, previous;
	struct vmm_profiler_counter *ptr;
	int cpu_id = vmm_smp_processor_id();

	if (pctrl.is_in_trace[cpu_id])
		return;

	pctrl.is_in_trace[cpu_id] = TRUE;

	index = kallsyms_get_symbol_pos((long unsigned int)ip, NULL, NULL);
	parent_index =
	    kallsyms_get_symbol_pos((long unsigned int)parent_ip, NULL, NULL);

	i = 0;

	while (pctrl.stat[index].counter[i].parent_index
	       && (i < (VMM_PROFILE_OTHER_INDEX))) {
		if (pctrl.stat[index].counter[i].parent_index == parent_index) {
			break;
		}
		i++;
	}

	if (i < VMM_PROFILE_OTHER_INDEX) {
		if (pctrl.stat[index].counter[i].parent_index == 0) {
			goto out;
		} else {
			ptr = &pctrl.stat[index].counter[i];
		}
	} else {
		ptr = &pctrl.stat[index].counter[VMM_PROFILE_OTHER_INDEX];
	}

	time = vmm_timer_timestamp_for_profile();
	previous = arch_atomic64_read(&ptr->time_per_call);

	/*
	 * we use time_per_call as a temporary variable, it will be
	 * filled in later on with a meaningfull value.
	 */
	if (time >= previous) {
		arch_atomic64_add(&ptr->total_time, time - previous);
		arch_atomic64_sub(&ptr->time_per_call, previous);
	} else {
		arch_atomic64_sub(&ptr->time_per_call, time);
	}

 out:
	pctrl.is_in_trace[cpu_id] = FALSE;
}

bool __notrace vmm_profiler_isactive(void)
{
	return pctrl.is_active;
}

int __notrace vmm_profiler_start(void)
{
	if (!vmm_profiler_isactive()) {
		int i;

		for (i = 0; i < CONFIG_CPU_COUNT; i++) {
			pctrl.is_in_trace[i] = FALSE;
		}

		memset(pctrl.stat, 0,
		       sizeof(struct vmm_profiler_stat) * kallsyms_num_syms);

		_vmm_profile_enter = vmm_profile_enter;
		_vmm_profile_exit = vmm_profile_exit;

		pctrl.is_active = TRUE;
	} else {
		return VMM_EFAIL;
	}

	return VMM_OK;
}

int __notrace vmm_profiler_stop(void)
{
	if (vmm_profiler_isactive()) {
		pctrl.is_active = FALSE;

		_vmm_profile_enter = vmm_profile_none;
		_vmm_profile_exit = vmm_profile_none;
	} else {
		return VMM_EFAIL;
	}

	return VMM_OK;
}

struct vmm_profiler_stat *vmm_profiler_get_stat_array(void)
{
	return pctrl.stat;
}

int __init vmm_profiler_init(void)
{
	pctrl.stat =
	    vmm_zalloc(sizeof(struct vmm_profiler_stat) * kallsyms_num_syms);

	if (pctrl.stat == NULL) {
		return VMM_EFAIL;
	}

	return VMM_OK;
}
