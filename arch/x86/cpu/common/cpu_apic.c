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
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_percpu.h>
#include <libs/stringlib.h>
#include <arch_cpu.h>
#include <arch_io.h>
#include <cpu_mmu.h>
#include <cpu_features.h>
#include <cpu_interrupts.h>
#include <cpu_apic.h>
#include <acpi.h>
#include <cpu_apic.h>
#include <vmm_clockchip.h>
#include <vmm_clocksource.h>
#include <vmm_wallclock.h>
#include <vmm_smp.h>

#include <tsc.h>
#include <timers/timer.h>

#undef DEBUG_IOAPIC

#ifdef DEBUG_IOAPIC
#define debug_print(fmt, args...) vmm_printf("ioapic: " fmt, ##args);
#else
#define debug_print(fmnt, args...) { }
#endif

/* FIXME we should spread the irqs across as many priority levels as possible
 * due to buggy hw */
#define LAPIC_VECTOR(irq)		(IRQ0_VECTOR +(irq))

#define IOAPIC_IRQ_STATE_MASKED		0x1

/* currently only 2 interrupt priority levels are used */
#define SPL0				0x0
#define SPLHI				0xF

#define IOAPIC_IOREGSEL			0x0
#define IOAPIC_IOWIN			0x10
#define NR_IOAPIC			8
#define NR_IOAPIC_IRQ			24

/* FIXME: SMP */
DEFINE_PER_CPU(struct cpu_lapic, lapic);
struct cpu_ioapic io_apic[NR_IOAPIC];
unsigned int nioapics;
static int apic_setup_done = 0;

/* Disable 8259 - write 0xFF in OCW1 master and slave. */
void i8259_disable(void)
{
	vmm_outb(0xFF, INT2_CTLMASK);
	vmm_outb(0xFF, INT_CTLMASK);
	vmm_inb(INT_CTLMASK);
}

