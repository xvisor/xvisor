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
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <cpu_timer.h>
#include <cpu_mmu.h>
#include <cpu_interrupts.h>
#include <vmm_scheduler.h>
#include <arch_cpu.h>
#include <arch_sections.h>

#if CONFIG_LOCAL_APIC
#include <cpu_apic.h>
#endif

static struct gate_descriptor int_desc_table[256] __attribute__((aligned(8)));
static struct idt64_ptr iptr;
static struct tss_64 vmm_tss __attribute__((aligned(8)));
extern struct tss64_desc __xvisor_tss_64_desc;

static int install_idt(void)
{
	vmm_memset(&int_desc_table, 0, sizeof(int_desc_table));

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
	if (gatenum > 255)
		return VMM_EFAIL;

	struct gate_descriptor *idt_entry = &int_desc_table[gatenum];

	idt_entry->ot.bits.z = 0;
	idt_entry->ot.bits.dpl = 0; /* RING 0 */
	idt_entry->ot.bits.ist = 0;
	idt_entry->ot.bits.offset = ((handler_base >> 16) & 0xFFFFUL);
	idt_entry->ot.bits.rz = 0;

	if (flags & IDT_GATE_TYPE_INTERRUPT)
		idt_entry->ot.bits.type = _GATE_TYPE_INTERRUPT;
	else if (flags & IDT_GATE_TYPE_TRAP)
		idt_entry->ot.bits.type = _GATE_TYPE_TRAP;
	else if (flags & IDT_GATE_TYPE_CALL)
		idt_entry->ot.bits.type = _GATE_TYPE_CALL;
	else {
		vmm_memset(idt_entry, 0, sizeof(*idt_entry));
		return VMM_EFAIL;
	}

	idt_entry->sso.bits.offset = handler_base & 0xFFFFUL;
	idt_entry->sso.bits.selector = VMM_CODE_SEG_SEL;

	idt_entry->off.bits.offset = ((handler_base >> 32) & 0xFFFFFFFFUL);

	idt_entry->ot.bits.present = 1;

	return VMM_OK;
}

static void install_tss_64_descriptor(void)
{
	physical_addr_t tss_base = VIRT_TO_PHYS(&vmm_tss);
	unsigned int tss_seg_sel = VMM_TSS_SEG_SEL;
	struct tss64_desc *tss_64_desc = &__xvisor_tss_64_desc;

	tss_64_desc->tbl.bits.tss_base1 = (tss_base & 0xFFFFUL);
	tss_64_desc->tbl.bits.tss_limit = sizeof(vmm_tss) - 1;

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

static int break_point_handler(void)
{
	while(1);
	return 0;
}

static void setup_gate_handlers(void)
{
	set_idt_gate_handler(3, VIRT_TO_PHYS(break_point_handler),
			     IDT_GATE_TYPE_INTERRUPT);
}

int arch_cpu_irq_setup(void)
{
	install_tss_64_descriptor();
	install_idt();
	setup_gate_handlers();

	__asm__ volatile("int $3\n\t"::);

#if CONFIG_LOCAL_APIC
	apic_init();
#endif

        return 0;
}

void arch_cpu_irq_enable(void)
{
}

void arch_cpu_irq_disable(void)
{
}

irq_flags_t arch_cpu_irq_save(void)
{
        return 0;
}

void arch_cpu_irq_restore(irq_flags_t flags)
{
}

void arch_cpu_wait_for_irq(void)
{
	/* FIXME: Use some hardware functionality to wait for interrupt */
	/* OR */
	/* FIXME: Use some soft delay */
}
