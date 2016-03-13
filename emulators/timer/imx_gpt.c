/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
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
 * @file imx_gpt.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief i.MX GPT timer emulator.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_timer.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_devemu.h>
#include <vmm_stdio.h>
#include <vmm_timer.h>
#include <vmm_scheduler.h>
#include <libs/mathlib.h>

#define MODULE_DESC		"i.MX GPT emulator"
#define MODULE_AUTHOR		"Jimmy Durand Wesolowski"
#define MODULE_LICENSE		"GPL"
#define MODULE_IPRIORITY	0
#define	MODULE_INIT		imx_gpt_emulator_init
#define	MODULE_EXIT		imx_gpt_emulator_exit

#define GPT_CR			0
#define GPT_CR_SW		(1 << 15)
#define GPT_CR_FRR		(1 << 9)
#define GPT_CR_CLKSRC_MASK	(7 << 6)
#define GPT_CR_STOPEN		(1 << 5)
#define GPT_CR_WAITEN		(1 << 3)
#define GPT_CR_DBGEN		(1 << 2)
#define GPT_CR_ENMOD		(1 << 1)
#define GPT_CR_EN		(1 << 0)
#define GPT_PR			0x4
#define GPT_PR_MASK		0xFFF
#define GPT_SR			0x8
#define GPT_SR_MASK		0x3F
#define GPT_IR			0xC
#define GPT_IR_ROVIE		(1 << 5)
#define GPT_IR_MASK		0x3F
#define GPT_OC1			0x10
#define GPT_OC2			0x14
#define GPT_OC3			0x18
#define GPT_CNT			0x24

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

struct gpt_t {
	struct vmm_guest *guest;
	/* OCR1, 2, and 3 events */
	struct vmm_timer_event event[3];
	/* irq number */
	u32 irq;

	u32 control;
	u32 output_compare[3];
	u32 prescaler;
	u32 freq;
	u32 status;

	/* Xvisor timestamp offset */
	u64 offset;

	/* irq enabling */
	u32 irq_ena;

	/* Freezing value */
	u32 freeze;

	/* Lock to protect registers */
	vmm_rwlock_t lock;
};

static u32 imx_gpt_cnt(struct gpt_t *gpt)
{

	if (likely(!(gpt->control & GPT_CR_EN))) {
		if (gpt->control & GPT_CR_ENMOD) {
			return 0;
		} else {
			return gpt->freeze;
		}
	}
	return (u32)udiv64(vmm_timer_timestamp() - gpt->offset, gpt->freq);
}

static void _imx_gpt_reset(struct gpt_t *gpt)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(gpt->output_compare); ++i) {
		gpt->output_compare[i] = 0xFFFFFFFF;
	}
	gpt->control &= GPT_CR_EN | GPT_CR_ENMOD | GPT_CR_STOPEN |
		GPT_CR_WAITEN | GPT_CR_DBGEN;
	gpt->prescaler = 0;
	gpt->status = 0;
	gpt->irq_ena = 0;
	gpt->offset = vmm_timer_timestamp();
}

static int imx_gpt_emulator_read(struct vmm_emudev *edev,
				 physical_addr_t offset,
				 u32 *dst)
{
	struct gpt_t *gpt = edev->priv;
	u16 reg = offset & ~0x3;

	vmm_read_lock(&gpt->lock);

	switch (reg) {
	case GPT_CR:
		*dst = gpt->control;
		break;
	case GPT_PR:
		*dst = gpt->prescaler;
		break;
	case GPT_SR:
		*dst = gpt->status;
		break;
	case GPT_IR:
		*dst = gpt->irq_ena;
		break;
	case GPT_OC1:
		*dst = gpt->output_compare[0];
		break;
	case GPT_OC2:
		*dst = gpt->output_compare[1];
		break;
	case GPT_OC3:
		*dst = gpt->output_compare[2];
		break;
	case GPT_CNT:
		*dst = imx_gpt_cnt(gpt);
		break;
	default:
		vmm_lwarning("i.MX GPT read at unknown register 0x%08x\n",
			     offset);
	}
	vmm_read_unlock(&gpt->lock);

	return VMM_OK;
}

