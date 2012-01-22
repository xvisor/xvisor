/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file sp804.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief SP804 Dual-Mode Timer Emulator.
 * @details This source file implements the SP804 Dual-Mode Timer emulator.
 *
 * The source has been largely adapted from QEMU 0.14.xx hw/arm_timer.c 
 *
 * ARM PrimeCell Timer modules.
 *
 * Copyright (c) 2005-2006 CodeSourcery.
 * Written by Paul Brook
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_math.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_timer.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_host_io.h>
#include <vmm_devemu.h>

#define MODULE_VARID			sp804_emulator_module
#define MODULE_NAME			"SP804 Dual-Mode Timer Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			sp804_emulator_init
#define	MODULE_EXIT			sp804_emulator_exit

/* Common timer implementation.  */
#define TIMER_CTRL_ONESHOT		(1 << 0)
#define TIMER_CTRL_32BIT		(1 << 1)
#define TIMER_CTRL_DIV16		(1 << 2)
#define TIMER_CTRL_DIV256		(1 << 3)
#define TIMER_CTRL_IE			(1 << 5)
#define TIMER_CTRL_PERIODIC		(1 << 6)
#define TIMER_CTRL_ENABLE		(1 << 7)

#define TIMER_CTRL_DIV_MASK 	(TIMER_CTRL_DIV16 | TIMER_CTRL_DIV256)

#define TIMER_CTRL_NOT_FREE_RUNNING	(TIMER_CTRL_PERIODIC | TIMER_CTRL_ONESHOT)

struct sp804_state;

struct sp804_timer {
	struct sp804_state *state;
	struct vmm_guest *guest;
	struct vmm_timer_event *event;
	vmm_spinlock_t lock;
	/* Configuration */
	u32 ref_freq;
	u32 freq;
	u32 irq;
	/* Registers */
	u32 control;
	u32 value;
	u64 value_tstamp;
	u32 limit;
	u32 irq_level;
};

struct sp804_state {
	struct sp804_timer t[2];
};

static bool sp804_timer_interrupt_is_raised(struct sp804_timer *t)
{
	return (t->irq_level && (t->control & TIMER_CTRL_ENABLE) 
		&& (t->control & TIMER_CTRL_IE));
}

static void sp804_timer_setirq(struct sp804_timer *t)
{
	if (sp804_timer_interrupt_is_raised(t)) {
		/*
		 * The timer is enabled, the interrupt mode is enabled and
		 * and an interrupt is pending ... So we can raise the 
		 * interrupt level to the guest OS
		 */
		vmm_devemu_emulate_irq(t->guest, t->irq, 1);
	} else {
		/*
		 * in all other cases, we need to lower the interrupt level.
		 */
		vmm_devemu_emulate_irq(t->guest, t->irq, 0);
	}
}

static u32 sp804_get_freq(struct sp804_timer *t)
{
	/* An array of dividers for our freq */
	static char freq_mul[4] = { 0, 4, 8, 0 };

	return (t->ref_freq >> freq_mul[(t->control >> 2) & 3]);
}

