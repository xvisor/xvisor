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
 * @file hpet.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief HPET access and configuration.
 */

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>
#include <vmm_heap.h>
#include <vmm_host_io.h>
#include <vmm_clockchip.h>
#include <vmm_clocksource.h>
#include <vmm_host_irq.h>
#include <acpi.h>
#include <cpu_apic.h>
#include <hpet.h>

/* FIXME: Add support for more HPET blocks */
struct hpet *hpet = NULL;

struct hpet_clockchip {
	u32 hpet_timer_id;
	virtual_addr_t base;
	struct vmm_clockchip clkchip;
};

static inline void hpet_timer_write(virtual_addr_t vbase, u32 reg_offset, u64 val)
{
	vmm_out_le64((u64 *)(vbase + reg_offset), val);
}

static inline u64 hpet_timer_read(virtual_addr_t vbase, u32 reg_offset)
{
	return vmm_in_le64((u64 *)(vbase + reg_offset));
}

static u32 hpet_get_int_route(u8 timer_id)
{
	u64 _v = hpet_timer_read(hpet->vbase, HPET_TIMER_N_CONF_BASE(timer_id));
	return (u32)(_v >> 32);
}

static u64 hpet_main_counter_val(void)
{
	return hpet_timer_read(hpet->vbase, HPET_GEN_MAIN_CNTR_BASE);
}

static void hpet_enable_main_counter(void)
{
	u64 _v = hpet_timer_read(hpet->vbase, HPET_GEN_CONF_BASE);
	_v |= 0x01;
	hpet_timer_write(hpet->vbase, HPET_GEN_CONF_BASE, _v);
}

static void hpet_disable_main_counter(void)
{
	u64 _v = hpet_timer_read(hpet->vbase, HPET_GEN_CONF_BASE);
	_v &= ~0x01;
	hpet_timer_write(hpet->vbase, HPET_GEN_CONF_BASE, _v);
}

static void hpet_arm_timer(u8 timer_id)
{
	u64 _v = hpet_timer_read(hpet->vbase, HPET_TIMER_N_CONF_BASE(timer_id));
	_v |= (0x01UL << 2);
	hpet_timer_write(hpet->vbase, HPET_TIMER_N_CONF_BASE(timer_id), _v);
}

static void hpet_disarm_timer(u8 timer_id)
{
	u64 _v = hpet_timer_read(hpet->vbase, HPET_TIMER_N_CONF_BASE(timer_id));
	_v &= ~(0x01UL << 2);
	hpet_timer_write(hpet->vbase, HPET_TIMER_N_CONF_BASE(timer_id), _v);
}

static void hpet_set_timer_periodic(u8 timer_id)
{
	u64 _v = hpet_timer_read(hpet->vbase, HPET_TIMER_N_CONF_BASE(timer_id));
	_v |= (0x01UL << 3);
	hpet_timer_write(hpet->vbase, HPET_TIMER_N_CONF_BASE(timer_id), _v);
}

static void hpet_set_timer_non_periodic(u8 timer_id)
{
	u64 _v = hpet_timer_read(hpet->vbase, HPET_TIMER_N_CONF_BASE(timer_id));
	_v &= ~(0x01UL << 3);
	hpet_timer_write(hpet->vbase, HPET_TIMER_N_CONF_BASE(timer_id), _v);
}

static int hpet_initialize_timer(u8 timer_id, u8 dest_int, u32 flags)
{
	u64 tmr = 0;
	u64 _v = hpet_timer_read(hpet->vbase, HPET_TIMER_N_CONF_BASE(timer_id));

	if (dest_int && !(flags & HPET_TIMER_INT_TO_FSB)) {
		if ((_v >> 32) & (0x01UL << dest_int)) {
			tmr |= (((u64)dest_int) << 9);
		} else {
			vmm_printf("Timer %d interrupt can't be routed to %d on IOAPIC.\n",
				   timer_id, dest_int);
			return VMM_EFAIL;
		}
	} else if ((flags & HPET_TIMER_INT_TO_FSB)) {
		if (_v & (0x01UL << 15)) {
			tmr |= (0x01UL << 14);
		} else {
			vmm_printf("Timer %d interrupt can't be delievered to FSB.\n");
			return VMM_EFAIL;
		}
	}

	if (flags & HPET_TIMER_FORCE_32BIT) {
		tmr |= (0x01UL << 8);
	}

	if (flags & HPET_TIMER_PERIODIC) {
		if (_v & (0x01UL << 4)) {
			tmr |= (0x01UL << 8);
			tmr |= (0x01UL << 6);
		}
	}

	if (!(flags & HPET_TIMER_INT_EDGE)) {
		tmr |= (0x01UL << 1);
	}

	hpet_timer_write(hpet->vbase, HPET_TIMER_N_CONF_BASE(timer_id), tmr);

	return VMM_OK;
}