static u32 is_lapic_present(void)
{
	u32 a, b, c, d;

	cpuid(CPUID_BASE_FEATURES,
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

static void ioapic_irq_mask(struct vmm_host_irq *irq)
{
	struct cpu_ioapic *ioapic = irq->chip_data;

	ioapic_disable_pin(ioapic->vaddr, irq->num);
}

static void ioapic_irq_unmask(struct vmm_host_irq *irq)
{
	struct cpu_ioapic *ioapic = irq->chip_data;

	ioapic_enable_pin(ioapic->vaddr, irq->num);
}

static void lapic_irq_eoi(struct vmm_host_irq *irq)
{
	lapic_write(LAPIC_EOI(this_cpu(lapic).vbase), 0);
}

#ifdef DEBUG_IOAPIC
static u64 ioapic_read_irt_entry(virtual_addr_t ioapic_addr, int pin)
{
	u8 hia = IOAPIC_REDIR_TABLE + pin * 2;
	u8 loa = hia++;
	u64 val;

	u32 hi = ioapic_read(ioapic_addr, hia);
	u32 lo = ioapic_read(ioapic_addr, loa);

	val = (u64)(((u64)hi) << 32 | (lo & 0xFFFFFFFFUL));

	return val;
}
#endif

static int ioapic_write_irt_entry(virtual_addr_t ioapic_addr, int pin, u64 entry)
{
	u8 hia = IOAPIC_REDIR_TABLE + pin * 2;
	u8 loa = hia++;
	u32 lo = (u32)(entry & 0xFFFFFFFFUL);
	u32 hi = (u32)((entry >> 32) & 0xFFFFFFFFUL);

	ioapic_write(ioapic_addr, loa, lo);
	ioapic_write(ioapic_addr, hia, hi);

	return VMM_OK;
}

#ifdef DEBUG_IOAPIC
static void ioapic_dump_redirect_table(virtual_addr_t ioapic_addr)
{
	int pin;
	u64 val;

	vmm_printf("Dumping IOAPIC redirection table:\n");
	vmm_printf("    PIN                VALUE\n");
	vmm_printf("============================\n");
	for (pin = 0; pin < NR_IOAPIC_PINS; pin++) {
		val = ioapic_read_irt_entry(ioapic_addr, pin);
		vmm_printf("PIN: %d HI: %x LO: %x\n",
			   pin, (val >> 32), (val & 0xFFFFFFFFUL));
	}
}
#endif

static int ioapic_route_irq_to_vector(struct cpu_ioapic *ioapic, u32 irq, u32 vector)
{
	union ioapic_irt_entry entry;

	entry.val = 0;
	entry.bits.intvec = vector;
	entry.bits.delmod = 0;
        entry.bits.destmod = 0;
	entry.bits.trigger = 0;
	entry.bits.mask = 1;
	entry.bits.dest = 0;

	if (irq >= NR_IOAPIC_IRQ || vector >= CONFIG_HOST_IRQ_COUNT)
		return VMM_EFAIL;

	if (ioapic_write_irt_entry(ioapic->vaddr, irq, entry.val) != VMM_OK)
		return VMM_EFAIL;

	return VMM_OK;
}

void ioapic_set_id(u32 addr, unsigned int id)
{
	ioapic_write(addr, IOAPIC_ID, id << 24);
}

void ioapic_enable(void)
{
	i8259_disable();

	/* Select IMCR and disconnect 8259s. */
	vmm_outb(0x70, 0x22);
	vmm_outb(0x01, 0x23);
}

int detect_ioapics(unsigned int *nr_ioapics)
{
	u32 val;
	int ret = VMM_OK;
	struct vmm_devtree_node *node;
	char apic_nm[512];
	unsigned int n = 0;

	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				VMM_DEVTREE_MOTHERBOARD_NODE_NAME
				VMM_DEVTREE_PATH_SEPARATOR_STRING
				"APIC");
	if (!node) {
		return VMM_ENODEV;
	}

	ret = vmm_devtree_read_u32(node,
			VMM_DEVTREE_NR_IOAPIC_ATTR_NAME, &val);
	vmm_devtree_dref_node(node);
	if (ret)
		return ret;

#if !defined(CONFIG_SMP)
	*nr_ioapics = 1;
#else
	if (nr_ioapics) *nr_ioapics = val;
#endif

	while (n < val) {
		vmm_sprintf(apic_nm, VMM_DEVTREE_PATH_SEPARATOR_STRING
			VMM_DEVTREE_MOTHERBOARD_NODE_NAME
			VMM_DEVTREE_PATH_SEPARATOR_STRING
			"APIC"
			VMM_DEVTREE_PATH_SEPARATOR_STRING
			VMM_DEVTREE_IOAPIC_NODE_FMT,
			n);

		node = vmm_devtree_getnode(apic_nm);
		BUG_ON(node == NULL);

		ret = vmm_devtree_read_physaddr(node,
				VMM_DEVTREE_IOAPIC_PADDR_ATTR_NAME,
				&io_apic[n].paddr);
		vmm_devtree_dref_node(node);
		if (ret)
			return ret;

		vmm_snprintf((char *)&io_apic[n].name, APIC_NAME_LEN, "IOAPIC-%d", n);
		io_apic[n].id = n;
		io_apic[n].vaddr = vmm_host_iomap(io_apic[n].paddr, PAGE_SIZE);
		io_apic[n].pins = ((ioapic_read(io_apic[n].vaddr,
				IOAPIC_VERSION) & 0xff0000) >> 16)+1;
		ioapic_set_id(io_apic[n].vaddr, n);
		n++;
	}

	return ret;
}

static int setup_ioapic_irq_route(struct cpu_ioapic *ioapic, u32 irq, u32 vector)
{
	/* route the IOAPIC pins to vectors on CPU */
	ioapic_route_irq_to_vector(ioapic, irq, vector);

	/* Host IRQ setup. */
	ioapic->irq_chip[irq].name = &ioapic->name[0];
	ioapic->irq_chip[irq].irq_mask = &ioapic_irq_mask;
	ioapic->irq_chip[irq].irq_unmask = &ioapic_irq_unmask;
	ioapic->irq_chip[irq].irq_eoi = &lapic_irq_eoi;

	/* register this IOAPIC with host IRQ */
	vmm_host_irq_set_chip(irq, &ioapic->irq_chip[irq]);
	vmm_host_irq_set_chip_data(irq, ioapic);
	vmm_host_irq_set_handler(irq, vmm_handle_fast_eoi);

	return VMM_OK;
}

#define IOAPIC_IRQ_TO_VECTOR(_ioapic_id, _irq) ((IOAPIC_IRQ_BASE * \
						(_ioapic_id + 1)) +      \
						 _irq)

