/**
 * Copyright (c) 2019 Anup Patel.
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
 * @file goldfish_rtc.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Google Goldfish RTC emulator.
 * @details This source file implements the Google Goldfish RTC device.
 *
 * For more details on Google Goldfish virtual platform RTC device refer:
 * https://android.googlesource.com/platform/external/qemu/+/master/docs/GOLDFISH-VIRTUAL-HARDWARE.TXT
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_timer.h>
#include <vmm_wallclock.h>
#include <vmm_devemu.h>
#include <libs/mathlib.h>

#define MODULE_DESC			"Goldfish RTC Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			goldfish_rtc_emulator_init
#define	MODULE_EXIT			goldfish_rtc_emulator_exit

#define RTC_TIME_LOW			0x00
#define RTC_TIME_HIGH			0x04
#define RTC_ALARM_LOW			0x08
#define RTC_ALARM_HIGH			0x0c
#define RTC_IRQ_ENABLED			0x10
#define RTC_CLEAR_ALARM			0x14
#define RTC_ALARM_STATUS		0x18
#define RTC_CLEAR_INTERRUPT		0x1c

struct goldfish_rtc_state {
	struct vmm_guest *guest;
	struct vmm_timer_event event;
	vmm_spinlock_t lock;
	u32 irq;
	u64 tick_offset;
	u64 tick_tstamp;
	u64 alarm_next;
	u32 alarm_running;
	u32 irq_pending;
	u32 irq_enabled;
};

/* Note: Must be called with s->lock held */
static void goldfish_rtc_update(struct goldfish_rtc_state *s)
{
	vmm_devemu_emulate_irq(s->guest, s->irq,
			       (s->irq_pending & s->irq_enabled) ? 1 : 0);
}

static void goldfish_rtc_timer_event(struct vmm_timer_event *event)
{
	struct goldfish_rtc_state *s = event->priv;

	vmm_spin_lock(&s->lock);

	s->alarm_running = 0;
	s->irq_pending = 1;
	goldfish_rtc_update(s);

	vmm_spin_unlock(&s->lock);
}

/* Note: Must be called with s->lock held */
static u64 goldfish_rtc_get_count(struct goldfish_rtc_state *s)
{
	return s->tick_offset + (vmm_timer_timestamp() - s->tick_tstamp);
}

/* Note: Must be called with s->lock held */
static void goldfish_rtc_clear_alarm(struct goldfish_rtc_state *s)
{
	vmm_timer_event_stop(&s->event);
	s->alarm_running = 0;
}

/* Note: Must be called with s->lock held */
static void goldfish_rtc_set_alarm(struct goldfish_rtc_state *s)
{
	u64 now = goldfish_rtc_get_count(s);
	u64 event = s->alarm_next;

	if (event <= now) {
		goldfish_rtc_clear_alarm(s);
		s->irq_pending = 1;
		goldfish_rtc_update(s);
	} else {
		s->alarm_running = 1;
		vmm_timer_event_start(&s->event, event - now);
	}
}

static int goldfish_rtc_emulator_read(struct vmm_emudev *edev,
					physical_addr_t offset,
					u32 *dst, u32 size)
{
	int rc = VMM_OK;
	struct goldfish_rtc_state *s = edev->priv;

	vmm_spin_lock(&s->lock);

	switch (offset) {
	case RTC_TIME_LOW:
		*dst = (u32)goldfish_rtc_get_count(s);
		break;
	case RTC_TIME_HIGH:
		*dst = (u32)(goldfish_rtc_get_count(s) >> 32);
		break;
	case RTC_ALARM_LOW:
		*dst = (u32)s->alarm_next;
		break;
	case RTC_ALARM_HIGH:
		*dst = (u32)(s->alarm_next >> 32);
		break;
	case RTC_IRQ_ENABLED:
		*dst = s->irq_enabled;
		break;
	case RTC_ALARM_STATUS:
		*dst = s->alarm_running;
		break;
	default:
		rc = VMM_EINVALID;
		break;
	};

	vmm_spin_unlock(&s->lock);

	return rc;
}

