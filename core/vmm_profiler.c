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
 * @version 0.01
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief source file of hypervisor profiler (profiling support).
 */

#include <vmm_profiler.h>
#include <vmm_heap.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_timer.h>
#include <arch_cpu.h>
#include <vmm_stdio.h>
#include <vmm_spinlocks.h>
#include <kallsyms.h>

typedef void (*vmm_profile_callback_t) (void *, void *);

struct vmm_profiler_stat {
	u64 counter;
	u64 time;
	u64 time_in;
	bool is_tracing;
};

struct vmm_profiler_ctrl {
	bool is_active;
	bool is_in_trace;
	vmm_spinlock_t lock;
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
	int index;
	irq_flags_t flags;

	if (pctrl.is_in_trace)
		return;

	pctrl.is_in_trace = 1;

	index = kallsyms_get_symbol_pos((long unsigned int)ip, NULL, NULL);

	if (pctrl.stat[index].is_tracing == 1) {
		goto out;
	}

	if (pctrl.stat[index].time_in != 0) {
		goto out;
	}

	flags = vmm_spin_lock_irqsave(&pctrl.lock);

	pctrl.stat[index].counter++;
	pctrl.stat[index].is_tracing = 1;
	pctrl.stat[index].time_in = vmm_timer_timestamp_for_profile();

	vmm_spin_unlock_irqrestore(&pctrl.lock, flags);

 out:
	pctrl.is_in_trace = 0;
}

static void __notrace vmm_profile_exit(void *ip, void *parent_ip)
{
	int index;
	u64 time;
	irq_flags_t flags;

	if (pctrl.is_in_trace) {
		return;
	}

	pctrl.is_in_trace = 1;

	index = kallsyms_get_symbol_pos((long unsigned int)ip, NULL, NULL);

	// If this function was no traced yet ...
	// we just return as we can't get the start timer
	if (pctrl.stat[index].is_tracing != 1) {
		goto out;
	}

	if (pctrl.stat[index].time_in == 0) {
		goto out;
	}

	flags = vmm_spin_lock_irqsave(&pctrl.lock);

	time = vmm_timer_timestamp_for_profile();

	if (pctrl.stat[index].time_in < time) {
		pctrl.stat[index].time += time - pctrl.stat[index].time_in;
	} else {
		//vmm_printf("negative time\n");
	}
	vmm_spin_unlock_irqrestore(&pctrl.lock, flags);

 out:
	pctrl.stat[index].time_in = 0;

	// OK we don't trace this function anymore
	pctrl.stat[index].is_tracing = 0;

	pctrl.is_in_trace = 0;
}

bool vmm_profiler_isactive(void)
{
	return pctrl.is_active;
}

int vmm_profiler_start(void)
{
	if (!vmm_profiler_isactive()) {
		irq_flags_t flags = arch_cpu_irq_save();

		vmm_memset(pctrl.stat, 0,
			   sizeof(struct vmm_profiler_stat) *
			   kallsyms_num_syms);
		_vmm_profile_enter = vmm_profile_enter;
		_vmm_profile_exit = vmm_profile_exit;
		pctrl.is_active = 1;

		arch_cpu_irq_restore(flags);
	} else {
		return VMM_EFAIL;
	}

	return VMM_OK;
}

int vmm_profiler_stop(void)
{
	if (vmm_profiler_isactive()) {
		irq_flags_t flags = arch_cpu_irq_save();

		_vmm_profile_enter = vmm_profile_none;
		_vmm_profile_exit = vmm_profile_none;
		pctrl.is_active = 0;

		arch_cpu_irq_restore(flags);
	} else {
		return VMM_EFAIL;
	}

	return VMM_OK;
}

u64 vmm_profiler_get_function_count(unsigned long addr)
{
	return pctrl.stat[kallsyms_get_symbol_pos(addr, NULL, NULL)].counter;
}

u64 vmm_profiler_get_function_total_time(unsigned long addr)
{
	return pctrl.stat[kallsyms_get_symbol_pos(addr, NULL, NULL)].time;
}

int __init vmm_profiler_init(void)
{
	pctrl.stat =
	    vmm_malloc(sizeof(struct vmm_profiler_stat) * kallsyms_num_syms);

	if (pctrl.stat == NULL) {
		return VMM_EFAIL;
	}

	vmm_memset(pctrl.stat, 0, sizeof(struct vmm_profiler_stat) *
		   kallsyms_num_syms);

	INIT_SPIN_LOCK(&pctrl.lock);

	return VMM_OK;
}
