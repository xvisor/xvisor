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
#include <acpi.h>
#include <cpu_apic.h>

#undef __DEBUG

#if defined(__DEBUG)
#define debug_print(fmt, args...) vmm_printf(fmt, ##args);
#else
#define debug_print(fmt, args...) { }
#endif

#define DEFAULT_HPET_SYS_TIMER		0 /* Timer 0 is system timer */
#define HPET_CAP_LEGACY_SUPPORT		(0x01UL << 0)
#define HPET_CAP_FSB_DELIVERY		(0x01UL << 1)

#define HPET_GEN_CAP_ID_BASE		(0x00)
#define HPET_GEN_CONF_BASE		(0x10)
#define HPET_GEN_INT_STATUS_BASE	(0x20)
#define HPET_GEN_MAIN_CNTR_BASE		(0xF0)
#define HPET_TIMER_N_CONF_BASE(__n)	(0x100 + 0x20 * __n)
#define HPET_TIMER_N_COMP_BASE(__n)	(0x108 + 0x20 * __n)

#define HPET_BLOCK_REV_ID(x)		(((u64)x) & 0xFFUL)
#define HPET_BLOCK_NR_TIMERS(x)		((((u64)x) >> 8) & 0x1fUL)
#define HPET_BLOCK_CNTR_SIZE(x)		(((((u64)x) >> 13) & 0x1UL) ? 64 : 32)
#define HPET_BLOCK_HAS_LEGACY_ROUTE(x)	((((u64)x) >> 15) & 0x1UL)
#define HPET_BLOCK_CLK_PERIOD(x)	((((u64)x) >> 32) & 0xFFFFFFFFUL)

#define HPET_TIMER_PERIODIC		(0x01UL << 0)
#define HPET_TIMER_INT_TO_FSB		(0x01UL << 1)
#define HPET_TIMER_FORCE_32BIT		(0x01UL << 2)
#define HPET_TIMER_INT_EDGE		(0x01UL << 3)

/************************************************************************
 * The system can have multiple HPET chips. Each chip can have upto 8
 * timer blocks. Each block can have upto 32 timers.
 ************************************************************************/
struct hpet_devices {
	int nr_chips;			/* Number of physical HPET on mother board */
	struct dlist chip_list;		/* List of all such chips */
};

struct hpet_chip {
	int nr_blocks;
	struct dlist head;
	struct dlist block_list;	/* list of all blocks in chip. */
	struct hpet_devices *parent;	/* parent HPET device */
};

struct hpet_block {
	int nr_timers;			/* Number of timers in this block */
	physical_addr_t pbase;		/* physical base of the block */
	virtual_addr_t vbase;		/* virtual base */
	u64 capabilities;		/* capabilities of the block */
	struct dlist head;
	struct dlist timer_list;	/* list of timers in this block */
	struct hpet_chip *parent;	/* parent hpet chip */
};

struct hpet_timer {
	u32 id;
	u32 conf_cap;
	u32 is_busy;			/* If under use */
	struct vmm_clockchip *clkchip;	/* clock chip for this timer */
	struct vmm_clocksource *clksrc;	/* clock source for this timer */
	struct dlist head;
	struct hpet_block *parent;	/* parent block of this timer */
};

struct hpet_devices hpet_devices;

/*****************************************************
 *             HPET READ/WRITE FUNCTIONS             *
 *****************************************************/
static inline void hpet_write(virtual_addr_t vbase, u32 reg_offset, u64 val)
{
	vmm_out_le64((u64 *)(vbase + reg_offset), val);
}

static inline u64 hpet_read(virtual_addr_t vbase, u32 reg_offset)
{
	return vmm_in_le64((u64 *)(vbase + reg_offset));
}

/*****************************************************
 *                  FEATURE READ/WRITE               *
 *****************************************************/
static u32 hpet_timer_get_int_route(struct hpet_timer *timer)
{
	u64 _v = hpet_read(timer->parent->vbase, HPET_TIMER_N_CONF_BASE(timer->id));
	return (u32)(_v >> 32);
}

static void hpet_enable_main_counter(struct hpet_timer *timer)
{
	u64 _v = hpet_read(timer->parent->vbase, HPET_GEN_CONF_BASE);
	_v |= 0x01;
	hpet_write(timer->parent->vbase, HPET_GEN_CONF_BASE, _v);
}

static void hpet_disable_main_counter(struct hpet_timer *timer)
{
	u64 _v = hpet_read(timer->parent->vbase, HPET_GEN_CONF_BASE);
	_v &= ~0x01;
	hpet_write(timer->parent->vbase, HPET_GEN_CONF_BASE, _v);
}