static int goldfish_rtc_emulator_write(struct vmm_emudev *edev,
					physical_addr_t offset,
					u32 src_mask, u32 src, u32 size)
{
	u32 val;
	int rc = VMM_OK;
	struct goldfish_rtc_state *s = edev->priv;

	vmm_spin_lock(&s->lock);

	switch (offset) {
	case RTC_TIME_LOW:
		val = (u32)s->tick_offset;
		val &= src_mask;
		val |= src;
		s->tick_offset &= (0xffffffffULL << 32);
		s->tick_offset |= ((u64)val);
		s->tick_tstamp = vmm_timer_timestamp();
		break;
	case RTC_TIME_HIGH:
		val = (u32)(s->tick_offset >> 32);
		val &= src_mask;
		val |= src;
		s->tick_offset &= 0xffffffffULL;
		s->tick_offset |= (((u64)val) << 32);
		s->tick_tstamp = vmm_timer_timestamp();
		break;
	case RTC_ALARM_LOW:
		val = (u32)s->alarm_next;
		val &= src_mask;
		val |= src;
		s->alarm_next &= (0xffffffffULL << 32);
		s->alarm_next |= ((u64)val);
		goldfish_rtc_set_alarm(s);
		break;
	case RTC_ALARM_HIGH:
		val = (u32)(s->alarm_next >> 32);
		val &= src_mask;
		val |= src;
		s->alarm_next &= 0xffffffffULL;
		s->alarm_next |= (((u64)val) << 32);
		break;
	case RTC_IRQ_ENABLED:
		s->irq_enabled = ((s->irq_enabled & src_mask) | src) & 0x1;
		goldfish_rtc_update(s);
		break;
	case RTC_CLEAR_ALARM:
		goldfish_rtc_clear_alarm(s);
		break;
	case RTC_CLEAR_INTERRUPT:
		s->irq_pending = 0;
		goldfish_rtc_update(s);
		break;
	default:
		rc = VMM_EINVALID;
		break;
	};

	vmm_spin_unlock(&s->lock);

	return rc;
}

static int goldfish_rtc_emulator_reset(struct vmm_emudev *edev)
{
	struct goldfish_rtc_state *s = edev->priv;
	struct vmm_timeval tv;
	struct vmm_timezone tz;
	int rc = VMM_OK;

	vmm_spin_lock(&s->lock);

	if ((rc = vmm_wallclock_get_timeofday(&tv, &tz))) {
		goto goldfish_rtc_emulator_reset_done;
	}

	s->tick_offset = (u64)(tv.tv_sec - (tz.tz_minuteswest * 60));
	s->tick_offset *= 1000000000ULL;
	s->tick_offset += (u64)tv.tv_nsec;
	s->tick_tstamp = vmm_timer_timestamp();

	s->alarm_next = s->tick_offset;
	s->alarm_running = 0;
	s->irq_pending = 0;
	s->irq_enabled = 0;

	vmm_timer_event_stop(&s->event);

	goldfish_rtc_update(s);

goldfish_rtc_emulator_reset_done:

	vmm_spin_unlock(&s->lock);

	return rc;
}

static int goldfish_rtc_emulator_probe(struct vmm_guest *guest,
					struct vmm_emudev *edev,
					const struct vmm_devtree_nodeid *eid)
{
	int rc = VMM_OK;
	struct goldfish_rtc_state *s;

	s = vmm_zalloc(sizeof(struct goldfish_rtc_state));
	if (!s) {
		rc = VMM_EFAIL;
		goto goldfish_rtc_emulator_probe_done;
	}

	s->guest = guest;
	INIT_SPIN_LOCK(&s->lock);

	rc = vmm_devtree_read_u32_atindex(edev->node,
					  VMM_DEVTREE_INTERRUPTS_ATTR_NAME,
					  &s->irq, 0);
	if (rc) {
		goto goldfish_rtc_emulator_probe_freestate_fail;
	}

	INIT_TIMER_EVENT(&s->event, &goldfish_rtc_timer_event, s);

	edev->priv = s;

	goto goldfish_rtc_emulator_probe_done;

goldfish_rtc_emulator_probe_freestate_fail:
	vmm_free(s);
goldfish_rtc_emulator_probe_done:
	return rc;
}

static int goldfish_rtc_emulator_remove(struct vmm_emudev *edev)
{
	struct goldfish_rtc_state *s = edev->priv;

	if (!s) {
		return VMM_EINVALID;
	}

	vmm_timer_event_stop(&s->event);
	vmm_free(s);
	edev->priv = NULL;

	return VMM_OK;
}

static struct vmm_devtree_nodeid goldfish_rtc_emuid_table[] = {
	{ .type = "rtc",
	  .compatible = "google,goldfish-rtc",
	  .data = NULL,
	},
	{ /* end of list */ },
};

VMM_DECLARE_EMULATOR_SIMPLE(goldfish_rtc_emulator,
			    "goldfish_rtc_emulator",
			    goldfish_rtc_emuid_table,
			    VMM_DEVEMU_LITTLE_ENDIAN,
			    goldfish_rtc_emulator_probe,
			    goldfish_rtc_emulator_remove,
			    goldfish_rtc_emulator_reset,
			    NULL,
			    goldfish_rtc_emulator_read,
			    goldfish_rtc_emulator_write);

static int __init goldfish_rtc_emulator_init(void)
{
	return vmm_devemu_register_emulator(&goldfish_rtc_emulator);
}

static void __exit goldfish_rtc_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&goldfish_rtc_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