static int __init hpet_init(void)
{
	u64 cap_reg;

	if (!hpet) {
		hpet = (struct hpet *)vmm_malloc(sizeof(struct hpet));
		BUG_ON(hpet == NULL);

		hpet->pbase = acpi_get_hpet_base_next();
		BUG_ON(hpet->pbase == 0);

		hpet->vbase = vmm_host_iomap(hpet->pbase, VMM_PAGE_SIZE);
		BUG_ON(hpet->vbase == 0);

		cap_reg = hpet_timer_read(hpet->vbase, HPET_GEN_CAP_ID_BASE);
		vmm_printf("HPET Rev ID: %x\n", cap_reg & 0xFF);
		vmm_printf("HPET Number of timers: %d\n", ((cap_reg >> 8) & 0x1f) + 1);
		vmm_printf("HPET Timers are %d bits.\n",
			   (cap_reg & (0x1UL << 13) ? 64 : 32));
		vmm_printf("HPET Legacy routing capable? %s\n",
			   cap_reg & (0x1UL << 15) ? "Yes" : "No");
		vmm_printf("HPET Counter CLK Period 0x%x\n", cap_reg >> 32);
	} else
		vmm_printf("Attempt to reinitialize HPET.\n");

	return VMM_OK;
}

static u32 hpet_main_counter_period_femto(void)
{
	u64 cap_reg = hpet_timer_read(hpet->vbase, HPET_GEN_CAP_ID_BASE);
	return (u32)(cap_reg >> 32);
}

static vmm_irq_return_t
hpet_clockchip_irq_handler(u32 irq_no,
			   arch_regs_t * regs,
			   void *dev)
{
	/* clear the interrupt */
	/* call the event handler */

	for (;;);

	return VMM_IRQ_HANDLED;
}

static void hpet_clockchip_set_mode(enum vmm_clockchip_mode mode,
				    struct vmm_clockchip *cc)
{
	struct hpet_clockchip *hpet_tmr = container_of(cc, struct hpet_clockchip, clkchip);

	BUG_ON(hpet_tmr == NULL);

	switch (mode) {
	case VMM_CLOCKCHIP_MODE_PERIODIC:
		hpet_set_timer_periodic(hpet_tmr->hpet_timer_id);
		break;
	case VMM_CLOCKCHIP_MODE_ONESHOT:
		hpet_set_timer_non_periodic(hpet_tmr->hpet_timer_id);
		break;
	case VMM_CLOCKCHIP_MODE_UNUSED:
	case VMM_CLOCKCHIP_MODE_SHUTDOWN:
	default:
		break;
	}
}

static int hpet_clockchip_set_next_event(unsigned long next,
					 struct vmm_clockchip *cc)
{
	struct hpet_clockchip *hpet_tmr = container_of(cc, struct hpet_clockchip, clkchip);
	BUG_ON(hpet_tmr == NULL);

	hpet_disarm_timer(hpet_tmr->hpet_timer_id);
	hpet_timer_write(hpet->vbase,
			 HPET_TIMER_N_COMP_BASE(hpet_tmr->hpet_timer_id), next);
	hpet_arm_timer(hpet_tmr->hpet_timer_id);

	vmm_printf("%s: comp value: %lx\n", __func__,
		   hpet_timer_read(hpet->vbase,
				   HPET_TIMER_N_COMP_BASE(hpet_tmr->hpet_timer_id)));
	return 0;
}

static int hpet_clockchip_expire(struct vmm_clockchip *cc)
{
#if 0
	u32 i;
	struct sp804_clockchip *tcc = cc->priv;
	unsigned long ctrl = vmm_readl((void *)(tcc->base + TIMER_CTRL));

	ctrl &= ~TIMER_CTRL_ENABLE;
	vmm_writel(ctrl, (void *)(tcc->base + TIMER_CTRL));
	vmm_writel(1, (void *)(tcc->base + TIMER_LOAD));
	vmm_writel(ctrl | TIMER_CTRL_ENABLE, (void *)(tcc->base + TIMER_CTRL));

	while (!vmm_readl((void *)(tcc->base + TIMER_MIS))) {
		for (i = 0; i < 100; i++);
	}
#endif

	return 0;
}

