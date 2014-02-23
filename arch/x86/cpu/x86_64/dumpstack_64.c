/*
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *  Copyright (C) 2000, 2001, 2002 Andi Kleen, SuSE Labs
 */
#include <vmm_compiler.h>
#include <vmm_stdio.h>
#include <vmm_error.h>
#include <cpu_interrupts.h>
#include <vmm_host_aspace.h>
#include <arch_regs.h>
#include <libs/kallsyms.h>
#include <stacktrace.h>
#include <libs/stacktrace.h>

static char x86_stack_ids[][8] = {
	[ REGULAR_INT_STACK		]	= "INT",
	[ STACKFAULT_STACK		]       = "#SF",
	[ DEBUG_STACK			]	= "#DB",
	[ NMI_STACK			]	= "NMI",
	[ DOUBLEFAULT_STACK		]	= "#DF",
	[ MCE_STACK			]	= "#MC",
	[ EXCEPTION_STACK		]	= "EXC",
};

extern u8 _ist_stacks_start, _ist_stacks_end, _stack_start, _stack_end;
static unsigned long *in_exception_stack(unsigned long stack,
					 unsigned *usedp, char **idp)
{
	unsigned k;
	unsigned long stacks_start = ((unsigned long)&_ist_stacks_start) + IRQ_STACK_SIZE;

	/*
	 * Iterate over all exception stacks, and figure out whether
	 * 'stack' is in one of them:
	 * IST stack 0 is the regular interrupt stack.
	 */
	for (k = STACKFAULT_STACK; k < N_EXCEPTION_STACKS; k++) {
		unsigned long end = stacks_start - IRQ_STACK_SIZE;
		stacks_start -= IRQ_STACK_SIZE;

		/*
		 * Is 'stack' above this exception frame's end?
		 * If yes then skip to the next frame.
		 */
		if (stack <= end)
			continue;
		/*
		 * Is 'stack' above this exception frame's start address?
		 * If yes then we found the right frame.
		 */
		if (stack <= end - IRQ_STACK_SIZE) {
			/*
			 * Make sure we only iterate through an exception
			 * stack once. If it comes up for the second time
			 * then there's something wrong going on - just
			 * break out and return NULL:
			 */
			if (*usedp & (1U << k))
				break;
			*usedp |= 1U << k;
			*idp = x86_stack_ids[k];
			return (unsigned long *)end;
		}
	}
	return NULL;
}

static inline int
in_irq_stack(unsigned long *stack, unsigned long *irq_stack,
	     unsigned long *irq_stack_end)
{
	return (stack <= irq_stack && stack > irq_stack_end);
}

/*
 * x86-64 can have up to three kernel stacks:
 * process stack
 * interrupt stack
 * severe exception (double fault, nmi, stack fault, debug, mce) hardware stack
 */
void dump_trace(struct arch_regs *regs, unsigned long *stack,
		unsigned long bp, const struct stacktrace_ops *ops,
		void *data)
{
	unsigned long irq_stack_start = (unsigned long)&_ist_stacks_start;
	unsigned long *irq_stack_end = (unsigned long *)(irq_stack_start + IRQ_STACK_SIZE);
	unsigned long *execution_stack_end = (unsigned long *)&_stack_end;
	unsigned used = 0;
	unsigned long dummy;

	if (!stack) {
		if (regs)
			stack = (unsigned long *)regs->rsp;
		else
			stack = &dummy;
	}

	if (!bp) {
		bp = stack_frame(regs);
		if (!bp) get_bp(bp);
	}

	/*
	 * Print function call entries in all stacks, starting at the
	 * current stack address. If the stacks consist of nested
	 * exceptions
	 */
	for (;;) {
		char *id;
		unsigned long *estack_end;
		estack_end = in_exception_stack((unsigned long)stack, &used, &id);

		if (estack_end) {
			if (ops->stack(data, id) < 0)
				break;

			bp = ops->walk_stack(stack, bp, ops,
					data, estack_end);
			ops->stack(data, "<EOE>");
			/*
			 * We link to the next stack via the
			 * second-to-last pointer (index -2 to end) in the
			 * exception stack:
			 */
			stack = (unsigned long *) estack_end[-2];
			continue;
		}
		if (irq_stack_end) {
			unsigned long *irq_stack;
			irq_stack = (unsigned long *)(irq_stack_end +
						(0x1000UL - 64) / sizeof(*irq_stack));

			if (in_irq_stack(stack, irq_stack, irq_stack_end)) {
				if (ops->stack(data, "IRQ") < 0)
					break;
				bp = ops->walk_stack(stack, bp,
						ops, data, irq_stack_end);
				/*
				 * We link to the next stack (which would be
				 * the process stack normally) the last
				 * pointer (index -1 to end) in the IRQ stack:
				 */
				stack = (unsigned long *) (irq_stack_end[-1]);
				irq_stack_end = NULL;
				ops->stack(data, "EOI");
				continue;
			}
		}
		if (execution_stack_end) {
			unsigned long *irq_stack;
			irq_stack = (unsigned long *)((u64)execution_stack_end +
						(0x2000UL - 64) / sizeof(*irq_stack));

			if (in_irq_stack(stack, irq_stack, execution_stack_end)) {
				if (regs)
					ops->address(data, regs->rip, 1);

				if (ops->stack(data, "EXEC") < 0)
					break;
				bp = ops->walk_stack(stack, bp,
						ops, data, execution_stack_end);
				/*
				 * We link to the next stack (which would be
				 * the process stack normally) the last
				 * pointer (index -1 to end) in the IRQ stack:
				 */
				stack = (unsigned long *) (execution_stack_end[-1]);
				execution_stack_end = NULL;
				ops->stack(data, "EOI");
				continue;
			}
		}
		break;
	}
}

