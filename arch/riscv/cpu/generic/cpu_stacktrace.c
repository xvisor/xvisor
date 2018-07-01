/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file cpu_stacktrace.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief RISC-V specific function stacktrace.
 */

#include <vmm_compiler.h>
#include <vmm_macros.h>
#include <vmm_stdio.h>
#include <vmm_error.h>
#include <libs/kallsyms.h>
#include <libs/stacktrace.h>

struct stackframe_ll {
	unsigned long fp;
	unsigned long ra;
};

struct stackframe {
	unsigned long sp;
	struct stackframe_ll ll;
};

int unwind_frame(struct stackframe *frame)
{
	unsigned long low, high;
	unsigned long fp = frame->ll.fp;
	struct stackframe_ll *ll;

	/* Validate frame pointer */
	low = frame->sp + sizeof(struct stackframe_ll);
	high = roundup2_order_size(frame->sp, 12);
	if (unlikely(fp < low || fp > high || fp & 0x7))
		return -1;

	ll = (struct stackframe_ll *)fp - 1;
	frame->ll.fp = ll->fp;
	frame->ll.ra = ll->ra;
	frame->sp = fp;

	return 0;
}

void walk_stackframe(struct stackframe *frame,
		     int (*fn)(struct stackframe *, void *), void *data)
{
	while (1) {
		int ret;

		if (fn(frame, data))
			break;
		ret = unwind_frame(frame);
		if (ret < 0)
			break;
	}
}

struct stack_trace_data {
	struct stack_trace *trace;
	unsigned int skip;
};

static int save_trace(struct stackframe *frame, void *d)
{
	struct stack_trace_data *data = d;
	struct stack_trace *trace = data->trace;
	unsigned long addr = frame->ll.ra - 0x4;

	if (data->skip) {
		data->skip--;
		return 0;
	}

	trace->entries[trace->nr_entries++] = addr;

	return trace->nr_entries >= trace->max_entries;
}

void arch_save_stacktrace(struct stack_trace *trace)
{
	struct stack_trace_data data;
	struct stackframe frame;

	data.trace = trace;
	data.skip = trace->skip;

	register unsigned long current_sp asm ("sp");

	frame.sp = current_sp;
	frame.ll.fp = (unsigned long)__builtin_frame_address(0);
	frame.ll.ra = (unsigned long)arch_save_stacktrace + 0x4;

	walk_stackframe(&frame, save_trace, &data);
}
