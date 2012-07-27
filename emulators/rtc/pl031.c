/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file pl031.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief PrimeCell PL031 RTC emulator.
 * @details This source file implements the PrimeCell PL031 RTC emulator.
 *
 * The source has been largely adapted from QEMU 0.15.xx hw/pl031.c
 * 
 * ARM AMBA PrimeCell PL031 RTC
 *
 * Copyright (c) 2007 CodeSourcery
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_timer.h>
#include <vmm_wallclock.h>
#include <vmm_host_io.h>
#include <vmm_devemu.h>
#include <mathlib.h>

#define MODULE_VARID			pl031_emulator_module
#define MODULE_NAME			"PL031 RTC Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			pl031_emulator_init
#define	MODULE_EXIT			pl031_emulator_exit

#define RTC_DR      0x00    /* Data read register */
#define RTC_MR      0x04    /* Match register */
#define RTC_LR      0x08    /* Data load register */
#define RTC_CR      0x0c    /* Control register */
#define RTC_IMSC    0x10    /* Interrupt mask and set register */
#define RTC_RIS     0x14    /* Raw interrupt status register */
#define RTC_MIS     0x18    /* Masked interrupt status register */
#define RTC_ICR     0x1c    /* Interrupt clear register */

struct pl031_state {
	struct vmm_guest *guest;
	struct vmm_timer_event event;
	vmm_spinlock_t lock;
	u32 irq;
	u32 tick_offset;
	u64 tick_tstamp;
	u32 mr;
	u32 lr;
	u32 cr;
	u32 im;
	u32 is;
};

static const unsigned char pl031_id[] = {
	0x31, 0x10, 0x14, 0x00,	/* Device ID        */
	0x0d, 0xf0, 0x05, 0xb1 	/* Cell ID      */
};

static void pl031_update(struct pl031_state *s)
{
	vmm_devemu_emulate_irq(s->guest, s->irq, s->is & s->im);
}

static void pl031_timer_event(struct vmm_timer_event * event)
{
	struct pl031_state *s = (struct pl031_state *)event->priv;

	s->im = 1;
	pl031_update(s);
}

static u32 pl031_get_count(struct pl031_state *s)
{
	/* This assumes qemu_get_clock_ns returns the time since 
	 * the machine was created.
	 */
	return s->tick_offset + 
	(u32)udiv64(vmm_timer_timestamp() - s->tick_tstamp, 1000000000);
}

static void pl031_set_alarm(struct pl031_state *s)
{
	u32 ticks = pl031_get_count(s);

	/* If timer wraps around then subtraction also wraps in the same way,
	 * and gives correct results when alarm < now_ticks.  */
	ticks = s->mr - ticks;
	if (ticks == 0) {
		vmm_timer_event_stop(&s->event);
		s->im = 1;
		pl031_update(s);
	} else {
		vmm_timer_event_start(&s->event, 
				      ((u64)ticks) * ((u64)1000000000));
	}
}

static int pl031_reg_read(struct pl031_state * s, u32 offset, u32 *dst)
{
	int rc = VMM_OK;

	vmm_spin_lock(&s->lock);

	if (offset >= 0xfe0  &&  offset < 0x1000) {
		*dst = *((u32 *)&pl031_id[(offset - 0xfe0) >> 2]);
	} else {
		switch (offset) {
		case RTC_DR:
			*dst = pl031_get_count(s);
			break;
		case RTC_MR:
			*dst = s->mr;
			break;
		case RTC_IMSC:
			*dst = s->im;
			break;
		case RTC_RIS:
			*dst = s->is;
			break;
		case RTC_LR:
			*dst = s->lr;
			break;
		case RTC_CR:
			/* RTC is permanently enabled.  */
			*dst = 1;
			break;
		case RTC_MIS:
			*dst = s->is & s->im;
			break;
		case RTC_ICR:
		default:
			rc = VMM_EFAIL;
			break;
		};
	}

	vmm_spin_unlock(&s->lock);

	return rc;
}

static int pl031_reg_write(struct pl031_state * s, u32 offset, 
			   u32 src_mask, u32 src)
{
	int rc = VMM_OK;

	vmm_spin_lock(&s->lock);

	switch (offset) {
	case RTC_LR:
		s->tick_offset += (src & ~src_mask) - pl031_get_count(s);
		s->tick_tstamp = vmm_timer_timestamp();
		pl031_set_alarm(s);
		break;
	case RTC_MR:
		s->mr &= src_mask;
		s->mr |= (src & ~src_mask);
		pl031_set_alarm(s);
		break;
	case RTC_IMSC:
		s->im &= src_mask;
		s->im |= (src & ~src_mask) & 1;
		pl031_update(s);
		break;
	case RTC_ICR:
		/* The PL031 documentation (DDI0224B) states that the interrupt
		 * is cleared when bit 0 of the written value is set. However
		 * the arm926e documentation (DDI0287B) states that the
		 * interrupt is cleared when any value is written.
		 */
		s->is = 0;
		pl031_update(s);
		break;
	case RTC_CR:
		/* Written value is ignored.  */
		break;
	case RTC_DR:
	case RTC_MIS:
	case RTC_RIS:
	default:
		rc = VMM_EFAIL;
		break;
	};

	vmm_spin_unlock(&s->lock);

	return rc;
}