static int setup_ioapic(void)
{
	int i, nr;

	/* Read from device tree about presence of IOAPICs
	 * Can't live without? IOAPIC? Shame!! */
	BUG_ON(detect_ioapics(&nioapics));

	for (nr = 0; nr < nioapics; nr++)
		for (i = 0; i < NR_IOAPIC_IRQ; i++)
			setup_ioapic_irq_route(&io_apic[nr],
					i,
					IOAPIC_IRQ_TO_VECTOR(nr, i));

#ifdef DEBUG_IOPIC
	ioapic_dump_redirect_table(io_apic[0].vaddr);
#endif

	ioapic_enable();

	return VMM_OK;
}

int lapic_enable(unsigned cpu)
{
	u32 val, nlvt;

	/* set the highest priority for ever */
	lapic_write(LAPIC_TPR(this_cpu(lapic).vbase), 0x0);

	/* clear error state register. */
	//val = lapic_errstatus();

	/* Enable Local APIC and set the spurious vector to 0xff. */
	val = lapic_read(LAPIC_SIVR(this_cpu(lapic).vbase));
	val |= APIC_ENABLE | APIC_SPURIOUS_INT_VECTOR;
	val &= ~APIC_FOCUS_DISABLED;
	lapic_write(LAPIC_SIVR(this_cpu(lapic).vbase), val);
	(void) lapic_read(LAPIC_SIVR(this_cpu(lapic).vbase));

	/* Program Logical Destination Register. */
	val = lapic_read(LAPIC_LDR(this_cpu(lapic).vbase)) & ~0xFF000000;
	val |= (cpu & 0xFF) << 24;
	lapic_write(LAPIC_LDR(this_cpu(lapic).vbase), val);

	/* Program Destination Format Register for Flat mode. */
	val = lapic_read(LAPIC_DFR(this_cpu(lapic).vbase)) | 0xF0000000;
	lapic_write (LAPIC_DFR(this_cpu(lapic).vbase), val);

	val = lapic_read (LAPIC_LVTER(this_cpu(lapic).vbase)) & 0xFFFFFF00;
	lapic_write (LAPIC_LVTER(this_cpu(lapic).vbase), val);

	nlvt = (lapic_read(LAPIC_VERSION(this_cpu(lapic).vbase))>>16) & 0xFF;

	if(nlvt >= 4) {
		val = lapic_read(LAPIC_LVTTMR(this_cpu(lapic).vbase));
		lapic_write(LAPIC_LVTTMR(this_cpu(lapic).vbase), val | APIC_ICR_INT_MASK);
	}

	if(nlvt >= 5) {
		val = lapic_read(LAPIC_LVTPCR(this_cpu(lapic).vbase));
		lapic_write(LAPIC_LVTPCR(this_cpu(lapic).vbase), val | APIC_ICR_INT_MASK);
	}

	/* setup TPR to allow all interrupts. */
	val = lapic_read(LAPIC_TPR(this_cpu(lapic).vbase));
	/* accept all interrupts */
	lapic_write(LAPIC_TPR(this_cpu(lapic).vbase), val & ~0xFF);

	(void)lapic_read(LAPIC_SIVR(this_cpu(lapic).vbase));

	lapic_write(LAPIC_EOI(this_cpu(lapic).vbase), 0);

	return 1;
}