static void sp804_timer_init_timer(struct sp804_timer *t)
{
	if (t->control & TIMER_CTRL_ENABLE) {
		u64 nsecs;
		/* Get a time stamp */
		u64 tstamp = vmm_timer_timestamp();

		if (!(t->control & TIMER_CTRL_NOT_FREE_RUNNING)) {
			/* Free running timer */
			t->value = 0xffffffff;
		} else {
			/* init the value with the limit value. */
			t->value = t->limit;
		}

		/* If only 16 bits, we keep the lower bytes */
		if (!(t->control & TIMER_CTRL_32BIT)) {
			t->value &= 0xffff;
		}

		/* If interrupt is not enabled then we are done */
		if (!(t->control & TIMER_CTRL_IE)) {
			if (t->value_tstamp == 0) {
				/* If value_tstamp was not set yet, we set it
				 * before leaving
				 */
				t->value_tstamp = tstamp;
			}
			return;
		}

		/* Now we need to compute our delay in nsecs. */
		nsecs = (u64) t->value;

		/* 
		 * convert t->value in ns based on freq
		 * We optimize the 1MHz case as this is the one that is 
		 * mostly used here (and this is easy).
		 */
		if (nsecs) {
			if (t->freq == 1000000) {
				nsecs *= 1000;
			} else {
				nsecs =
				    vmm_udiv64((nsecs * 1000000000),
					       (u64) t->freq);
			}

			/* compute the tstamp */
			if (t->value_tstamp
			    && (!(t->control & TIMER_CTRL_ONESHOT))) {
				/* This is a restart of a periodic or free
				 * running timer
				 * We need to adjust our duration and start 
				 * time to account for timer processing
				 * overhead and expired periods
				 */
				u64 adjust_duration = tstamp - t->value_tstamp;

				while (adjust_duration > nsecs) {
					t->value_tstamp += nsecs;
					adjust_duration -= nsecs;
				}

				nsecs -= adjust_duration;
			} else {
				/* This is a simple one shot timer or the first
				 * run of a periodic timer
				 */
				t->value_tstamp = tstamp;
			}
		} else {
			t->value_tstamp = tstamp;
		}

		/*
		 * We start our timer
		 */
		if (vmm_timer_event_start(t->event, nsecs) == VMM_EFAIL) {
			/* FIXME: What should we do??? */
		}
	} else {
		/*
		 * This timer is not enabled ...
		 * To be safe, we stop the timer
		 */
		if (vmm_timer_event_stop(t->event) == VMM_EFAIL) {
			/* FIXME: What should we do??? */
		}
		/*
		 * At this point the timer should be frozen but could restart
		 * at any time if the timer is enabled again through the ctrl 
		 * reg
		 */
	}
}

static void sp804_timer_clear_irq(struct sp804_timer *t)
{
	if (t->irq_level == 1) {
		t->irq_level = 0;
		sp804_timer_setirq(t);
		if (!(t->control & TIMER_CTRL_ONESHOT)) {
			/* this is either free running or periodic timer.
			 * We restart the timer.
			 */
			sp804_timer_init_timer(t);
		}
	}
}

static void sp804_timer_event(struct vmm_timer_event * event)
{
	struct sp804_timer *t = event->priv;

	/* A timer event expired, if the timer is still activated,
	 * and the level is low, we need to process it
	 */
	if (t->control & TIMER_CTRL_ENABLE) {
		vmm_spin_lock(&t->lock);

		if (t->irq_level == 0) {
			/* We raise the interrupt */
			t->irq_level = 1;
			/* Raise an interrupt to the guest if required */
			sp804_timer_setirq(t);
		}

		if (t->control & TIMER_CTRL_ONESHOT) {
			/* If One shot timer, we disable it */
			t->control &= ~TIMER_CTRL_ENABLE;
			t->value_tstamp = 0;
		}

		vmm_spin_unlock(&t->lock);
	} else {
		/* The timer was not activated
		 * So we need to lower the interrupt level (if raised)
		 */
		sp804_timer_clear_irq(t);
	}
}

static u32 sp804_timer_current_value(struct sp804_timer *t)
{
	u32 ret = 0;

	if (t->control & TIMER_CTRL_ENABLE) {
		/* How much nsecs since the timer was started */
		u64 cval = vmm_timer_timestamp() - t->value_tstamp;

		/* convert the computed time to freqency ticks */
		if (t->freq == 1000000) {
			/* Note: Timestamps are in nanosecs so we convert
			 * nanosecs timestamp difference to microsecs timestamp
			 * difference for 1MHz clock. To achive this we simply
			 * have to divide timestamp difference by 1000, but in
			 * integer arithmetic any integer divided by 1000
			 * can be approximated as follows.
			 * (a / 1000)
			 * = (a / 1024) * (1024 / 1000)
			 * = (a / 1024) + (a / 1024) * (24 / 1000)
			 * = (a >> 10) + (a >> 10) * (3 / 125)
			 * = (a >> 10) + (a >> 10) * (3 / 128) * (128 / 125)
			 * = (a >> 10) + (a >> 10) * (3 / 128) +
			 *		    (a >> 10) * (3 / 128) * (3 / 125)
			 * ~ (a >> 10) + (a >> 10) * (3 / 128) +
			 *		    (a >> 10) * (3 / 128) * (3 / 128)
			 * ~ (a >> 10) + (((a >> 10) * 3) >> 7) +
			 *			      (((a >> 10) * 9) >> 14)
			 */
			cval = cval >> 10;
			cval = cval + ((cval * 3) >> 7) + ((cval * 9) >> 14);
		} else if (t->freq != 1000000000) {
			cval = vmm_udiv64(cval * t->freq, (u64) 1000000000);
		}

		if (t->control & (TIMER_CTRL_PERIODIC | TIMER_CTRL_PERIODIC)) {
			if (cval >= t->value) {
				ret = 0;
			} else {
				ret = t->value - (u32)cval;
			}
		} else {
			/*
			 * We need to convert this number of ticks (on 64 bits)
			 * to a number on 32 bits.
			 */
			switch (t->value) {
			case 0xFFFFFFFF:
			case 0xFFFF:
				ret = t->value - ((u32)cval & t->value);
				break;
			default:
				cval = vmm_umod64(cval, (u64) t->value);
				ret = t->value - (u32)cval;
				break;
			}
		}
	}

	return ret;
}