static int __init hpet_clockchip_init(u8 timer_id, const char *chip_name,
				      u32 irqno, u32 target_cpu)
{
	int rc, pinno;
	struct hpet_clockchip *cc;
	u32 int_dest;

	int_dest = hpet_get_int_route(timer_id);
	for (pinno = 0; pinno < 32; pinno++) {
		if (int_dest & (0x01UL << pinno)) {
			rc = hpet_initialize_timer(timer_id,
						   pinno, HPET_TIMER_INT_EDGE);
			BUG_ON(rc != VMM_OK);

			/* route the IOAPIC pin to CPU IRQ/Exception vector */
			ioapic_route_pin_to_irq(pinno, irqno);
			break;
		}
	}

	BUG_ON(pinno == 32);

	vmm_printf("Initialized HPET timer %d and routed its "
		   "interrupt to %d pin on I/O APIC.\n", timer_id, pinno);

	cc = vmm_zalloc(sizeof(struct hpet_clockchip));
	if (!cc) {
		return VMM_EFAIL;
	}

	cc->base = hpet->vbase;
	cc->hpet_timer_id = timer_id;
	cc->clkchip.name = chip_name;
	cc->clkchip.hirq = 20;
	cc->clkchip.rating = 250;
#ifdef CONFIG_SMP
	cc->clkchip.cpumask = vmm_cpumask_of(target_cpu);
#else
	cc->clkchip.cpumask = cpu_all_mask;
#endif
	cc->clkchip.features =
		VMM_CLOCKCHIP_FEAT_PERIODIC | VMM_CLOCKCHIP_FEAT_ONESHOT;
	/* FIXME_FIRST */
	cc->clkchip.mult = vmm_clockchip_khz2mult(10000, 20);
	cc->clkchip.shift = 20;
	cc->clkchip.min_delta_ns = hpet_main_counter_period_femto()/1000000;
	cc->clkchip.max_delta_ns =
			vmm_clockchip_delta2ns(0xFFFFFFFF, &cc->clkchip);
	cc->clkchip.set_mode = &hpet_clockchip_set_mode;
	cc->clkchip.set_next_event = &hpet_clockchip_set_next_event;
	cc->clkchip.expire = &hpet_clockchip_expire;
	cc->clkchip.priv = cc;

	/* Register interrupt handler */
	if ((rc = vmm_host_irq_register(20, "hpet_int",
					&hpet_clockchip_irq_handler, cc))) {
		return rc;
	}

#ifdef CONFIG_SMP
	/* Set host irq affinity to target cpu */
	if ((rc = vmm_host_irq_set_affinity(20,
					    vmm_cpumask_of(target_cpu),
					    TRUE))) {
		return rc;
	}
#endif

	return vmm_clockchip_register(&cc->clkchip);
}

int arch_clockchip_init(void)
{
	BUG_ON(hpet_init() != VMM_OK);

	return hpet_clockchip_init(DEFAULT_HPET_SYS_TIMER, "system_timer",
				   20, 0);
}

/*****************************************
 *          HPET CLOCK SOURCE            *
 *****************************************/
static u64 hpet_clocksource_read(struct vmm_clocksource * cs)
{
	return hpet_main_counter_val();
}

static int hpet_clocksource_enable(struct vmm_clocksource * cs)
{
	hpet_enable_main_counter();

	return VMM_OK;
}

static void hpet_clocksource_disable(struct vmm_clocksource * cs)
{
	hpet_disable_main_counter();
}

static struct vmm_clocksource hpet_cs =
{
	.name = "hpet_clksrc",
	.rating = 300,
	.mask = 0xFFFFFFFF,
	.shift = 20,
	.read = &hpet_clocksource_read,
	.disable = &hpet_clocksource_disable,
	.enable = &hpet_clocksource_enable,
};

static int hpet_clocksource_init(void)
{
	u64 t1, t2;

	BUG_ON(hpet_init() != VMM_OK);

	vmm_printf("Initializing HPET main counter.\n");
	/* stop the free running counter. */
	hpet_disable_main_counter();

	vmm_printf("Verifying if the HPET main counter can count... ");
	hpet_clocksource_enable(NULL);
	t1 = hpet_clocksource_read(NULL);
	t2 = hpet_clocksource_read(NULL);

	if (t1 != t2 && t2 > t1)
		vmm_printf("Yes.\n");
	else {
		vmm_panic("No.\n");
	}

	hpet_clocksource_disable(NULL);

	hpet_cs.mult = vmm_clocksource_khz2mult(10000, 20);

	return vmm_clocksource_register(&hpet_cs);
}

int arch_clocksource_init(void)
{
	return hpet_clocksource_init();
}
