/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file cpu_interrupts.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief source code for handling cpu interrupts
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_host_irq.h>
#include <vmm_host_aspace.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <libs/stringlib.h>
#include <arch_cpu.h>
#include <arch_sections.h>
#include <cpu_interrupts.h>

#undef __DEBUG
//#define __DEBUG

#ifdef __DEBUG
#define debug_print(fmt, args...) vmm_printf("cpu_interrupt[%d]: " fmt, __LINE__, ##args)
#else
#define debug_print(fmt, args...) { }
#endif

static struct gate_descriptor int_desc_table[256] __attribute__((aligned(8)));
static struct idt64_ptr iptr;
static struct tss_64 vmm_tss __attribute__((aligned(8)));
extern struct tss64_desc __xvisor_tss_64_desc;

#define VIRT_TO_PHYS(ptr)	({ \
				physical_addr_t pa = 0x0; \
				vmm_host_va2pa((virtual_addr_t)ptr, &pa); \
				pa; \
				})

static int install_idt(void)
{
	memset(&int_desc_table, 0, sizeof(int_desc_table));

	iptr.idt_base = VIRT_TO_PHYS(&int_desc_table[0]);
	iptr.idt_limit = sizeof(int_desc_table) - 1;

	__asm__ volatile("lidt (%0)\n\t"
			 ::"r"(&iptr));

	return 0;
}

/* only trap and interrupt gates. No Task */
static int set_idt_gate_handler(u32 gatenum, physical_addr_t handler_base,
				u32 flags)
{
	if (gatenum >= NR_GATES)
		return VMM_EFAIL;

	struct gate_descriptor *idt_entry = &int_desc_table[gatenum];

	idt_entry->ot.bits.z = 0;
	idt_entry->ot.bits.dpl = 0; /* RING 0 */
	idt_entry->ot.bits.ist = 1;
	idt_entry->ot.bits.offset = ((handler_base >> 16) & 0xFFFFUL);
	idt_entry->ot.bits.rz = 0;

	if (flags & IDT_GATE_TYPE_INTERRUPT)
		idt_entry->ot.bits.type = _GATE_TYPE_INTERRUPT;
	else if (flags & IDT_GATE_TYPE_TRAP)
		idt_entry->ot.bits.type = _GATE_TYPE_TRAP;
	else if (flags & IDT_GATE_TYPE_CALL)
		idt_entry->ot.bits.type = _GATE_TYPE_CALL;
	else {
		memset(idt_entry, 0, sizeof(*idt_entry));
		return VMM_EFAIL;
	}

	idt_entry->sso.bits.offset = handler_base & 0xFFFFUL;
	idt_entry->sso.bits.selector = VMM_CODE_SEG_SEL;

	idt_entry->off.bits.offset = ((handler_base >> 32) & 0xFFFFFFFFUL);

	idt_entry->ot.bits.present = 1;

	return VMM_OK;
}

static inline void set_interrupt_gate(u8 intrno, physical_addr_t addr)
{
	set_idt_gate_handler(intrno, addr, IDT_GATE_TYPE_INTERRUPT);
}

static inline void set_trap_gate(u8 trapno, physical_addr_t addr)
{
	set_idt_gate_handler(trapno, addr, IDT_GATE_TYPE_TRAP);
}

static void setup_tss64(struct tss_64 *init_tss)
{
	extern u8 _ist_stacks_start;
	int i;
	u32 *tss_stacks = (u32 *)(&init_tss->ist1_lo);
	u64 stack_start = (u64)&_ist_stacks_start;

	for (i = 0; i < (2 * NR_IST_STACKS); i += 2) {
		debug_print("stack[%d]: %lx%lx\n", i, stack_start >> 32, stack_start & 0xFFFFFFFFUL);
		tss_stacks[i] = (u32)(stack_start & 0xFFFFFFFFUL);
		tss_stacks[i+1] = (u32)((stack_start >> 32) & 0xFFFFFFFFUL);
		stack_start -= PAGE_SIZE;
	}
}

static void install_tss_64_descriptor(struct tss_64 *init_tss)
{
	physical_addr_t tss_base = VIRT_TO_PHYS(init_tss);
	unsigned int tss_seg_sel = VMM_TSS_SEG_SEL;
	struct tss64_desc *tss_64_desc = &__xvisor_tss_64_desc;

	tss_64_desc->tbl.bits.tss_base1 = (tss_base & 0xFFFFUL);
	tss_64_desc->tbl.bits.tss_limit = sizeof(*init_tss) - 1;

	tss_64_desc->tbt.bits.tss_base2 = ((tss_base >> 16) & 0xFFUL);
	tss_64_desc->tbt.bits.type = _GATE_TYPE_TSS_AVAILABLE;
	tss_64_desc->tbt.bits.dpl = 0; /* ring 0 */
	tss_64_desc->tbt.bits.present = 1;
	tss_64_desc->tbt.bits.limit = 0;
	tss_64_desc->tbt.bits.tss_base3 = ((tss_base >> 24) & 0xFFUL);
	tss_64_desc->tbt.bits.granularity = 0; /* count in bytes */
	tss_64_desc->tbt.bits.avl = 0;

	tss_64_desc->tb.bits.tss_base4 = ((tss_base >> 32)
					  & 0xFFFFFFFFUL);

	__asm__ volatile("ltr (%0)\n\t"
			 ::"r"(&tss_seg_sel));
}

