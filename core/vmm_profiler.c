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
#include <kallsyms.h>

/* prototype for mcount callback functions */
typedef void (*ftrace_func_t) (unsigned long ip, unsigned long parent_ip);

/* This function is architecture specific */
extern void vmm_profiler_trace_stub(unsigned long ip, unsigned long parent_ip);

static bool _trace_on = 0;

static unsigned int *_counter = NULL;

/* This is used from the __gnu_mcount_nc function in cpu_entry.S */
ftrace_func_t vmm_profiler_trace_function = vmm_profiler_trace_stub;

static __notrace void vmm_profiler_trace_count(unsigned long ip,
					       unsigned long parent_ip)
{
	if (_trace_on && _counter) {
		_counter[kallsyms_get_symbol_pos(ip, NULL, NULL)]++;
	}
}

bool vmm_profiler_isactive(void)
{
	return _trace_on;
}

int vmm_profiler_start(void)
{
	if (!vmm_profiler_isactive()) {
		vmm_memset(_counter, 0,
			   sizeof(unsigned int) * kallsyms_num_syms);

		vmm_profiler_trace_function = vmm_profiler_trace_count;
		_trace_on = 1;
	} else {
		return VMM_EFAIL;
	}

	return VMM_OK;
}

int vmm_profiler_stop(void)
{
	if (vmm_profiler_isactive()) {
		_trace_on = 0;
		vmm_profiler_trace_function = vmm_profiler_trace_stub;
	} else {
		return VMM_EFAIL;
	}

	return VMM_OK;
}

unsigned int vmm_profiler_get_function_count(unsigned long addr)
{
	return _counter[kallsyms_get_symbol_pos(addr, NULL, NULL)];
}

char *vmm_profiler_get_function_name(unsigned long addr)
{
	return NULL;
}

int vmm_profiler_init(void)
{
	_counter = vmm_malloc(sizeof(unsigned int) * kallsyms_num_syms);

	if (_counter == NULL) {
		return VMM_EFAIL;
	}

	return VMM_OK;
}