static int setup_lapic(int cpu)
{
	/* Configuration says that  support APIC but its not present! */
	BUG_ON(!is_lapic_present());

	this_cpu(lapic).msr = cpu_read_msr(MSR_IA32_APICBASE);

	if (!APIC_ENABLED(this_cpu(lapic).msr)) {
		this_cpu(lapic).msr |= (0x1UL << 11);
		cpu_write_msr(MSR_IA32_APICBASE, this_cpu(lapic).msr);
	}

	this_cpu(lapic).pbase = (APIC_BASE(this_cpu(lapic).msr) << 12);

	/* remap base */
	this_cpu(lapic).vbase = vmm_host_iomap(this_cpu(lapic).pbase, PAGE_SIZE);

	BUG_ON(unlikely(this_cpu(lapic).vbase == 0));

	this_cpu(lapic).version = lapic_read(LAPIC_VERSION(this_cpu(lapic).vbase));

	this_cpu(lapic).integrated = IS_INTEGRATED_APIC(this_cpu(lapic).version);
	this_cpu(lapic).nr_lvt = NR_LVT_ENTRIES(this_cpu(lapic).version);

	lapic_enable(cpu);

	return VMM_OK;
}

int apic_init(void)
{
	setup_lapic(0);
	setup_ioapic(); /* in SMP only BSP should do it */

	apic_setup_done = 1;

	return VMM_OK;
}

/******************************/
/* LAPIC TIMER INITIALIZATION */
/******************************/
struct lapic_timer {
	u8 timer_name[APIC_NAME_LEN];
	u32 timer_cpu;
	u32 freq_khz;
	u32 armed;
	struct cpu_lapic *lapic;
	struct vmm_host_irq_chip irq_chip;
	struct vmm_clockchip clkchip;
	struct vmm_clocksource clksrc;
} lapic_sys_timer = { 0 };

int is_tsc_deadline_supported(void)
{
	u32 a, b, c, d;

	cpuid(CPUID_BASE_FEATURES,
	      &a, &b, &c, &d);

	return (c & CPUID_FEAT_ECS_TSCDL);
}

static vmm_irq_return_t
lapic_clockchip_irq_handler(int irq_no, void *dev)
{
	struct lapic_timer *timer = (struct lapic_timer *)dev;

	if (unlikely(!timer->clkchip.event_handler))
		return VMM_IRQ_NONE;

	/* call the event handler */
	timer->clkchip.event_handler(&timer->clkchip);

	return VMM_IRQ_HANDLED;
}

static void
lapic_clockchip_set_mode(enum vmm_clockchip_mode mode,
			 struct vmm_clockchip *cc)
{
	BUG_ON(cc == NULL);

	switch (mode) {
	case VMM_CLOCKCHIP_MODE_PERIODIC:
		/* Not supported currently */
		BUG_ON(0);
		break;
	case VMM_CLOCKCHIP_MODE_ONESHOT:
		/* Nothing to be done for being one shot */
		return;
	case VMM_CLOCKCHIP_MODE_UNUSED:
	case VMM_CLOCKCHIP_MODE_SHUTDOWN:
	default:
		/* See later */
		BUG_ON(0);
		break;
	}
}

static int
lapic_arm_timer(struct lapic_timer *timer)
{
	u32 lvt;

	lvt = lapic_read(LAPIC_LVTTR(this_cpu(lapic).vbase));
	lvt &= ~APIC_LVT_MASKED;
	lapic_write(LAPIC_LVTTR(this_cpu(lapic).vbase), lvt);
	timer->armed = 1;

	return VMM_OK;
}