static void _imx_gpt_restart_timer(struct gpt_t *gpt,
				   int timer_idx,
				   u32 timestamp)
{
	u32 delta = 0;
	u64 delta64 = 0UL;

	if (likely((gpt->output_compare[timer_idx] > timestamp))) {
		/* Usual case */
		delta = gpt->output_compare[timer_idx] - timestamp;
	} else {
		delta = ~timestamp + 1 + gpt->output_compare[timer_idx];
	}

	delta64 = delta;
	delta64 *= (gpt->prescaler + 1) * gpt->freq;
	vmm_read_unlock(&gpt->lock);

	vmm_timer_event_stop(&gpt->event[timer_idx]);
	vmm_timer_event_start(&gpt->event[timer_idx], delta64);
}

static void _imx_gpt_stop_timer(struct gpt_t *gpt,
				int timer_idx)
{
	vmm_timer_event_stop(&gpt->event[timer_idx]);
}

static void _imx_gpt_enable(struct gpt_t *gpt,
			    int enable)
{
	int i = 0;

	enable = !!enable;
	if (enable == (gpt->control & 1)) {
		return;
	}

	/* Enabling timer */
	if (enable) {
		u32 cnt = 0;
		/*
		 * If the timer has never run before, set the first restart.
		 * If it is in ENMOD, the timer must be reset when enabled.
		 * It is frozen otherwise.
		 */
		if (gpt->control & GPT_CR_ENMOD) {
			cnt = 0;
		} else {
			cnt = gpt->freeze;
		}
		gpt->offset = vmm_timer_timestamp() - cnt * gpt->freq;

		for (i = 0; i < ARRAY_SIZE(gpt->event); ++i) {
			_imx_gpt_restart_timer(gpt, i, cnt);
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(gpt->event); ++i) {
			_imx_gpt_stop_timer(gpt, i);
		}
		gpt->freeze = imx_gpt_cnt(gpt);
	}
}

static void _imx_gpt_cnt_update(struct gpt_t *gpt,
				int cmpidx,
				u32 *cnt)
{
	/* Is the counter restarted (OCR1 match only)? */
	if ((0 == cmpidx) && !(gpt->control & GPT_CR_FRR)) {
		gpt->offset = vmm_timer_timestamp();
		*cnt = 0;
	} else {
		*cnt = imx_gpt_cnt(gpt);
	}
}

static int imx_gpt_emulator_write(struct vmm_emudev *edev,
				  physical_addr_t offset,
				  u32 regmask,
				  u32 regval)
{
	struct gpt_t *gpt = edev->priv;
	u32 idx = 0;
	u32 cnt = 0;
	u16 reg = offset & ~0x3;

	vmm_write_lock(&gpt->lock);
	switch (reg) {
	case GPT_CR:
		if (regval & GPT_CR_SW) {
			_imx_gpt_reset(gpt);
		}
		_imx_gpt_enable(gpt, regval & GPT_CR_EN);
		gpt->control = (gpt->control & regmask) | (regval & ~regmask);
		break;

	case GPT_PR:
		gpt->prescaler = (gpt->prescaler & regmask) |
			(regval & regmask & GPT_PR_MASK);
		break;

	case GPT_SR:
		gpt->status &= ~regval;
		gpt->status &= GPT_SR_MASK;
		if (!(gpt->status & GPT_SR_MASK)) {
			vmm_devemu_emulate_irq(gpt->guest, gpt->irq, 0);
		}
		break;

	case GPT_IR:
		if (regval & GPT_IR_ROVIE) {
			vmm_lwarning("Rollover interrupt not supported\n");
		}
		gpt->irq_ena = regval & GPT_IR_MASK;
		break;

	case GPT_OC1:
	case GPT_OC2:
	case GPT_OC3:
		idx = (reg - GPT_OC1) >> 2;
		_imx_gpt_cnt_update(gpt, idx, &cnt);
		gpt->output_compare[idx] = regval;

		if (gpt->control & GPT_CR_EN) {
			_imx_gpt_restart_timer(gpt, idx, cnt);
		}
		break;

	default:
		vmm_printf("i.MX GPT write at unknown register 0x%08x"
			   "\n", offset);
	}
	vmm_write_unlock(&gpt->lock);

	return VMM_OK;
}

