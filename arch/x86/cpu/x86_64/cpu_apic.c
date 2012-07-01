/**
 * Copyright (c) 2012 Himanshu Chauhan.
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
 * @file cpu_apic.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Local APIC programming.
 */

#include <vmm_types.h>
#include <vmm_host_aspace.h>
#include <vmm_stdio.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_host_io.h>
#include <cpu_mmu.h>
#include <arch_cpu.h>
#include <arch_io.h>
#include <cpu_private.h>
#include <cpu_interrupts.h>
#include <acpi.h>
#include <cpu_apic.h>

/* FIXME we should spread the irqs across as many priority levels as possible
 * due to buggy hw */
#define LAPIC_VECTOR(irq)		(IRQ0_VECTOR +(irq))

#define IOAPIC_IRQ_STATE_MASKED		0x1

/* currently only 2 interrupt priority levels are used */
#define SPL0				0x0
#define SPLHI				0xF

#define IOAPIC_IOREGSEL			0x0
#define IOAPIC_IOWIN			0x10
#define MAX_NR_IOAPICS			32

struct cpu_lapic lapic; /* should be per-cpu for SMP */
struct cpu_ioapic io_apic[MAX_NR_IOAPICS];
unsigned int nioapics;
virtual_addr_t lapic_eoi_addr;

struct irq;
typedef void (* eoi_method_t)(struct irq *);

struct irq {
        struct cpu_ioapic *ioa;
        unsigned int pin;
        unsigned int vector;
        eoi_method_t eoi;
        unsigned int state;
};

static struct irq io_apic_irq[NR_IRQ_VECTORS];

/* Disable 8259 - write 0xFF in OCW1 master and slave. */
void i8259_disable(void)
{
	outb(INT2_CTLMASK, 0xFF);
	outb(INT_CTLMASK, 0xFF);
	inb(INT_CTLMASK);
}

static u32 is_lapic_present(void)
{
	u32 a, b, c, d;

	cpuid(CPUID_GETFEATURES,
	      &a, &b, &c, &d);

	return (d & CPUID_FEAT_EDX_APIC);
}

static inline u32 lapic_read(virtual_addr_t base)
{
	return vmm_readl((void *)base);
}

static inline void lapic_write(virtual_addr_t base, u32 val)
{
	vmm_writel(val, (void *)base);
}

static u32 ioapic_read(virtual_addr_t ioa_base, u32 reg)
{
	vmm_writel((reg & 0xff), (void *)(ioa_base + IOAPIC_IOREGSEL));
	return vmm_readl((void *)(ioa_base + IOAPIC_IOWIN));
}

static void ioapic_write(virtual_addr_t ioa_base, u8 reg, u32 val)
{
	vmm_writel(reg, (void *)(ioa_base + IOAPIC_IOREGSEL));
	vmm_writel(val, (void *)(ioa_base + IOAPIC_IOWIN));
}

static void ioapic_enable_pin(virtual_addr_t ioapic_addr, int pin)
{
	u32 lo = ioapic_read(ioapic_addr, IOAPIC_REDIR_TABLE + pin * 2);

	lo &= ~APIC_ICR_INT_MASK;
	ioapic_write(ioapic_addr, IOAPIC_REDIR_TABLE + pin * 2, lo);
}

static void ioapic_disable_pin(virtual_addr_t ioapic_addr, int pin)
{
	u32 lo = ioapic_read(ioapic_addr, IOAPIC_REDIR_TABLE + pin * 2);

	lo |= APIC_ICR_INT_MASK;
	ioapic_write(ioapic_addr, IOAPIC_REDIR_TABLE + pin * 2, lo);
}

static int __unused ioapic_read_irt_entry(virtual_addr_t ioapic_addr, int pin, u64 *entry)
{
	u8 loa = IOAPIC_REDIR_TABLE + pin * 2;
	u8 hia = loa++;

	u32 lo = ioapic_read(ioapic_addr, loa);
	u32 hi = ioapic_read(ioapic_addr, hia);
	*entry = (u64)((((u64)hi) << 32) | lo);

	return VMM_OK;
}

static int ioapic_write_irt_entry(virtual_addr_t ioapic_addr, int pin, u64 entry)
{
	u8 loa = IOAPIC_REDIR_TABLE + pin * 2;
	u8 hia = loa++;
	u32 lo = (u32)(entry & 0xFFFFFFFFUL);
	u32 hi = (u32)((entry >> 32) & 0xFFFFFFFFUL);

#if 0
	ioapic_write(ioapic_addr, loa, lo);
	ioapic_write(ioapic_addr, hia, hi);
#else
	ioapic_write(ioapic_addr, loa, hi);
	ioapic_write(ioapic_addr, hia, lo);
#endif

	return VMM_OK;
}

int ioapic_route_pin_to_irq(u32 pin, u32 irqno)
{
	union ioapic_irt_entry entry;
	vmm_memset(&entry, 0, sizeof(entry));

	entry.bits.intvec = irqno;

	return ioapic_write_irt_entry(io_apic[0].vaddr, pin, entry.val);
}

static int acpi_get_ioapics(struct cpu_ioapic *ioa, unsigned *nioa, unsigned max)
{
	unsigned int n = 0;
	struct acpi_madt_ioapic * acpi_ioa;

	while (n < max) {
		acpi_ioa = acpi_get_ioapic_next();
		if (acpi_ioa == NULL)
			break;

		ioa[n].id = acpi_ioa->id;
		ioa[n].vaddr = vmm_host_iomap(acpi_ioa->address, PAGE_SIZE);
		ioa[n].paddr = (physical_addr_t) acpi_ioa->address;
		ioa[n].gsi_base = acpi_ioa->global_int_base;
		ioa[n].pins = ((ioapic_read(ioa[n].vaddr,
				IOAPIC_VERSION) & 0xff0000) >> 16)+1;
		n++;
	}

	*nioa = n;
	return n;
}

