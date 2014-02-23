/*
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *  Copyright (C) 2000, 2001, 2002 Andi Kleen, SuSE Labs
 */

#ifndef _ASM_X86_STACKTRACE_H
#define _ASM_X86_STACKTRACE_H

#include <vmm_compiler.h>
#include <vmm_stdio.h>
#include <vmm_error.h>
#include <arch_regs.h>
#include <libs/kallsyms.h>
#include <libs/stacktrace.h>

extern int vmm_stack_depth_to_print;

#define STACKSLOTS_PER_LINE	4
#define IRQ_STACK_SIZE		0x1000UL
#define EXEC_STACK_SIZE		0x2000UL

struct stacktrace_ops;

typedef unsigned long (*walk_stack_t)(unsigned long *stack,
				unsigned long bp,
				const struct stacktrace_ops *ops,
				void *data,
				unsigned long *end);

extern unsigned long
print_context_stack(unsigned long *stack, unsigned long bp,
		const struct stacktrace_ops *ops, void *data,
		unsigned long *end);

extern unsigned long
print_context_stack_bp(unsigned long *stack, unsigned long bp,
		const struct stacktrace_ops *ops, void *data,
		unsigned long *end);

/* Generic stack tracer with callbacks */

struct stacktrace_ops {
	void (*address)(void *data, unsigned long address, int reliable);
	/* On negative return stop dumping */
	int (*stack)(void *data, char *name);
	walk_stack_t	walk_stack;
};

void arch_save_stacktrace_regs(struct arch_regs *regs, struct stack_trace *trace);

void dump_trace(struct arch_regs *regs, unsigned long *stack,
		unsigned long bp, const struct stacktrace_ops *ops,
		void *data);

#define STACKSLOTS_PER_LINE 4
#define get_bp(bp) asm("movq %%rbp, %0" : "=r" (bp) :)

static inline unsigned long
stack_frame(struct arch_regs *regs)
{
	if (regs)
		return regs->rbp;

	return 0;
}

extern void
show_trace_log_lvl(struct arch_regs *regs,
		unsigned long *stack, unsigned long bp, char *log_lvl);

extern void
show_stack_log_lvl(struct arch_regs *regs,
		unsigned long *sp, unsigned long bp, char *log_lvl);

extern unsigned int code_bytes;

/* The form of the top of the frame on the stack */
struct stack_frame {
	struct stack_frame *next_frame;
	unsigned long return_address;
};

static inline unsigned long caller_frame_pointer(void)
{
	struct stack_frame *frame;

	get_bp(frame);

	frame = frame->next_frame;

	return (unsigned long)frame;
}

#endif /* _ASM_X86_STACKTRACE_H */