static __unused int
lapic_disarm_timer(struct lapic_timer *timer)
{
	u32 lvt;

	lvt = lapic_read(LAPIC_LVTTR(this_cpu(lapic).vbase));
	lvt |= APIC_LVT_MASKED;
	lapic_write(LAPIC_LVTTR(this_cpu(lapic).vbase), lvt);
	timer->armed = 0;

	return VMM_OK;
}

static int
lapic_clockchip_set_next_event(unsigned long next,
			       struct vmm_clockchip *cc)
{
	u64 res;
	u32 nr_tries = 5;
	struct lapic_timer *timer = container_of(cc, struct lapic_timer,
						 clkchip);
	BUG_ON(timer == NULL);

	if (unlikely(!timer->armed)) {
		lapic_arm_timer(timer);
		timer->armed = 1;
	}

	/* This can be racey. So try for 5 times */
 _tryagain:
	res = get_tsc_serialized();
	res += next;
	cpu_write_msr(MSR_IA32_TSC_DEADLINE, res);
	next = get_tsc_serialized();
	if (next > res) {
		BUG_ON(!nr_tries);
		nr_tries--;
		goto _tryagain;
	}
	return VMM_OK;
}

static void
lapic_timer_irq_mask(struct vmm_host_irq *irq)
{
	struct lapic_timer *timer = (struct lapic_timer *)irq->chip_data;
	u32 lvt;

	/* Disable the local APIC timer */
	lvt = lapic_read(LAPIC_LVTTR(timer->lapic->vbase));
	lvt |= APIC_LVT_MASKED;
	lapic_write(LAPIC_LVTTR(timer->lapic->vbase), lvt);
}

static void
lapic_timer_irq_unmask(struct vmm_host_irq *irq)
{
	struct lapic_timer *timer = (struct lapic_timer *)irq->chip_data;
	u32 lvt;

	/* Disable the local APIC timer */
	lvt = lapic_read(LAPIC_LVTTR(timer->lapic->vbase));
	lvt &= ~APIC_LVT_MASKED;
	lapic_write(LAPIC_LVTTR(timer->lapic->vbase), lvt);
}

int __cpuinit lapic_clockchip_init(void)
{
	int rc, irq;
	u32 lvt;

	lapic_sys_timer.clkchip.name = "lapic_clkchip";
	lapic_sys_timer.clkchip.hirq = IRQ_VECTOR_TO_IRQ(LAPIC_TIMER_IRQ_VECTOR);
	lapic_sys_timer.clkchip.rating = 350;
	lapic_sys_timer.clkchip.cpumask = cpu_all_mask;
	lapic_sys_timer.clkchip.features =
		(VMM_CLOCKCHIP_FEAT_PERIODIC
		 | VMM_CLOCKCHIP_FEAT_ONESHOT);


	/* Since LAPIC timer is internal to LAPIC change mask/unmask function and name */
	irq = IRQ_VECTOR_TO_IRQ(LAPIC_TIMER_IRQ_VECTOR);
	lapic_sys_timer.irq_chip.name = "lapic";
	lapic_sys_timer.irq_chip.irq_mask = &lapic_timer_irq_mask;
	lapic_sys_timer.irq_chip.irq_unmask = &lapic_timer_irq_unmask;
	lapic_sys_timer.irq_chip.irq_eoi = &lapic_irq_eoi;
	vmm_host_irq_set_chip(irq, &lapic_sys_timer.irq_chip);
	vmm_host_irq_set_chip_data(irq, &lapic_sys_timer);
	vmm_host_irq_set_handler(irq, vmm_handle_fast_eoi);

	lvt = lapic_read(LAPIC_LVTTR(this_cpu(lapic).vbase));

	/* Set the LAPIC timer in Deadline mode */
	lvt &= ~(0x3<<17);
	lvt |= APIC_LVT_TIMER_TSCDL;

	/* set the LAPIC timer interrupt vector */
	lvt |= (LAPIC_TIMER_IRQ_VECTOR & 0xFF);

	lapic_write(LAPIC_LVTTR(this_cpu(lapic).vbase), lvt);
	lapic_sys_timer.armed = 0;

	vmm_clocks_calc_mult_shift(&lapic_sys_timer.clkchip.mult,
				   &lapic_sys_timer.clkchip.shift,
				   NSEC_PER_SEC,
				   (lapic_sys_timer.freq_khz*1000),
				   5);
	lapic_sys_timer.clkchip.min_delta_ns = 100000;
	lapic_sys_timer.clkchip.max_delta_ns =
		vmm_clockchip_delta2ns(0x7FFFFFFFFFFFFFFULL,
				       &lapic_sys_timer.clkchip);
	lapic_sys_timer.clkchip.set_mode = &lapic_clockchip_set_mode;
	lapic_sys_timer.clkchip.set_next_event =
		&lapic_clockchip_set_next_event;
	lapic_sys_timer.clkchip.priv = (void *)&lapic_sys_timer;

	vmm_clockchip_register(&lapic_sys_timer.clkchip);

	rc = vmm_host_irq_register(IRQ_VECTOR_TO_IRQ(LAPIC_TIMER_IRQ_VECTOR),
				   "lapic_clkchip",
				   lapic_clockchip_irq_handler,
				   &lapic_sys_timer);
	BUG_ON(rc != VMM_OK);

	return VMM_OK;
}