static int pl031_emulator_read(struct vmm_emudev *edev,
			       physical_addr_t offset, 
			       void *dst, u32 dst_len)
{
	int rc = VMM_OK;
	u32 regval = 0x0;
	struct pl031_state * s = edev->priv;

	rc = pl031_reg_read(s, offset & ~0x3, &regval);

	if (!rc) {
		regval = (regval >> ((offset & 0x3) * 8));
		switch (dst_len) {
		case 1:
			*(u8 *)dst = regval & 0xFF;
			break;
		case 2:
			*(u16 *)dst = arch_cpu_to_le16(regval & 0xFFFF);
			break;
		case 4:
			*(u32 *)dst = arch_cpu_to_le32(regval);
			break;
		default:
			rc = VMM_EFAIL;
			break;
		};
	}

	return rc;
}

static int pl031_emulator_write(struct vmm_emudev *edev,
				physical_addr_t offset, 
				void *src, u32 src_len)
{
	int i;
	u32 regmask = 0x0, regval = 0x0;
	struct pl031_state * s = edev->priv;

	switch (src_len) {
	case 1:
		regmask = 0xFFFFFF00;
		regval = *(u8 *)src;
		break;
	case 2:
		regmask = 0xFFFF0000;
		regval = vmm_le16_to_cpu(*(u16 *)src);
		break;
	case 4:
		regmask = 0x00000000;
		regval = vmm_le32_to_cpu(*(u32 *)src);
		break;
	default:
		return VMM_EFAIL;
		break;
	};

	for (i = 0; i < (offset & 0x3); i++) {
		regmask = (regmask << 8) | ((regmask >> 24) & 0xFF);
	}
	regval = (regval << ((offset & 0x3) * 8));

	return pl031_reg_write(s, offset & ~0x3, regmask, regval);
}

static int pl031_emulator_reset(struct vmm_emudev *edev)
{
	struct pl031_state * s = edev->priv;

	vmm_spin_lock(&s->lock);

	vmm_timer_event_stop(&s->event);
	s->im = 0;
	pl031_update(s);

	vmm_spin_unlock(&s->lock);

	return VMM_OK;
}

static int pl031_emulator_probe(struct vmm_guest *guest,
				struct vmm_emudev *edev,
				const struct vmm_emuid *eid)
{
	int rc = VMM_OK;
	const char *attr;
	struct vmm_timeval tv;
	struct vmm_timezone tz;
	struct pl031_state * s;

	s = vmm_malloc(sizeof(struct pl031_state));
	if (!s) {
		rc = VMM_EFAIL;
		goto pl031_emulator_probe_done;
	}
	vmm_memset(s, 0x0, sizeof(struct pl031_state));

	s->guest = guest;
	INIT_SPIN_LOCK(&s->lock);

	attr = vmm_devtree_attrval(edev->node, "irq");
	if (attr) {
		s->irq = *((u32 *)attr);
	} else {
		rc = VMM_EFAIL;
		goto pl031_emulator_probe_freestate_fail;
	}

	INIT_TIMER_EVENT(&s->event, &pl031_timer_event, s);

	if ((rc = vmm_wallclock_get_timeofday(&tv, &tz))) {
		goto pl031_emulator_probe_freestate_fail;
	}
        s->tick_offset = (u32)(tv.tv_sec - (tz.tz_minuteswest * 60));
	s->tick_tstamp = vmm_timer_timestamp();

	edev->priv = s;

	goto pl031_emulator_probe_done;

pl031_emulator_probe_freestate_fail:
	vmm_free(s);
pl031_emulator_probe_done:
	return rc;
}

static int pl031_emulator_remove(struct vmm_emudev *edev)
{
	struct pl031_state * s = edev->priv;

	if (s) {
		vmm_free(s);
		edev->priv = NULL;
	}

	return VMM_OK;
}

static struct vmm_emuid pl031_emuid_table[] = {
	{ .type = "rtc", 
	  .compatible = "primecell,pl031", 
	  .data = NULL,
	},
	{ /* end of list */ },
};

static struct vmm_emulator pl031_emulator = {
	.name = "pl031",
	.match_table = pl031_emuid_table,
	.probe = pl031_emulator_probe,
	.read = pl031_emulator_read,
	.write = pl031_emulator_write,
	.reset = pl031_emulator_reset,
	.remove = pl031_emulator_remove,
};

static int __init pl031_emulator_init(void)
{
	return vmm_devemu_register_emulator(&pl031_emulator);
}

static void pl031_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&pl031_emulator);
}

VMM_DECLARE_MODULE(MODULE_VARID, 
			MODULE_NAME, 
			MODULE_AUTHOR, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