int detect_ioapics(void)
{
	int ret;
	ret = acpi_get_ioapics(io_apic, &nioapics, MAX_NR_IOAPICS);
	return ret;
}

void ioapic_eoi(int irq)
{
	io_apic_irq[irq].eoi(&io_apic_irq[irq]);
}

void ioapic_set_id(u32 addr, unsigned int id)
{
	ioapic_write(addr, IOAPIC_ID, id << 24);
}

void ioapic_enable(void)
{
	i8259_disable();

	/* Select IMCR and disconnect 8259s. */
	outb(0x22, 0x70);
	outb(0x23, 0x01);
}

static void ioapic_disable_irq(unsigned int irq)
{
	BUG_ON(io_apic_irq[irq].ioa == NULL,
	       "Disabling unregistered IRQ!!\n");

	ioapic_disable_pin(io_apic_irq[irq].ioa->vaddr, io_apic_irq[irq].pin);
	io_apic_irq[irq].state |= IOAPIC_IRQ_STATE_MASKED;
}

static void ioapic_enable_irq(unsigned int irq)
{
	BUG_ON(io_apic_irq[irq].ioa == NULL,
	       "Trying to enable an unregistered IRQ!!\n");

	ioapic_enable_pin(io_apic_irq[irq].ioa->vaddr, io_apic_irq[irq].pin);
	io_apic_irq[irq].state &= ~IOAPIC_IRQ_STATE_MASKED;
}

void ioapic_unmask_irq(unsigned irq)
{
	ioapic_enable_irq(irq);
}

void ioapic_mask_irq(unsigned irq)
{
	ioapic_disable_irq(irq);
}

static int setup_ioapic(void)
{
	/* FIXME: Get away with this lousy behaviour */
	BUG_ON(!detect_ioapics(), "NO IOAPIC Detected.\n"
	       "System doesn't support boot without IOAPIC.\n");

	ioapic_enable();

	return VMM_OK;
}

int lapic_enable(unsigned cpu)
{
	u32 val, nlvt;

	/* set the highest priority for ever */
	lapic_write(LAPIC_TPR(lapic.vbase), 0x0);

	lapic_eoi_addr = LAPIC_EOI(lapic.vbase);
	/* clear error state register. */
	//val = lapic_errstatus();

	/* Enable Local APIC and set the spurious vector to 0xff. */
	val = lapic_read(LAPIC_SIVR(lapic.vbase));
	val |= APIC_ENABLE | APIC_SPURIOUS_INT_VECTOR;
	val &= ~APIC_FOCUS_DISABLED;
	lapic_write(LAPIC_SIVR(lapic.vbase), val);
	(void) lapic_read(LAPIC_SIVR(lapic.vbase));

	apic_eoi();

	/* Program Logical Destination Register. */
	val = lapic_read(LAPIC_LDR(lapic.vbase)) & ~0xFF000000;
	val |= (cpu & 0xFF) << 24;
	lapic_write(LAPIC_LDR(lapic.vbase), val);

	/* Program Destination Format Register for Flat mode. */
	val = lapic_read(LAPIC_DFR(lapic.vbase)) | 0xF0000000;
	lapic_write (LAPIC_DFR(lapic.vbase), val);

	val = lapic_read (LAPIC_LVTER(lapic.vbase)) & 0xFFFFFF00;
	lapic_write (LAPIC_LVTER(lapic.vbase), val);

	nlvt = (lapic_read(LAPIC_VERSION(lapic.vbase))>>16) & 0xFF;

	if(nlvt >= 4) {
		val = lapic_read(LAPIC_LVTTMR(lapic.vbase));
		lapic_write(LAPIC_LVTTMR(lapic.vbase), val | APIC_ICR_INT_MASK);
	}

	if(nlvt >= 5) {
		val = lapic_read(LAPIC_LVTPCR(lapic.vbase));
		lapic_write(LAPIC_LVTPCR(lapic.vbase), val | APIC_ICR_INT_MASK);
	}

	/* setup TPR to allow all interrupts. */
	val = lapic_read(LAPIC_TPR(lapic.vbase));
	/* accept all interrupts */
	lapic_write(LAPIC_TPR(lapic.vbase), val & ~0xFF);

	(void)lapic_read(LAPIC_SIVR(lapic.vbase));
	apic_eoi();

	return 1;
}

static int setup_lapic(int cpu)
{
	/* Configuration says that  support APIC but its not present! */
	BUG_ON(!is_lapic_present(), "No Local APIC Detected in System!\n");

	lapic.msr = cpu_read_msr(MSR_APIC);

	if (!APIC_ENABLED(lapic.msr)) {
		lapic.msr |= (0x1UL << 11);
		cpu_write_msr(MSR_APIC, lapic.msr);
	}

	lapic.pbase = (APIC_BASE(lapic.msr) << 12);

	/* remap base */
	lapic.vbase = vmm_host_iomap(lapic.pbase, PAGE_SIZE);

	BUG_ON(unlikely(lapic.vbase == 0), "APIC Base mapping failed!\n");

	lapic.version = lapic_read(LAPIC_VERSION(lapic.vbase));

	lapic.integrated = IS_INTEGRATED_APIC(lapic.version);
	lapic.nr_lvt = NR_LVT_ENTRIES(lapic.version);
	lapic_eoi_addr = LAPIC_EOI(lapic.vbase);

	lapic_enable(cpu);

	return VMM_OK;
}

int apic_init(void)
{
	setup_lapic(0);
	setup_ioapic(); /* in SMP only BSP should do it */

	return VMM_OK;
}