static int sp804_timer_read(struct sp804_timer *t, u32 offset, u32 * dst)
{
	int rc = VMM_OK;

	vmm_spin_lock(&t->lock);

	switch (offset >> 2) {
	case 0:		/* TimerLoad */
	case 6:		/* TimerBGLoad */
		*dst = t->limit;
		break;
	case 1:		/* TimerValue */
		*dst = sp804_timer_current_value(t);
		break;
	case 2:		/* TimerControl */
		*dst = t->control;
		break;
	case 4:		/* TimerRIS */
		*dst = t->irq_level;
		break;
	case 5:		/* TimerMIS */
		*dst = t->irq_level & ((t->control & TIMER_CTRL_IE) >> 5);
		break;
	default:
		rc = VMM_EFAIL;
		break;
	};

	vmm_spin_unlock(&t->lock);

	return rc;
}

static int sp804_timer_write(struct sp804_timer *t, u32 offset,
			     u32 src_mask, u32 src)
{
	int rc = VMM_OK;
	int timer_divider_select;

	vmm_spin_lock(&t->lock);

	switch (offset >> 2) {
	case 0:		/* TimerLoad */
		/* This update the limit and the timer value immediately */
		t->limit = (t->limit & src_mask) | (src & ~src_mask);
		sp804_timer_init_timer(t);
		break;
	case 1:		/* TimerValue */
		/* ??? Guest seems to want to write to readonly register.
		 * Ignore it. 
		 */
		break;
	case 2:		/* TimerControl */
		timer_divider_select = t->control;
		t->control = (t->control & src_mask) | (src & ~src_mask);
		if ((timer_divider_select & TIMER_CTRL_DIV_MASK) !=
		    (t->control & TIMER_CTRL_DIV_MASK)) {
			t->freq = sp804_get_freq(t);
		}
		sp804_timer_init_timer(t);
		break;
	case 3:		/* TimerIntClr */
		/* Any write to this register clear the interrupt status */
		sp804_timer_clear_irq(t);
		break;
	case 6:		/* TimerBGLoad */
		/* This will update the limit value for next interrupt 
		 * setting
		 */
		t->limit = (t->limit & src_mask) | (src & ~src_mask);
		break;
	default:
		rc = VMM_EFAIL;
		break;
	};

	vmm_spin_unlock(&t->lock);

	return rc;
}

static int sp804_timer_reset(struct sp804_timer *t)
{
	vmm_spin_lock(&t->lock);

	vmm_timer_event_stop(t->event);
	t->limit = 0xFFFFFFFF;
	t->control = TIMER_CTRL_IE;
	t->irq_level = 0;
	t->freq = sp804_get_freq(t);
	t->value_tstamp = 0;
	sp804_timer_setirq(t);
	sp804_timer_init_timer(t);

	vmm_spin_unlock(&t->lock);

	return VMM_OK;
}

static int sp804_timer_init(struct sp804_timer *t,
			    const char *t_name,
			    struct vmm_guest * guest, u32 freq, u32 irq)
{
	t->event = vmm_timer_event_create(t_name, &sp804_timer_event, t);

	if (t->event == NULL) {
		return VMM_EFAIL;
	}

	t->guest = guest;
	t->ref_freq = freq;
	t->freq = sp804_get_freq(t);
	t->irq = irq;
	INIT_SPIN_LOCK(&t->lock);

	return VMM_OK;
}

