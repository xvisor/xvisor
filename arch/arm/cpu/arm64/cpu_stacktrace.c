/**
 * Copyright (c) 2013 Sukanto Ghosh.
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
 * @brief ARM64 specific function stacktrace.
 *
 * Portions of this file are derived from arch/arm/kernel/stacktrace.c
 * in linux source
 *
 */

#include <vmm_compiler.h>
#include <vmm_stdio.h>
#include <vmm_error.h>
#include <libs/kallsyms.h>
#include <libs/stacktrace.h>

struct stackframe {
        unsigned long fp;
        unsigned long sp;
	unsigned long lr;
        unsigned long pc;
};

/*
 * AArch64 PCS assigns the frame pointer to x29.
 *
 * A simple function prologue looks like this:
 *      sub     sp, sp, #0x10
 *      stp     x29, x30, [sp]
 *      mov     x29, sp
 *
 * A simple function epilogue looks like this:
 *      mov     sp, x29
 *      ldp     x29, x30, [sp]
 *      add     sp, sp, #0x10
 */
int unwind_frame(struct stackframe *frame)
{
        unsigned long low;
        unsigned long fp = frame->fp;

        low  = frame->sp;

       // if (fp < low ||  fp & 0xf)
        if (fp < low)
                return -1;

        frame->sp = fp + 0x10;
        frame->fp = *(unsigned long *)(fp);
        frame->pc = *(unsigned long *)(fp + 8);

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


