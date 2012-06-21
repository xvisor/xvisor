/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @file arm_timer.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief ARM MP private & watchdog timer emulator.
 *
 * @details This source file implements Private peripheral timer and 
 * watchdog blocks for ARM 11MPCore and A9MP
 *
 * The source has been largely adapted from QEMU hw/arm_mptimer.c 
 * and Xvisor emulators/timer/sp804.c
 *
 * Copyright (c) 2006-2007 CodeSourcery.
 * Copyright (c) 2011 Linaro Limited
 * Written by Paul Brook, Peter Maydell
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_timer.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_host_io.h>
#include <vmm_devemu.h>
#include <vmm_manager.h>
#include <vmm_scheduler.h>
#include <vmm_stdio.h>
#include <mathlib.h>

#define MODULE_VARID		mptimer_emulator_module
#define MODULE_NAME		"MPCore Private Timer and Watchdog Emulator"
#define MODULE_AUTHOR		"Sukanto Ghosh"
#define MODULE_IPRIORITY	0
#define	MODULE_INIT		mptimer_emulator_init
#define	MODULE_EXIT		mptimer_emulator_exit

#define MAX_CPUS		4

#define NUM_TIMERS_PER_CPU	2

struct mptimer_state;

struct timer_block {
	struct mptimer_state *mptimer;
	u32 cpu;
	vmm_spinlock_t lock;
	struct vmm_timer_event *event;

	/* Configuration */
	u32 irq;
	bool is_wdt;

	u32 freq;

	/* Common Registers */
	u32 load;
	u32 count;
	u32 control;
	u32 status;

	/* Watchdog-only register */
	u32 wrst_status;
	u32 wdisable;

	u64 tstamp;
};

struct mptimer_state {
	struct vmm_guest *guest;
	u32 num_cpu;
	u32 ref_freq;

	/* Array of (2 * num_cpu) timers */
	struct timer_block *timers;
};

#define TIMER_CTRL_ENABLE	(1 << 0)
#define TIMER_CTRL_ARELOAD	(1 << 1)
#define TIMER_CTRL_IE		(1 << 2)
#define TIMER_CTRL_WDM		(1 << 3)
#define TIMER_CTRL_SCALER(c)	((c >> 8) & 0xff)
#define TIMER_CTRL_RESVD	(0xFFFF00F8)

/* TODO: Support Prescaling */
#define timer_block_get_freq(timer)	(timer->mptimer->ref_freq)

static u32 timer_block_counter_value(struct timer_block *timer)
{
	u32 ret = 0;

	if(timer->control & TIMER_CTRL_ENABLE) {
		/* How much nsecs since the timer was started */
		u64 cval = vmm_timer_timestamp() - timer->tstamp;

		/* convert the computed time to freqency ticks */
		if (timer->freq == 1000000) {
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
		} else if (timer->freq != 1000000000) {
			cval = udiv64(cval * timer->freq, (u64) 1000000000);
		}

		if (timer->control & TIMER_CTRL_ARELOAD) {
			/*
			 * We need to convert this number of ticks (on 
			 * 64 bits) to a number on 32 bits.
			 */
			cval = umod64(cval, (u64) timer->load);
			ret = timer->load - (u32)cval;
		} else {
			if (cval >= timer->count) {
				ret = 0;
			} else {
				ret = timer->count - (u32)cval;
			}
		}
	}

	return ret;
}

static inline void timer_block_update_irq(struct timer_block *timer)
{
	if((timer->control & TIMER_CTRL_ENABLE) && 
	   (timer->control & TIMER_CTRL_IE)) {
		vmm_devemu_emulate_percpu_irq(timer->mptimer->guest, timer->irq, 
					      timer->cpu, timer->status);
	} else {
		vmm_devemu_emulate_percpu_irq(timer->mptimer->guest, timer->irq, 
					      timer->cpu, 0);
	}
}