static u64 lapic_clocksource_read(struct vmm_clocksource *cs)
{
	return get_tsc_serialized();
}

/*
 * We use TSC Deadline mode in which clock source is the CPU's
 * timestamp counter. This can't be enabled or disabled.
 */
static int lapic_clocksource_enable(struct vmm_clocksource *cs)
{
	return VMM_OK;
}

static void lapic_clocksource_disable(struct vmm_clocksource *cs)
{
	return;
}

int __cpuinit lapic_clocksource_init(void)
{
	int rc;

	lapic_sys_timer.clksrc.name = "lapic_clksrc";
	lapic_sys_timer.clksrc.rating = 400;
	lapic_sys_timer.clksrc.mask = 0xFFFFFFFFUL;

	vmm_clocks_calc_mult_shift(&lapic_sys_timer.clksrc.mult,
				   &lapic_sys_timer.clksrc.shift,
				   (lapic_sys_timer.freq_khz*1000), NSEC_PER_SEC, 5);

	lapic_sys_timer.clksrc.read = &lapic_clocksource_read;
	lapic_sys_timer.clksrc.disable = &lapic_clocksource_disable;
	lapic_sys_timer.clksrc.enable = &lapic_clocksource_enable;
	lapic_sys_timer.clksrc.priv = (void *)(&lapic_sys_timer);

	rc = vmm_clocksource_register(&lapic_sys_timer.clksrc);

	return rc;
}

static unsigned int __init
pit_calibrate_tsc(void)
{
	cycles_t start, end;
	u64 pit_tick_rate = 1193182UL; /* 1.193182 Mhz */

	vmm_outb((vmm_inb(0x61) & ~0x02) | 0x1, 0x61);

	vmm_outb(0xb0, 0x43);
	vmm_outb((pit_tick_rate/(1000/50)) & 0xff, 0x42);
	vmm_outb((pit_tick_rate/(1000/50)) >> 8, 0x42);
	start = get_tsc_serialized();
	while((vmm_inb(0x61) & 0x20) == 0);
	end = get_tsc_serialized();

	return (end - start)/50;
}

