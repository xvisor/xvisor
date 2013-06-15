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
#include <vmm_wallclock.h>
#include <acpi.h>
#include <cpu_apic.h>
#include <hpet.h>

#undef DEBUG

#if defined(DEBUG)
#define debug_print(fmt, args...) vmm_printf(fmt, ##args);
#else
#define debug_print(fmt, args...) { }
#endif

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
static int hpet_in_legacy_mode(struct hpet_timer *timer)
{
	u64 _v = hpet_read(timer->parent->vbase, HPET_GEN_CONF_BASE);
	return (_v & (0x01 << 1) ? 1 : 0);
}

static u32 hpet_timer_get_int_route(struct hpet_timer *timer)
{
	u64 _v = hpet_read(timer->parent->vbase, HPET_TIMER_N_CONF_BASE(timer->timer_id));
	debug_print("%s: int route: %x\n", __func__, (u32)(_v >> 32));
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

static u64 hpet_read_main_counter(struct hpet_timer *timer)
{
	return hpet_read(timer->parent->vbase, HPET_GEN_MAIN_CNTR_BASE);
}

static void hpet_write_main_counter(struct hpet_timer *timer, u64 val)
{
	hpet_write(timer->parent->vbase, HPET_GEN_MAIN_CNTR_BASE, val);
}

static void hpet_arm_timer(void *data)
{
	struct hpet_timer *timer = (struct hpet_timer *)data;
	u64 _v = hpet_read(timer->parent->vbase, HPET_TIMER_N_CONF_BASE(timer->timer_id));
	_v |= (0x01UL << 2);
	hpet_write(timer->parent->vbase, HPET_TIMER_N_CONF_BASE(timer->timer_id), _v);
	debug_print("%s: timer %d armed\n", __func__, timer->timer_id);
}

#if 0
static void hpet_disarm_timer(void *data)
{
	struct hpet_timer *timer = (struct hpet_timer *)data;
	u64 _v = hpet_read(timer->parent->vbase, HPET_TIMER_N_CONF_BASE(timer->timer_id));
	_v &= ~(0x01UL << 2);
	hpet_write(timer->parent->vbase, HPET_TIMER_N_CONF_BASE(timer->timer_id), _v);
	debug_print("%s: timer %d disarmed\n", __func__, timer->timer_id);
}
#endif

static int hpet_initialize_timer(struct hpet_timer *timer, u8 dest_int, u32 flags)
{
	u64 tmr = 0;
	u64 _v = hpet_read(timer->parent->vbase, HPET_TIMER_N_CONF_BASE(timer->timer_id));

	if (dest_int && !(flags & HPET_TIMER_INT_TO_FSB)) {
		if (hpet_in_legacy_mode(timer) && (timer->timer_id == 0)) {
			tmr |= (((u64)dest_int) << 9);
		} else if ((_v >> 32) & (0x01UL << dest_int)) {
			tmr |= (((u64)dest_int) << 9);
		} else {
			vmm_printf("Timer %d interrupt can't be routed to %d on IOAPIC.\n",
				   timer->timer_id, dest_int);
			return VMM_EFAIL;
		}
	} else if ((flags & HPET_TIMER_INT_TO_FSB)) {
		if (_v & (0x01UL << 15)) {
			tmr |= (0x01UL << 14);
		} else {
			vmm_printf("Timer %d interrupt can't be delievered to FSB.\n", timer->timer_id);
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

	debug_print("%s: Writing %x to configuration register\n",
		    __func__, tmr);
	hpet_write(timer->parent->vbase, HPET_TIMER_N_CONF_BASE(timer->timer_id), tmr);

	return VMM_OK;
}

static struct hpet_timer *get_timer_from_id(timer_id_t timer_id)
{
	int chip_no = HPET_TIMER_CHIP(timer_id);
	int block_no = HPET_TIMER_BLOCK(timer_id);
	int timer_no = HPET_TIMER(timer_id);
	struct dlist *clist, *blist, *tlist;
	struct hpet_chip *chip;
	struct hpet_block *block;
	struct hpet_timer *timer;

	list_for_each(clist, &hpet_devices.chip_list) {
		chip = list_entry(clist, struct hpet_chip, head);

		if (!chip) goto _err;

		if (chip->chip_id != chip_no) continue;

		list_for_each(blist, &chip->block_list) {
			block = list_entry(blist, struct hpet_block, head);

			if (block->block_id != block_no) continue;

			list_for_each(tlist, &block->timer_list) {
				timer = list_entry(tlist, struct hpet_timer, head);

				if (timer == NULL) goto _err;

				if (timer->timer_id != timer_no) continue;

				return timer;
			}
		}		
	}

 _err:
	return NULL;
}

int __init hpet_init(void)
{
	u32 nr_hpet_chips = 0, *aval;
	struct hpet_chip *chip;
	struct hpet_block *block;
	struct hpet_timer *timer;
	int i, j, k;
	struct vmm_devtree_node *node;
	char hpet_nm[512];
	physical_addr_t *base;

	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				VMM_DEVTREE_MOTHERBOARD_NODE_NAME
				VMM_DEVTREE_PATH_SEPARATOR_STRING
				"HPET");

	BUG_ON(node == NULL);
	aval = vmm_devtree_attrval(node, VMM_DEVTREE_NR_HPET_ATTR_NAME);
	nr_hpet_chips = *aval;

	/* Need at least one HPET as system timer */
	BUG_ON(nr_hpet_chips == 0);

	/* Initialize the hpet list */
	INIT_LIST_HEAD(&hpet_devices.chip_list);

	for (i = 0; i < nr_hpet_chips; i++) {
		chip = (struct hpet_chip *)vmm_malloc(sizeof(struct hpet_chip));
		chip->chip_id = i;
		/* Fold blocks on chip. 1 chip == 1 block. */
		chip->nr_blocks = 1;
		INIT_LIST_HEAD(&chip->block_list);
		list_add(&chip->head, &hpet_devices.chip_list);

		for (j = 0; j < chip->nr_blocks; j++) {
			block = (struct hpet_block *)vmm_malloc(sizeof(struct hpet_block));
			BUG_ON(block == NULL);

			/* FIXME: read from devtree */
			block->nr_timers = 32;

			vmm_sprintf(hpet_nm, VMM_DEVTREE_PATH_SEPARATOR_STRING
				VMM_DEVTREE_MOTHERBOARD_NODE_NAME
				VMM_DEVTREE_PATH_SEPARATOR_STRING
				"HPET"
				VMM_DEVTREE_PATH_SEPARATOR_STRING
				VMM_DEVTREE_HPET_NODE_FMT, j);

			node = vmm_devtree_getnode(hpet_nm);
			BUG_ON(node == NULL);

			base = vmm_devtree_attrval(node, VMM_DEVTREE_HPET_PADDR_ATTR_NAME);
			block->pbase = *base;
			BUG_ON(block->pbase == 0);

			/* Make the block and its timer accessible to us. */
			block->vbase = vmm_host_iomap(block->pbase, VMM_PAGE_SIZE);
			BUG_ON(block->vbase == 0);

			block->block_id = j;
			block->parent = chip;
			block->capabilities = hpet_read(block->vbase, HPET_GEN_CAP_ID_BASE);
			block->nr_timers = HPET_BLOCK_NR_TIMERS(block->capabilities);
			INIT_LIST_HEAD(&block->timer_list);

			list_add(&block->head, &chip->block_list);

			for (k = 0; k < block->nr_timers; k++) {
				timer = (struct hpet_timer *)vmm_malloc(sizeof(struct hpet_timer));
				BUG_ON(timer == NULL);
				/* TODO FIXME: Block is only a software concept, the timer id will
				 * go from 0 to 31 for hardware. timer_id should be per block. */
				timer->timer_id = k;
				timer->is_busy = 0;
				timer->parent = block;
				timer->conf_cap = hpet_read(timer->parent->vbase,
							    HPET_GEN_CAP_ID_BASE);
				timer->hpet_period = HPET_BLOCK_CLK_PERIOD(timer->conf_cap);
				BUG_ON(timer->hpet_period == 0);
				timer->hpet_freq = udiv64(FSEC_PER_SEC, timer->hpet_period);
				list_add(&timer->head, &block->timer_list);
			}
		}
	}

	return VMM_OK;
}

static vmm_irq_return_t hpet_clockchip_irq_handler(int irq_no, void *dev)
{
	struct hpet_timer *timer = (struct hpet_timer *)dev;

	if (unlikely(!timer->clkchip.event_handler))
		return VMM_IRQ_NONE;

	/* call the event handler */
	timer->clkchip.event_handler(&timer->clkchip);

	return VMM_IRQ_HANDLED;
}

static void hpet_clockchip_set_mode(enum vmm_clockchip_mode mode,
				    struct vmm_clockchip *cc)
{

	BUG_ON(cc == NULL);

	switch (mode) {
	case VMM_CLOCKCHIP_MODE_PERIODIC:
		/* Not supported currently */
		BUG_ON(0);
		break;
	case VMM_CLOCKCHIP_MODE_ONESHOT:
		debug_print("%s: Configuring timer in one-shot mode.\n",
			    __func__);
		struct hpet_timer *timer = container_of(cc, struct hpet_timer, clkchip);
		BUG_ON(timer == NULL);
		/* FIXME: N_CONF_BASE should be passed a timer id */
		u64 _v = hpet_read(timer->parent->vbase, HPET_TIMER_N_CONF_BASE(0));
		_v &= ~(0x01ULL << 3);
		hpet_write(timer->parent->vbase, HPET_TIMER_N_CONF_BASE(0), _v);
		break;
	case VMM_CLOCKCHIP_MODE_UNUSED:
	case VMM_CLOCKCHIP_MODE_SHUTDOWN:
	default:
		/* See later */
		BUG_ON(0);
		break;
	}
}

static int hpet_clockchip_set_next_event(unsigned long next,
					 struct vmm_clockchip *cc)
{
	struct hpet_timer *timer = container_of(cc, struct hpet_timer, clkchip);
	BUG_ON(timer == NULL);
	u64 res;

	if (unlikely(!timer->armed)) {
		timer->armed = 1;
		hpet_arm_timer((void *)timer);
	}

	res = hpet_read_main_counter(timer);
	res += next;
	hpet_write(timer->parent->vbase, HPET_TIMER_N_COMP_BASE(0), res);
	next = hpet_read_main_counter(timer);
	BUG_ON(next > res);

	return 0;
}

int __cpuinit hpet_clockchip_init(timer_id_t timer_id, 
				  const char *chip_name,
				  u32 target_cpu)
{
	int rc;
	u32 int_dest;
	struct hpet_timer *timer = get_timer_from_id(timer_id);

	if (unlikely(timer == NULL)) {
		return VMM_ENODEV;
	}

	/* first disable the free running counter */
	hpet_disable_main_counter(timer);
	hpet_write_main_counter(timer, 0);

	if (hpet_in_legacy_mode(timer)) {
		/* legacy mode hpet overrides PIT interrupt at pin 2 of IOAPIC */
		int_dest = 2;
	} else {
		int_dest = hpet_timer_get_int_route(timer);
		for (rc = 0; rc < 32; rc++) {
			if (int_dest & (0x01UL << rc)) {
				debug_print("%s: interrupt pin to route: %d\n",
					    __func__, rc);
				int_dest = rc;
				break;
			}
		}
		BUG_ON(rc == 32);
	}

	rc = hpet_initialize_timer(timer, int_dest, HPET_TIMER_INT_EDGE);
	BUG_ON(rc != VMM_OK);

	timer->clkchip.name = chip_name;
	timer->clkchip.hirq = int_dest;
	timer->clkchip.rating = 250;
#ifdef CONFIG_SMP
	timer->clkcip.cpumask = vmm_cpumask_of(target_cpu);
#else
	timer->clkchip.cpumask = cpu_all_mask;
#endif
	debug_print("%s: Hpet Freq: %l Period: %l\n", __func__, timer->hpet_freq, timer->hpet_period);
	timer->clkchip.features = VMM_CLOCKCHIP_FEAT_PERIODIC | VMM_CLOCKCHIP_FEAT_ONESHOT;
	vmm_clocks_calc_mult_shift(&timer->clkchip.mult, &timer->clkchip.shift, NSEC_PER_SEC, timer->hpet_freq, 5);
	timer->clkchip.min_delta_ns = 100000;
	debug_print("%s: Min Delts NS: %d\n", __func__, timer->clkchip.min_delta_ns);
	debug_print("%s: mult: %l shift: %l\n", __func__, timer->clkchip.mult, timer->clkchip.shift);
	timer->clkchip.max_delta_ns = vmm_clockchip_delta2ns(0x7FFFFFFFFFFFFFFFULL, &timer->clkchip);
	debug_print("%s: Max delta ns: %l\n", __func__, timer->clkchip.max_delta_ns);
	timer->clkchip.set_mode = &hpet_clockchip_set_mode;
	timer->clkchip.set_next_event = &hpet_clockchip_set_next_event;
	timer->clkchip.priv = (void *)timer;

#ifdef CONFIG_SMP
	/* Set host irq affinity to target cpu */
	if ((rc = vmm_host_irq_set_affinity(irqno,
					    vmm_cpumask_of(target_cpu),
					    TRUE))) {
		return rc;
	}
#endif

	vmm_clockchip_register(&timer->clkchip);
	timer->armed = 0;
	rc = vmm_host_irq_register(int_dest, chip_name,
				hpet_clockchip_irq_handler,
				timer);
	BUG_ON(rc != VMM_OK);

	return VMM_OK;
}

/*****************************************
 *          HPET CLOCK SOURCE            *
 *****************************************/
static u64 hpet_clocksource_read(struct vmm_clocksource *cs)
{
	struct hpet_timer *timer = container_of(cs, struct hpet_timer, clksrc);
	BUG_ON(timer == NULL);

	debug_print("%s\n", __func__);
	return hpet_read_main_counter(timer);
}

static int hpet_clocksource_enable(struct vmm_clocksource *cs)
{
	struct hpet_timer *timer = container_of(cs, struct hpet_timer, clksrc);
	BUG_ON(timer == NULL);
	debug_print("Enabling clocksource.\n");
	hpet_enable_main_counter(timer);

	return VMM_OK;
}

static void hpet_clocksource_disable(struct vmm_clocksource *cs)
{
	struct hpet_timer *timer = container_of(cs, struct hpet_timer, clksrc);
	BUG_ON(timer == NULL);
	hpet_disable_main_counter(timer);
}

int __init hpet_clocksource_init(timer_id_t timer_id,
				 const char *chip_name)
{
	u64 t1, t2;
	struct hpet_timer *timer = get_timer_from_id(DEFAULT_HPET_SYS_TIMER);

	if (unlikely(timer == NULL)) {
		return VMM_ENODEV;
	}

	debug_print("Initializing HPET main counter.\n");

	timer->clksrc.name = chip_name;
	timer->clksrc.rating = 300;
	timer->clksrc.mask = 0xFFFFFFFFULL;
	vmm_clocks_calc_mult_shift(&timer->clksrc.mult, &timer->clksrc.shift, timer->hpet_freq, NSEC_PER_SEC, 5);
	debug_print("%s: Mult 0x%x shift: 0x%x\n", __func__, timer->clksrc.mult, timer->clksrc.shift);
	timer->clksrc.read = &hpet_clocksource_read;
	timer->clksrc.disable = &hpet_clocksource_disable;
	timer->clksrc.enable = &hpet_clocksource_enable;
	timer->clksrc.priv = (void *)timer;

	u64 _v = hpet_read(timer->parent->vbase, HPET_GEN_CONF_BASE);
	_v |= (0x01ULL << 1);
	hpet_write(timer->parent->vbase, HPET_GEN_CONF_BASE, _v);

	vmm_printf("Verifying if the HPET main counter can count... ");
	hpet_enable_main_counter(timer);

	t1 = hpet_clocksource_read(&timer->clksrc);
	t2 = hpet_clocksource_read(&timer->clksrc);

	hpet_disable_main_counter(timer);

	hpet_write_main_counter(timer, 0);

	if (t1 != t2 && t2 > t1) {
		vmm_printf("Yes.\n");
	} else {
		vmm_panic("No.\n");
	}

	return vmm_clocksource_register(&timer->clksrc);
}