static void timer_block_reload(struct timer_block *timer)
{
	u64 nsecs;

	if (timer->count == 0) {
		return;
	}

	timer->tstamp = vmm_timer_timestamp();

	nsecs = timer->count; 
	if (timer->freq == 1000000) {
		nsecs *= 1000;
	} else {
		nsecs = udiv64((nsecs * 1000000000),
					(u64) timer->freq);
	}

	vmm_timer_event_stop(timer->event); 
	vmm_timer_event_start(timer->event, nsecs);
}

static void timer_block_event(struct vmm_timer_event *event)
{
	struct timer_block *timer = event->priv;

	timer->status = 1;

	if(timer->control & TIMER_CTRL_WDM) {
		timer->wrst_status = 1;
		/* TODO: Watchdog reset logic */
	}

	if(timer->control & TIMER_CTRL_ARELOAD) {
		timer->count = timer->load;
		timer_block_reload(timer);
	} else {
		timer->count = 0;
	}

	timer_block_update_irq(timer);
}

int mptimer_reg_read(struct mptimer_state *s, u32 offset, u32 *dst)
{
	struct timer_block *timer;
	struct vmm_vcpu *vcpu;
	u32 cpu = 0;

	vcpu = vmm_scheduler_current_vcpu();
	if (!vcpu || !vcpu->guest) {
		return VMM_EFAIL;
	}
	if (s->guest->id != vcpu->guest->id) {
		return VMM_EFAIL;
	}

	cpu = vcpu->subid; 

	if(cpu >= s->num_cpu) {
		return VMM_EFAIL;
	}

	if (offset >= 0x38) {
		return VMM_EFAIL;
	} else if(offset < 0x20) {
		/* Private Timer */
		timer = &(s->timers[(2 * cpu)]);
	} else {
		/* Watchdog timer */ 
		timer = &(s->timers[(2 * cpu) + 1]);
		offset -= 0x20;
	}

	vmm_spin_lock(&(timer->lock));
	switch (offset) {
		case 0x0: /* Load */
			*dst = timer->load;
			break;
		case 0x4: /* Counter.  */
			*dst = timer_block_counter_value(timer);
			break;
		case 0x8: /* Control.  */
			*dst = timer->control;
			break;
		case 0xC: /* Interrupt status.  */
			*dst = timer->status;
			break;
		case 0x10:
			/* Watchdog status */
			*dst = timer->wrst_status;
			break;
		case 0x14:
			/* Watchdog disable */
			/* Write-only register */
		default:
			return 0;
	}

	vmm_spin_unlock(&timer->lock);
	return VMM_OK;
}

int mptimer_reg_write(struct mptimer_state *s, u32 offset, u32 src_mask, 
		      u32 src)
{
	struct timer_block *timer;
	struct vmm_vcpu *vcpu;
	u32 cpu = 0, old;

	vcpu = vmm_scheduler_current_vcpu();
	if (!vcpu || !vcpu->guest) {
		return VMM_EFAIL;
	}
	if (s->guest->id != vcpu->guest->id) {
		return VMM_EFAIL;
	}

	cpu = vcpu->subid; 

	if(cpu >= s->num_cpu) {
		return VMM_EFAIL;
	}

	if (offset >= 0x38) {
		return VMM_EFAIL;
	} else if (offset < 0x20) {
		/* Private Timer */
		timer = &(s->timers[(cpu << 1)]);
	} else {
		/* Watchdog timer */ 
		timer = &(s->timers[(cpu << 1) + 1]);
		offset -= 0x20;
	}

	vmm_spin_lock(&timer->lock);

	switch (offset) {
		case 0x0: /* Load */
			timer->load = src;
			/* Fall through.  */
		case 0x4: /* Counter.  */
			if ((timer->control & TIMER_CTRL_ENABLE) && 
			    timer->count) {
				/* Cancel the previous timer.  */
				vmm_timer_event_stop(timer->event); 
			}
			timer->count = src;
			if (timer->control & TIMER_CTRL_ENABLE) {
				timer_block_reload(timer);
			}
			break;
		case 0x8: /* Control.  */
			old = timer->control;
			timer->control = (src & ~TIMER_CTRL_RESVD);
			if(old & TIMER_CTRL_WDM) {
				timer->control |= TIMER_CTRL_WDM;
			}
			timer->freq = timer_block_get_freq(timer);
			if (((old & TIMER_CTRL_ENABLE) == 0) && 
			    (src & TIMER_CTRL_ENABLE)) {
				if (timer->count == 0 && 
				    (timer->control & TIMER_CTRL_ARELOAD)) {
					timer->count = timer->load;
				}
				timer_block_reload(timer);
			}
			break;
		case 0xc: /* Interrupt status.  */
			timer->status &= ~(src & 1);
			timer_block_update_irq(timer);
			break;
		case 0x10:
			/* Watchdog Reset status */
			timer->wrst_status &= ~(src & 1);
			break;
		case 0x14:
			/* Watchdog Disable */
			if (src == 0x12345678) {
				timer->wdisable = 0x12345678;
			} else if ((src == 0x87654321) && 
				   (timer->wdisable == 0x12345678)) {
				timer->control &= ~TIMER_CTRL_WDM;
			} else {
				timer->wdisable = 0;
			}
			break;
	}
	vmm_spin_unlock(&timer->lock);
	return VMM_OK;
}