void
show_stack_log_lvl(struct arch_regs *regs, unsigned long *sp,
		unsigned long bp, char *log_lvl)
{
	unsigned long *irq_stack_end;
	unsigned long *irq_stack;
	unsigned long *stack;
	int i;

	irq_stack	= (unsigned long *)&_ist_stacks_start;
	irq_stack_end	= (unsigned long *)((unsigned long)irq_stack + 0x1000UL);

	/*
	 * Debugging aid: "show_stack(NULL, NULL);" prints the
	 * back trace for this cpu:
	 */
	if (sp == NULL) {
		sp = (unsigned long *)&sp;
	}

	extern int kstack_depth_to_print;
	stack = sp;
	for (i = 0; i < kstack_depth_to_print; i++) {
		if (stack >= irq_stack && stack <= irq_stack_end) {
			if (stack == irq_stack_end) {
				stack = (unsigned long *) (irq_stack_end[-1]);
				vmm_printf(" <EOI> ");
			}
		} else {
			if (i && ((i % STACKSLOTS_PER_LINE) == 0))
				vmm_printf("\n");
			vmm_printf(" %016lx", *stack++);
			//touch_nmi_watchdog();
		}
	}

	vmm_printf("\n");
	show_trace_log_lvl(regs, sp, bp, log_lvl);
}

/* Prints also some state that isn't saved in the pt_regs */
void __show_regs(struct arch_regs *regs, int all)
{
	//unsigned long cr0 = 0L, cr2 = 0L, cr3 = 0L, cr4 = 0L, fs, gs, shadowgs;
	//unsigned long d0, d1, d2, d3, d6, d7;
	unsigned int fsindex, gsindex;
	unsigned int ds, cs, es;

	vmm_printf("RIP: %04lx:[<%016lx>] ", regs->cs & 0xffff, regs->rip);
	vmm_printf("RSP: %04lx:%016lx  EFLAGS: %08lx\n", regs->ss,
		regs->rsp, regs->rflags);
	vmm_printf("RAX: %016lx RBX: %016lx RCX: %016lx\n",
		regs->rax, regs->rbx, regs->rcx);
	vmm_printf("RDX: %016lx RSI: %016lx RDI: %016lx\n",
		regs->rdx, regs->rsi, regs->rdi);
	vmm_printf("RBP: %016lx R08: %016lx R09: %016lx\n",
		regs->rbp, regs->r8, regs->r9);
	vmm_printf("R10: %016lx R11: %016lx R12: %016lx\n",
		regs->r10, regs->r11, regs->r12);
	vmm_printf("R13: %016lx R14: %016lx R15: %016lx\n",
		regs->r13, regs->r14, regs->r15);

	asm("movl %%ds,%0" : "=r" (ds));
	asm("movl %%cs,%0" : "=r" (cs));
	asm("movl %%es,%0" : "=r" (es));
	asm("movl %%fs,%0" : "=r" (fsindex));
	asm("movl %%gs,%0" : "=r" (gsindex));

#if 0
	fs = rdmsrl(MSR_FS_BASE);
	gs = rdmsrl(MSR_GS_BASE);
	shadowgs = rdmsrl(MSR_KERNEL_GS_BASE);

	if (!all)
		return;

	cr0 = read_cr0();
	cr2 = read_cr2();
	cr3 = read_cr3();
	cr4 = read_cr4();

	vmm_printf("FS:  %016lx(%04x) GS:%016lx(%04x) knlGS:%016lx\n",
		fs, fsindex, gs, gsindex, shadowgs);
	vmm_printf("CS:  %04x DS: %04x ES: %04x CR0: %016lx\n", cs, ds,
		es, cr0);
	vmm_printf("CR2: %016lx CR3: %016lx CR4: %016lx\n", cr2, cr3,
		cr4);
#endif
}

void show_regs(struct arch_regs *regs)
{
	//int i;
	unsigned long sp;
	//unsigned int code_prologue = code_bytes * 43 / 64;
	//unsigned int code_len = code_bytes;
	//unsigned char c;
	//u8 *ip;

	sp = regs->rsp;
	__show_regs(regs, 1);


	vmm_printf("Stack:\n");
	show_stack_log_lvl(regs, (unsigned long *)sp,
			0, 0);

#if 0
	vmm_printf("Code: ");
	ip = (u8 *)regs->rip - code_prologue;
	if (ip < (u8 *)VMM_PAGE_OFFSET) {
		/* try starting at IP */
		ip = (u8 *)regs->rip;
		code_len = code_len - code_prologue + 1;
	}
#endif
}
