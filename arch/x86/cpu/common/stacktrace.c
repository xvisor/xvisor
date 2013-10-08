/**
 * Copyright (c) 2013 Himanshu Chauhan.
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
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief x86 stack trace functions.
 *
 * Portions of this file are derived from arch/x86/kernel/stacktrace.c
 * in linux source.
 *
 */
#include <vmm_compiler.h>
#include <vmm_stdio.h>
#include <vmm_error.h>
#include <stacktrace.h>
#include <arch_regs.h>
#include <libs/kallsyms.h>
#include <libs/stacktrace.h>

static int save_stack_stack(void *data, char *name)
{
	return 0;
}

static void
__save_stack_address(void *data, unsigned long addr, bool reliable)
{
	struct stack_trace *trace = data;
	if (!reliable)
		return;

	if (trace->skip > 0) {
		trace->skip--;
		return;
	}
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = addr;
}

static void save_stack_address(void *data, unsigned long addr, int reliable)
{
	return __save_stack_address(data, addr, reliable);
}

static const struct stacktrace_ops save_stack_ops = {
	.stack		= save_stack_stack,
	.address	= save_stack_address,
	.walk_stack	= print_context_stack_bp,
};

/*
 * Save stack-backtrace addresses into a stack_trace buffer.
 */
void arch_save_stacktrace(struct stack_trace *trace)
{
	dump_trace(NULL, NULL, 0, &save_stack_ops, trace);
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = 0UL;
}

void arch_save_stacktrace_regs(struct arch_regs *regs, struct stack_trace *trace)
{
	dump_trace(regs, NULL, 0, &save_stack_ops, trace);
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = 0UL;
}