int mptimer_state_reset(struct mptimer_state *mpt)
{
	int i;

	for(i=0; i<(2 * mpt->num_cpu); i++)
	{
		struct timer_block *timer = &(mpt->timers[i]);

		vmm_spin_lock(&timer->lock);

		vmm_timer_event_stop(timer->event);
		timer->load = 0;
		timer->control = 0;
		timer->status = 0;
		timer->tstamp = 0;
		timer->wdisable = 0;
		timer->freq = timer_block_get_freq(timer);
		timer_block_update_irq(timer);

		vmm_spin_unlock(&timer->lock);
	}

	return VMM_OK;
}

int mptimer_state_free(struct mptimer_state *s)
{
	int rc = VMM_OK, i=0;
	if (s) {
		if (s->timers) {
			while(i<(NUM_TIMERS_PER_CPU * s->num_cpu)) {
				if(vmm_timer_event_destroy(s->timers[i].event) 
						!= VMM_OK) {
					rc = VMM_EFAIL;
				}
				i++;
			}
			vmm_free(s->timers);
		}

		vmm_free(s);
	}
	return rc;
}

struct mptimer_state *mptimer_state_alloc(struct vmm_guest *guest,
					  struct vmm_emudev * edev, 
					  u32 num_cpu,
					  u32 periphclk,
					  u32 irq[])
{
	struct mptimer_state *s = NULL;
	int i;
	char tname[64];

	s = vmm_malloc(sizeof(struct mptimer_state));
	if (!s) {
		goto mptimer_state_alloc_done;
	}
	vmm_memset(s, 0x0, sizeof(struct mptimer_state));

	s->guest = guest;
	s->num_cpu = num_cpu;
	s->ref_freq = periphclk; 

	s->timers = vmm_malloc(NUM_TIMERS_PER_CPU * s->num_cpu * sizeof(struct timer_block));
	if (!s->timers) {
		goto mptimer_timerblock_alloc_failed;
	}
	vmm_memset(s->timers, 0x0, NUM_TIMERS_PER_CPU * s->num_cpu * sizeof(struct timer_block));

	/* Init the timer blocks */
	for(i=0; i<(NUM_TIMERS_PER_CPU * num_cpu); i++) {
		s->timers[i].mptimer = s;
		s->timers[i].irq = irq[i & 0x1];
		s->timers[i].cpu = (i >> 1);
		s->timers[i].is_wdt = (i & 0x1);
		INIT_SPIN_LOCK(&(s->timers[i].lock));
		vmm_sprintf(tname, " %s%s%s(%d/%d)",
			    guest->node->name,
			    VMM_DEVTREE_PATH_SEPARATOR_STRING,
			    edev->node->name,
			    (i >> 1),
			    (i & 0x1));
		s->timers[i].event = vmm_timer_event_create(tname, &timer_block_event, 
							    &(s->timers[i]));
	}

	goto mptimer_state_alloc_done;

mptimer_timerblock_alloc_failed:
	vmm_free(s);
	s = NULL;

mptimer_state_alloc_done:
	return s;
}