static int imx_gpt_emulator_reset(struct vmm_emudev *edev)
{
	struct gpt_t *gpt = edev->priv;

	vmm_write_lock(&gpt->lock);
	_imx_gpt_reset(edev->priv);
	vmm_write_unlock(&gpt->lock);

	return VMM_OK;
}

static void imx_gpt_event(struct vmm_timer_event *event)
{
	int i = 0;
	struct gpt_t *gpt = event->priv;

	vmm_write_lock(&gpt->lock);
	for (i = 0; i < ARRAY_SIZE(gpt->event); ++i) {
		if (event == &gpt->event[i]) {
			u32 cnt;

			/* Is the GPT enabled? */
			if (!(gpt->control & 1)) {
				return;
			}

			_imx_gpt_cnt_update(gpt, i, &cnt);
			gpt->status |= 1 << i;

			/* Is the corresponding IRQ enabled */
			if (gpt->irq_ena & (1 << i)) {
				vmm_devemu_emulate_irq(gpt->guest, gpt->irq,
						       1);
			}
			_imx_gpt_restart_timer(gpt, i, cnt);
		}
	}
	vmm_write_unlock(&gpt->lock);
}

static int imx_gpt_emulator_probe(struct vmm_guest *guest,
				  struct vmm_emudev *edev,
				  const struct vmm_devtree_nodeid *eid)
{
	int i = 0;
	u32 freq = 0;
	struct gpt_t *gpt = NULL;

	gpt = vmm_zalloc(sizeof (struct gpt_t));
	if (NULL == gpt) {
		return VMM_ENOMEM;
	}
	gpt->guest = guest;

	if (VMM_OK != vmm_devtree_irq_get(edev->node, &gpt->irq, 0)) {
		vmm_free(gpt);
		return VMM_ENODEV;
	}

	if (VMM_OK == vmm_devtree_read_u32(edev->node, "clock-frequency",
					   &freq)) {
		gpt->freq = udiv32(1000000000UL, freq);
	} else {
		gpt->freq = 1000000000UL / 32000;
	}

	INIT_RW_LOCK(&gpt->lock);
	for (i = 0; i < ARRAY_SIZE(gpt->event); ++i) {
		INIT_TIMER_EVENT(&gpt->event[i], &imx_gpt_event, gpt);
	}

	edev->priv = gpt;

	return VMM_OK;
}

static int imx_gpt_emulator_remove(struct vmm_emudev *edev)
{
	return VMM_OK;
}

static struct vmm_devtree_nodeid imx_gpt_emuid_table[] = {
	{ .type = "timer",
	  .compatible = "fsl,imx6q-gpt",
	},
	{ /* end of list */ },
};

VMM_DECLARE_EMULATOR_SIMPLE(imx_gpt,
			    imx_gpt_emuid_table,
			    VMM_DEVEMU_LITTLE_ENDIAN,
			    imx_gpt_emulator_probe,
			    imx_gpt_emulator_remove,
			    imx_gpt_emulator_reset,
			    imx_gpt_emulator_read,
			    imx_gpt_emulator_write);

static int __init imx_gpt_emulator_init(void)
{
	int rc = vmm_devemu_register_emulator(&imx_gpt_emulator);
	return rc;
}

static void __exit imx_gpt_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&imx_gpt_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