static void hpet_arm_timer(struct hpet_timer *timer)
{
	u64 _v = hpet_read(timer->parent->vbase, HPET_TIMER_N_CONF_BASE(timer->id));
	_v |= (0x01UL << 2);
	hpet_write(timer->parent->vbase, HPET_TIMER_N_CONF_BASE(timer->id), _v);
}

static void hpet_disarm_timer(struct hpet_timer *timer)
{
	u64 _v = hpet_read(timer->parent->vbase, HPET_TIMER_N_CONF_BASE(timer->id));
	_v &= ~(0x01UL << 2);
	hpet_write(timer->parent->vbase, HPET_TIMER_N_CONF_BASE(timer->id), _v);
}

static int hpet_initialize_timer(struct hpet_timer *timer, u8 dest_int, u32 flags)
{
	u64 tmr = 0;
	u64 _v = hpet_read(timer->parent->vbase, HPET_TIMER_N_CONF_BASE(timer->id));

	if (dest_int && !(flags & HPET_TIMER_INT_TO_FSB)) {
		if ((_v >> 32) & (0x01UL << dest_int)) {
			tmr |= (((u64)dest_int) << 9);
		} else {
			vmm_printf("Timer %d interrupt can't be routed to %d on IOAPIC.\n",
				   timer->id, dest_int);
			return VMM_EFAIL;
		}
	} else if ((flags & HPET_TIMER_INT_TO_FSB)) {
		if (_v & (0x01UL << 15)) {
			tmr |= (0x01UL << 14);
		} else {
			vmm_printf("Timer %d interrupt can't be delievered to FSB.\n", timer->id);
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

	hpet_write(timer->parent->vbase, HPET_TIMER_N_CONF_BASE(timer->id), tmr);

	return VMM_OK;
}

int __cpuinit hpet_init(void)
{
	u32 nr_hpet_chips = acpi_get_nr_hpet_chips();
	struct hpet_chip *chip;
	struct hpet_block *block;
	struct hpet_timer *timer;
	struct dlist *l;
	int i, j, k;

	/* Need at least one HPET as system timer */
	BUG_ON(nr_hpet_chips == 0);

	/* Initialize the hpet list */
	INIT_LIST_HEAD(&hpet_devices.chip_list);

	for (i = 0; i < nr_hpet_chips; i++) {
		chip = (struct hpet_chips *)vmm_malloc(sizeof(struct hpet_chip));
		chip->nr_hpet_blocks = acpi_get_nr_hpet_blocks(i);
		INIT_LIST_HEAD(&chip->block_list);
		list_add(&chip->head, &hpet_devices.chip_list);

		for (j = 0; j < chip->nr_hpet_blocks; j++) {
			block = (struct hpet_block *)vmm_malloc(sizeof(struct hpet_block));
			BUG_ON(block == NULL);

			/*
			 * Find the number of timers in this block and initialize
			 * each one of them.
			 */
			block->nr_timers = hpet_nr_timers_in_block(block);

			block->pbase = acpi_get_hpet_block_base(j);
			BUG_ON(block->pbase == NULL);

			/* Make the block and its timer accessible to us. */
			block->vbase = vmm_host_iomap(block->pbase, VMM_PAGE_SIZE);
			BUG_ON(block->vbase == NULL);

			block->parent = chip;
			block->capabilities = timer_read(block->vbase, HPET_GEN_CAP_ID_BASE);
			block->nr_timers = HPET_BLOCK_NR_TIMERS(block->capabilities);
			INIT_LIST_HEAD(&block->timer_list);

			list_add(&block->head, &chip->block_list);

			for (k = 0; k < block->nr_timers; k++) {
				timer = (struct hpet_timer *)vmm_malloc(sizeof(struct hpet_timer));
				BUG_ON(timer == NULL);
				timer->id = k;
				timer->is_busy = 0;
				timer->clkchip = NULL;
				timer->clksrc = NULL;
				timer->parent = block;
				timer->capabilities = hpet_read(timer->parent->vbase,
								HPET_GEN_CAP_ID_BASE);
				list_add(&timer->head, &block->timer_list);
			}
		}
	}

	return VMM_OK;
}

static u32 hpet_main_counter_period_femto(struct hpet_chip *chip)
{
	u64 cap_reg = hpet_timer_read(chip->vbase, HPET_GEN_CAP_ID_BASE);
	return (u32)(cap_reg >> 32);
}

static vmm_irq_return_t hpet_clockchip_irq_handler(u32 irq_no,
						   arch_regs_t * regs,
						   void *dev)
{
	struct hpet_clockchip *cc = dev;

	/* clear the interrupt */
	/* call the event handler */

	return VMM_IRQ_HANDLED;
}

static void hpet_clockchip_set_mode(enum vmm_clockchip_mode mode,
				    struct vmm_clockchip *cc)
{
	switch (mode) {
	case VMM_CLOCKCHIP_MODE_PERIODIC:
		break;
	case VMM_CLOCKCHIP_MODE_ONESHOT:
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
	return 0;
}

static int hpet_clockchip_expire(struct vmm_clockchip *cc)
{
	return 0;
}

int __cpuinit hpet_clockchip_init(u8 timer_id, const char *chip_name,
				  u32 irqno, u32 target_cpu)
{
	int rc;
	struct hpet_clockchip *cc;
	u32 int_dest;

	/* first disable the free running counter */
	hpet_disable_main_counter();

	int_dest = hpet_get_int_route(timer_id);
	for (rc = 0; rc < 32; rc++) {
		if (int_dest & (0x01UL << rc)) {
			rc = hpet_initialize_timer(timer_id,
						   rc, HPET_TIMER_INT_EDGE);
			BUG_ON(rc != VMM_OK, "Failed to initialize the system timer.\n");

			/* route the IOAPIC pin to CPU IRQ/Exception vector */
			ioapic_route_pin_to_irq(rc, irqno);
			break;
		}
	}

	BUG_ON(rc == 32);

	debug_printf("Initialized HPET timer %d and routed its "
		     "interrupt to %d pin on I/O APIC.\n", timer_id, rc);

	hpet_arm_timer(rc);
	debug_printf("HPET Timer %d has been armed.\n", timer_id);

	cc = vmm_malloc(sizeof(struct hpet_clockchip));
	if (!cc) {
		return VMM_EFAIL;
	}

	cc->base = hpet->vbase;
	cc->hpet_timer_id = timer_id;
	cc->clkchip.name = chip_name;
	cc->clkchip.hirq = 20;
	cc->clkchip.rating = rating;
#ifdef CONFIG_SMP
	cc->clkchip.cpumask = vmm_cpumask_of(target_cpu);
#else
	cc->clkchip.cpumask = cpu_all_mask;
#endif
	cc->clkchip.features =
		VMM_CLOCKCHIP_FEAT_PERIODIC | VMM_CLOCKCHIP_FEAT_ONESHOT;
	cc->clkchip.mult = vmm_clockchip_hz2mult(freq_hz, 32);
	cc->clkchip.shift = 32;
	cc->clkchip.min_delta_ns = hpet_main_counter_period_femto()/1000000;
	cc->clkchip.max_delta_ns =
			vmm_clockchip_delta2ns(0xFFFFFFFF, &cc->clkchip);
	cc->clkchip.set_mode = &hpet_clockchip_set_mode;
	cc->clkchip.set_next_event = &hpet_clockchip_set_next_event;
	cc->clkchip.expire = &hpet_clockchip_expire;
	cc->clkchip.priv = cc;

	/* Register interrupt handler */
	if ((rc = vmm_host_irq_register(20,
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

int __cpuinit arch_clockchip_init(void)
{
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

static int __init hpet_clocksource_init(struct hpet_timer *timer)
{
	u64 t1, t2;
	vmm_clocksource *clksrc = NULL;

	debug_print("Initializing HPET main counter.\n");

	clksrc = (vmm_clocksource *)vmm_malloc(sizeof(struct vmm_clocksource));
	BUG_ON(clksrc == NULL);

	clksrc->name = "hpet_clksrc";
	clksrc->rating = 300;
	clksrc->mask = 0xFFFFFFFF;
	clksrc->shift = 20;
	clksrc->read = &hpet_clocksource_read;
	clksrc->disable = &hpet_clocksource_disable;
	clksrc->enable = &hpet_clocksource_enable;
	clksrc->priv = (void *)timer;

	timer->clksrc = clksrc;

	/* stop the free running counter. */
	hpet_disable_main_counter(timer);

	vmm_printf("Verifying if the HPET main counter can count... ");
	hpet_enable_main_counter(timer);

	t1 = hpet_clocksource_read(timer);
	t2 = hpet_clocksource_read(timer);

	if (t1 != t2 && t2 > t1) {
		vmm_printf("Yes.\n");
	} else {
		vmm_panic("No.\n");
	}

	return vmm_clocksource_register(clksrc);
}

int __init arch_clocksource_init(void)
{

	return hpet_clocksource_init();
}

