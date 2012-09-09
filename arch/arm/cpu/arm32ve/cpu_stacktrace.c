/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief ARM32 specific function stacktrace.
 *
 * Portions of this file are derived from arch/arm/kernel/stacktrace.c
 * in linux source
 *
 */

#include <vmm_compiler.h>
#include <vmm_stdio.h>
#include <vmm_error.h>
#include <kallsyms.h>
#include <stacktrace.h>

struct stackframe {
	unsigned long fp;
	unsigned long sp;
	unsigned long lr;
	unsigned long pc;
};

#define ALIGN(x, a)	(((x) + (a) - 1) & ~((a) - 1))
/*
 * Unwind the current stack frame and store the new register values in the
 * structure passed as argument. Unwinding is equivalent to a function return,
 * hence the new PC value rather than LR should be used for backtrace.
 *
 * With framepointer enabled, a simple function prologue looks like this:
 *	mov	ip, sp
 *	stmdb	sp!, {fp, ip, lr, pc}
 *	sub	fp, ip, #4
 *
 * A simple function epilogue looks like this:
 *	ldm	sp, {fp, sp, pc}
 *
 * Note that with framepointer enabled, even the leaf functions have the same
 * prologue and epilogue, therefore we can ignore the LR value in this case.
 */
int unwind_frame(struct stackframe *frame)
{
	unsigned long high, low;
	unsigned long fp = frame->fp;

	/* only go to a higher address on the stack */
	low = frame->sp;
	high = ALIGN(low, 4096);

	/* check current frame pointer is within bounds */
	if (fp < (low + 12) || fp + 4 >= high)
		return VMM_EFAIL;

	/* restore the registers from the stack frame */
	frame->fp = *(unsigned long *)(fp - 12);
	frame->sp = *(unsigned long *)(fp - 8);
	frame->pc = *(unsigned long *)(fp - 4);

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
	unsigned long addr = frame->pc;

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

	frame.fp = (unsigned long)__builtin_frame_address(0);
	frame.sp = current_sp;
	frame.lr = (unsigned long)__builtin_return_address(0);
	frame.pc = (unsigned long)arch_save_stacktrace;

	walk_stackframe(&frame, save_trace, &data);
}