static int sp804_emulator_read(struct vmm_emudev * edev,
			       physical_addr_t offset, void *dst, u32 dst_len)
{
	int rc = VMM_OK;
	u32 regval = 0x0;
	struct sp804_state *s = edev->priv;

	rc = sp804_timer_read(&s->t[(offset<0x20)?0:1], offset & 0x1C, &regval);

	if (!rc) {
		regval = (regval >> ((offset & 0x3) * 8));
		switch (dst_len) {
		case 1:
			*(u8 *)dst = regval & 0xFF;
			break;
		case 2:
			*(u16 *)dst = vmm_cpu_to_le16(regval & 0xFFFF);
			break;
		case 4:
			*(u32 *)dst = vmm_cpu_to_le32(regval);
			break;
		default:
			rc = VMM_EFAIL;
			break;
		};
	}

	return rc;
}

static int sp804_emulator_write(struct vmm_emudev * edev,
				physical_addr_t offset, void *src, u32 src_len)
{
	int i;
	u32 regmask = 0x0, regval = 0x0;
	struct sp804_state *s = edev->priv;

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

	return sp804_timer_write(&s->t[(offset<0x20)?0:1], offset & 0x1C, regmask, regval);
}

static int sp804_emulator_reset(struct vmm_emudev * edev)
{
	int rc;
	struct sp804_state *s = edev->priv;

	if (!(rc = sp804_timer_reset(&s->t[0])) &&
	    !(rc = sp804_timer_reset(&s->t[1])));
	return rc;
}

static int sp804_emulator_probe(struct vmm_guest * guest,
				struct vmm_emudev * edev, 
				const struct vmm_emuid * eid)
{
	int rc = VMM_OK;
	u32 irq;
	char tname[32];
	const char *attr;
	struct sp804_state *s;

	s = vmm_malloc(sizeof(struct sp804_state));
	if (!s) {
		rc = VMM_EFAIL;
		goto sp804_emulator_probe_done;
	}
	vmm_memset(s, 0x0, sizeof(struct sp804_state));

	attr = vmm_devtree_attrval(edev->node, "irq");
	if (attr) {
		irq = *((u32 *) attr);
	} else {
		rc = VMM_EFAIL;
		goto sp804_emulator_probe_freestate_fail;
	}

	/* ??? The timers are actually configurable between 32kHz and 1MHz, 
	 * but we don't implement that.  */
	vmm_strcpy(tname, guest->node->name);
	vmm_strcat(tname, VMM_DEVTREE_PATH_SEPARATOR_STRING);
	vmm_strcat(tname, edev->node->name);
	vmm_strcat(tname, "(0)");
	s->t[0].state = s;
	if ((rc = sp804_timer_init(&s->t[0], tname, guest, 1000000, irq))) {
		goto sp804_emulator_probe_freestate_fail;
	}
	vmm_strcpy(tname, guest->node->name);
	vmm_strcat(tname, VMM_DEVTREE_PATH_SEPARATOR_STRING);
	vmm_strcat(tname, edev->node->name);
	vmm_strcat(tname, "(1)");
	s->t[1].state = s;
	if ((rc = sp804_timer_init(&s->t[1], tname, guest, 1000000, irq))) {
		goto sp804_emulator_probe_freestate_fail;
	}

	edev->priv = s;

	goto sp804_emulator_probe_done;

 sp804_emulator_probe_freestate_fail:
	vmm_free(s);
 sp804_emulator_probe_done:
	return rc;
}

static int sp804_emulator_remove(struct vmm_emudev * edev)
{
	struct sp804_state *s = edev->priv;

	if (s) {
		vmm_free(s);
		edev->priv = NULL;
	}

	return VMM_OK;
}

static struct vmm_emuid sp804_emuid_table[] = {
	{.type = "timer",
	 .compatible = "primecell,sp804",
	 },
	{ /* end of list */ },
};

static struct vmm_emulator sp804_emulator = {
	.name = "sp804",
	.match_table = sp804_emuid_table,
	.probe = sp804_emulator_probe,
	.read = sp804_emulator_read,
	.write = sp804_emulator_write,
	.reset = sp804_emulator_reset,
	.remove = sp804_emulator_remove,
};

static int __init sp804_emulator_init(void)
{
	return vmm_devemu_register_emulator(&sp804_emulator);
}

static void sp804_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&sp804_emulator);
}

VMM_DECLARE_MODULE(MODULE_VARID,
		   MODULE_NAME,
		   MODULE_AUTHOR,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