static void
lapic_set_timer_count(u32 count, int periodic)
{
	u32 lvt;

	/* Setup Divide Count Register to use the bus frequency directl. */
	lapic_write(LAPIC_TIMER_DCR(this_cpu(lapic).vbase), LAPIC_TDR_DIV_1);

	/* Program the initial count register */
	lapic_write(LAPIC_TIMER_ICR(this_cpu(lapic).vbase), count);

	/* Enable the local APIC timer */
	lvt = lapic_read(LAPIC_LVTTR(this_cpu(lapic).vbase));
	lvt &= ~APIC_LVT_MASKED;
	if (periodic)
		lvt |= APIC_LVT_TIMER_PERIODIC;
	else
		lvt &= ~APIC_LVT_TIMER_PERIODIC;

	lapic_write(LAPIC_LVTTR(this_cpu(lapic).vbase), lvt);
}

void
lapic_stop_timer(void)
{
	u32 lvt;

	/* set initial count to 0 */
	lapic_write(LAPIC_TIMER_ICR(this_cpu(lapic).vbase), 0);

	/* Disable the local APIC timer */
	lvt = lapic_read(LAPIC_LVTTR(this_cpu(lapic).vbase));
	/*
	 * If operating in Deadline mode MSR needs to be zeroed to
	 * disable timer.
	 */
	if ((lvt & (0x3 << 17)) == APIC_LVT_TIMER_TSCDL)
		cpu_write_msr(MSR_IA32_TSC_DEADLINE, 0);
	lvt |= APIC_LVT_MASKED;
	lapic_write(LAPIC_LVTTR(this_cpu(lapic).vbase), lvt);
}

unsigned int __init
lapic_calibrate_timer(void)
{
	const unsigned int tick_count = 100000000;
	cycles_t tsc_start, tsc_now;
	u32 apic_start, apic_now;
	u32 apic_Hz;

	/* Start the APIC counter running for calibration */
	lapic_set_timer_count(400000000, 1);

	apic_start = lapic_read(LAPIC_TIMER_CCR(this_cpu(lapic).vbase));
	tsc_start = get_tsc_serialized();

	/* Spin until enough ticks */
	do {
		apic_now = lapic_read(LAPIC_TIMER_CCR(this_cpu(lapic).vbase));
		tsc_now = get_tsc_serialized();
	} while(((tsc_now - tsc_start) < tick_count) &&
		((apic_start - apic_now) < tick_count));

	apic_Hz = (apic_start - apic_now) * 1000L * \
		cpu_info.tsc_khz / (tsc_now - tsc_start);

	lapic_stop_timer();

	return (apic_Hz/1000);
}

int __init
lapic_timer_init(void)
{
	struct x86_system_timer_ops lapic_sys_timer_ops;

	lapic_sys_timer_ops.sys_cc_init = &lapic_clockchip_init;
	lapic_sys_timer_ops.sys_cs_init = &lapic_clocksource_init;

	if (!apic_setup_done) {
		vmm_printf("%s: LAPIC setup is not done yet!\n", __func__);
		return VMM_EFAIL;
	}

	if (!is_tsc_deadline_supported()) {
		vmm_printf("%s: TSC Deadline is not supported by LAPIC\n", __func__);
		return VMM_EFAIL;
	}

	/* save the calibrated CPU frequency */
	cpu_info.tsc_khz = pit_calibrate_tsc();
	cpu_info.lapic_khz = lapic_calibrate_timer();

	/*
	 * We are going to USE TSC_DEADLINE mode which will use
	 * TSC frequency to count. Hence High Resolution timer
	 * needs to be programmed with same frequency.
	 */
	lapic_sys_timer.freq_khz = cpu_info.tsc_khz;
	lapic_sys_timer.lapic = &this_cpu(lapic);
	lapic_sys_timer.timer_cpu = vmm_smp_processor_id();
	vmm_snprintf((char *)&lapic_sys_timer.timer_name[0], APIC_NAME_LEN,
		     "LAPIC-%d", vmm_smp_processor_id());
	vmm_printf("TSC Freq: %d kHZ LAPIC Freq: %d kHZ\n",
		   cpu_info.tsc_khz, cpu_info.lapic_khz);

	x86_register_system_timer_ops(&lapic_sys_timer_ops);

	return VMM_OK;
}