static void setup_gate_handlers(void)
{
	u32 i;
	u64 user_irq_base = VIRT_TO_PHYS(__IRQ_32);

	/* Install default handler for all interrupts, then cherry pick */
	for (i = 0; i < NR_IRQ_VECTORS; i++) {
		if (i >= USER_DEFINED_IRQ_BASE) {
			set_interrupt_gate(i, user_irq_base);
			user_irq_base += IRQ_VECTOR_ALIGN_SZ; /* 128 instructions */
		} else
			set_interrupt_gate(i, VIRT_TO_PHYS(_generic_handler));
	}

	set_trap_gate(0, VIRT_TO_PHYS(_exception_div_error));	        /* divide error */
	set_trap_gate(1, VIRT_TO_PHYS(_exception_debug));	        /* debug */
	set_trap_gate(3, VIRT_TO_PHYS(_exception_bp));                  /* Breakpoint */
	set_trap_gate(4, VIRT_TO_PHYS(_exception_ovf));                 /* Overflow */
	set_trap_gate(5, VIRT_TO_PHYS(_exception_bounds));	        /* Bounds error */
	set_trap_gate(6, VIRT_TO_PHYS(_exception_inval_opc));	        /* Invalid Opcode */
	set_trap_gate(7, VIRT_TO_PHYS(_exception_no_dev));	        /* Dev not avail */
	set_trap_gate(8, VIRT_TO_PHYS(_exception_double_fault));        /* double fault */
	set_trap_gate(9, VIRT_TO_PHYS(_exception_coproc_overrun));      /* coproc seg ovrn */
	set_trap_gate(10, VIRT_TO_PHYS(_exception_inval_tss));          /* invalid tss */
	set_trap_gate(11, VIRT_TO_PHYS(_exception_missing_seg));        /* seg not present */
	set_trap_gate(12, VIRT_TO_PHYS(_exception_missing_stack));      /* stack segment */
	set_trap_gate(13, VIRT_TO_PHYS(_exception_gpf));                /* GPF */
	set_trap_gate(16, VIRT_TO_PHYS(_exception_coproc_err));         /* coproc error */
	set_trap_gate(17, VIRT_TO_PHYS(_exception_align_check));        /* alignment check */
	set_trap_gate(18, VIRT_TO_PHYS(_exception_machine_check));      /* machine check */
	set_trap_gate(19, VIRT_TO_PHYS(_exception_simd_err));           /* simd coproc error */

	set_interrupt_gate(2, VIRT_TO_PHYS(_exception_nmi));            /* NMI */
	set_interrupt_gate(14, VIRT_TO_PHYS(_exception_page_fault));    /* page fault */
}

int __cpuinit arch_cpu_irq_setup(void)
{
	setup_tss64(&vmm_tss);
	install_tss_64_descriptor(&vmm_tss);
	install_idt();
	setup_gate_handlers();

        return 0;
}

extern void dump_vcpu_regs(arch_regs_t *regs);

/* All Handlers */
int do_page_fault(int error, arch_regs_t *regs)
{
	u64 bad_vaddr;
	struct vmm_vcpu *cvcpu = NULL;

	__asm__ __volatile__("movq %%cr2, %0\n\t"
			     :"=r"(bad_vaddr));

	cvcpu = vmm_scheduler_current_vcpu();

	if (cvcpu) {
		vmm_printf("Unhandled access from VMM vcpu %s @ address %lx\n",
			cvcpu->name, bad_vaddr);
	} else {
		vmm_printf("(Page Fault): Unhandled VMM access to address %lx\n", bad_vaddr);
	}

	dump_vcpu_regs(regs);

	while(1); /* Should never reach here. */
}

int do_breakpoint(int intno, arch_regs_t *regs)
{
	dump_vcpu_regs(regs);
	vmm_panic("Unhandled breakpoint in VMM code.\n");
	while(1);
}

int do_gpf(int intno, arch_regs_t *regs)
{
	vmm_panic("(General Proctection Fault)\n");

	while(1); /* Should never reach here. */
}

int do_generic_int_handler(int intno, arch_regs_t *regs)
{
	if (intno == 0x80) {
		vmm_scheduler_preempt_orphan(regs);
	} else {
		/* Get interrupt number from Vector number */
		intno -= USER_DEFINED_IRQ_BASE;

		vmm_scheduler_irq_enter(regs, FALSE);
		vmm_host_irq_exec(intno);
		vmm_scheduler_irq_exit(regs);
	}

	return 0;
}
