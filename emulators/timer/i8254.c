/*
 * Copyright (c) 2014 Himanshu Chauhan
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
 * @file i8254.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief i8254 Programmable Interval Timer
 * @details This source file implements the i8254 PIT emulator
 *
 * This work as largely been derived from Qemu hw/timer/i8254*
 * Copyright (c) 2003-2004 Fabrice Bellard
 */

#include <vmm_stdio.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_timer.h>
#include <vmm_devemu.h>
#include <vmm_spinlocks.h>
#include <vmm_vcpu_irq.h>

#include <libs/mathlib.h>
#include <emu/i8254.h>

//#define DEBUG_PIT

#define RW_STATE_LSB	1
#define RW_STATE_MSB	2
#define RW_STATE_WORD0	3
#define RW_STATE_WORD1	4

#define MODULE_DESC			"8253/8254 PIC Emulator"
#define MODULE_AUTHOR			"Himanshu Chauhan"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			i8254_emulator_init
#define	MODULE_EXIT			i8254_emulator_exit

static void pit_irq_timer_update(pit_channel_state_t *s, s64 current_time);

/* get pit output bit */
int pit_get_out(pit_channel_state_t *s, s64 current_time)
{
	u64 d;
	int out;

	d = muldiv64(current_time - s->count_load_time, PIT_FREQ,
		     1000000000ul);
	switch (s->mode) {
	default:
	case 0:
		out = (d >= s->count);
		break;
	case 1:
		out = (d < s->count);
		break;
	case 2:
		if ((d % s->count) == 0 && d != 0) {
			out = 1;
		} else {
			out = 0;
		}
		break;
	case 3:
		out = (d % s->count) < ((s->count + 1) >> 1);
		break;
	case 4:
	case 5:
		out = (d == s->count);
		break;
	}
	return out;
}

/* return -1 if no transition will occur.  */
s64 pit_get_next_transition_time(pit_channel_state_t *s, s64 current_time)
{
	u64 d, next_time, base;
	int period2;

	d = muldiv64(current_time - s->count_load_time, PIT_FREQ,
		     1000000000ul);

	switch (s->mode) {
	default:
	case 0:
	case 1:
		if (d < s->count) {
			next_time = s->count;
		} else {
			return -1;
		}
		break;
	case 2:
		base = (d / s->count) * s->count;
		if ((d - base) == 0 && d != 0) {
			next_time = base + s->count;
		} else {
			next_time = base + s->count + 1;
		}
		break;
	case 3:
		base = (d / s->count) * s->count;
		period2 = ((s->count + 1) >> 1);
		if ((d - base) < period2) {
			next_time = base + period2;
		} else {
			next_time = base + s->count;
		}
		break;
	case 4:
	case 5:
		if (d < s->count) {
			next_time = s->count;
		} else if (d == s->count) {
			next_time = s->count + 1;
		} else {
			return -1;
		}
		break;
	}

	/* convert to timer units */
	next_time = s->count_load_time + muldiv64(next_time, 1000000000ul,
						  PIT_FREQ);

	/* fix potential rounding problems */
	/* XXX: better solution: use a clock at PIT_FREQ Hz */
	if (next_time <= current_time) {
		next_time = current_time + 1;
	}
	return next_time;
}

void pit_reset_common(pit_common_state_t *pit)
{
	pit_channel_state_t *s;
	int i;

	for (i = 0; i < 3; i++) {
		s = &pit->channels[i];
		s->mode = 3;
		s->gate = (i != 2);
		s->count_load_time = vmm_timer_timestamp();
		s->count = 0x10000;
		if (i == 0 && !s->irq_disabled) {
			s->next_transition_time =
				pit_get_next_transition_time(s, s->count_load_time);
		}
	}
}

static int pit_get_count(pit_channel_state_t *s)
{
	u64 d;
	int counter;

	d = muldiv64(vmm_timer_timestamp() - s->count_load_time, PIT_FREQ,
		     1000000000UL);
	switch(s->mode) {
	case 0:
	case 1:
	case 4:
	case 5:
		counter = (s->count - d) & 0xffff;
		break;
	case 3:
		/* XXX: may be incorrect for odd counts */
		counter = s->count - ((2 * d) % s->count);
		break;
	default:
		counter = s->count - (d % s->count);
		break;
	}
	return counter;
}

#if 0
/* val must be 0 or 1 */
static void pit_set_channel_gate(pit_common_state_t *s, pit_channel_state_t *sc,
                                 int val)
{
	switch (sc->mode) {
	default:
	case 0:
	case 4:
		/* XXX: just disable/enable counting */
		break;
	case 1:
	case 5:
		if (sc->gate < val) {
			/* restart counting on rising edge */
			sc->count_load_time = vmm_timer_timestamp();
			pit_irq_timer_update(sc, sc->count_load_time);
		}
		break;
	case 2:
	case 3:
		if (sc->gate < val) {
			/* restart counting on rising edge */
			sc->count_load_time = vmm_timer_timestamp();
			pit_irq_timer_update(sc, sc->count_load_time);
		}
		/* XXX: disable/enable counting */
		break;
	}
	sc->gate = val;
}
#endif

static inline void pit_load_count(pit_channel_state_t *s, int val)
{
	if (val == 0)
		val = 0x10000;
	s->count_load_time = vmm_timer_timestamp();
	s->count = val;
	pit_irq_timer_update(s, s->count_load_time);
}

/* if already latched, do not latch again */
static void pit_latch_count(pit_channel_state_t *s)
{
	if (!s->count_latched) {
		s->latched_count = pit_get_count(s);
		s->count_latched = s->rw_mode;
	}
}

static int pit_ioport_write(pit_common_state_t *pit, u32 addr,
			    u32 src_mask, u32 val)
{
	int channel, access;
	pit_channel_state_t *s;

	addr &= 3;
	if (addr == 3) {
		channel = val >> 6;
		if (channel == 3) {
			/* read back command */
			for(channel = 0; channel < 3; channel++) {
				s = &pit->channels[channel];
				if (val & (2 << channel)) {
					if (!(val & 0x20)) {
						pit_latch_count(s);
					}
					if (!(val & 0x10) && !s->status_latched) {
						/* status latch */
						/* XXX: add BCD and null count */
						s->status =
							(pit_get_out(s,
								     vmm_timer_timestamp()) << 7) |
							(s->rw_mode << 4) |
							(s->mode << 1) |
							s->bcd;
						s->status_latched = 1;
					}
				}
			}
		} else {
			s = &pit->channels[channel];
			access = (val >> 4) & 3;
			if (access == 0) {
				pit_latch_count(s);
			} else {
				s->rw_mode = access;
				s->read_state = access;
				s->write_state = access;

				s->mode = (val >> 1) & 7;
				s->bcd = val & 1;
				/* XXX: update irq timer ? */
			}
		}
	} else {
		s = &pit->channels[addr];
		switch(s->write_state) {
		default:
		case RW_STATE_LSB:
			pit_load_count(s, val);
			break;
		case RW_STATE_MSB:
			pit_load_count(s, val << 8);
			break;
		case RW_STATE_WORD0:
			s->write_latch = val;
			s->write_state = RW_STATE_WORD1;
			break;
		case RW_STATE_WORD1:
			pit_load_count(s, s->write_latch | (val << 8));
			s->write_state = RW_STATE_WORD0;
			break;
		}
	}

	return VMM_OK;
}

static u64 pit_ioport_read(pit_common_state_t *pit, u32 addr,
			   u32 *dst)
{
	int ret, count;
	pit_channel_state_t *s;

	addr &= 3;
	s = &pit->channels[addr];
	if (s->status_latched) {
		s->status_latched = 0;
		ret = s->status;
	} else if (s->count_latched) {
		switch(s->count_latched) {
		default:
		case RW_STATE_LSB:
			ret = s->latched_count & 0xff;
			s->count_latched = 0;
			break;
		case RW_STATE_MSB:
			ret = s->latched_count >> 8;
			s->count_latched = 0;
			break;
		case RW_STATE_WORD0:
			ret = s->latched_count & 0xff;
			s->count_latched = RW_STATE_MSB;
			break;
		}
	} else {
		switch(s->read_state) {
		default:
		case RW_STATE_LSB:
			count = pit_get_count(s);
			ret = count & 0xff;
			break;
		case RW_STATE_MSB:
			count = pit_get_count(s);
			ret = (count >> 8) & 0xff;
			break;
		case RW_STATE_WORD0:
			count = pit_get_count(s);
			ret = count & 0xff;
			s->read_state = RW_STATE_WORD1;
			break;
		case RW_STATE_WORD1:
			count = pit_get_count(s);
			ret = (count >> 8) & 0xff;
			s->read_state = RW_STATE_WORD0;
			break;
		}
	}

	*dst = ret;

	return VMM_OK;
}

static void pit_irq_timer_update(pit_channel_state_t *s, s64 current_time)
{
	s64 expire_time;
	int irq_level;

	if (s->irq_disabled) {
		return;
	}
	expire_time = pit_get_next_transition_time(s, current_time);
	irq_level = pit_get_out(s, current_time);
	vmm_devemu_emulate_irq(s->guest, s->irq, irq_level);
#ifdef DEBUG_PIT
	vmm_printf("irq_level=%d next_delay=%f\n",
		   irq_level,
		   (double)(expire_time - current_time) / 1000000000ull);
#endif
	s->next_transition_time = expire_time;
	if (expire_time != -1) {
		vmm_timer_event_stop(&s->irq_timer);
		vmm_timer_event_start(&s->irq_timer,
				      (expire_time - s->count_load_time));
	} else {
		vmm_timer_event_stop(&s->irq_timer);
	}
}

static void pit_irq_timer(struct vmm_timer_event *event)
{
	pit_channel_state_t *s = event->priv;

	pit_irq_timer_update(s, s->next_transition_time);
}

static void pit_reset(pit_common_state_t *pit)
{
	pit_channel_state_t *s;

	pit_reset_common(pit);

	s = &pit->channels[0];
	if (!s->irq_disabled) {
		vmm_timer_event_stop(&s->irq_timer);
		vmm_timer_event_start(&s->irq_timer, s->next_transition_time);
	}
}

#if 0
/* When HPET is operating in legacy mode, suppress the ignored timer IRQ,
 * reenable it when legacy mode is left again. */
static void pit_irq_control(pit_common_state_t *pit, int n, int enable)
{
	pit_common_state_t *pit = opaque;
	pit_channel_state_t *s = &pit->channels[0];

	if (enable) {
		s->irq_disabled = 0;
		pit_irq_timer_update(s, vmm_timer_timestamp());
	} else {
		s->irq_disabled = 1;
		vmm_timer_event_stop(&s->irq_timer);
	}
}
#endif

static int i8254_emulator_reset(struct vmm_emudev *edev)
{
	pit_common_state_t *s = edev->priv;

	pit_reset(s);

	return VMM_OK;
}

static int i8254_emulator_read8(struct vmm_emudev *edev,
				physical_addr_t offset, 
				u8 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = pit_ioport_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFF;
	}

	return rc;
}

static int i8254_emulator_read16(struct vmm_emudev *edev,
				 physical_addr_t offset, 
				 u16 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = pit_ioport_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFFFF;
	}

	return rc;
}

static int i8254_emulator_read32(struct vmm_emudev *edev,
				 physical_addr_t offset, 
				 u32 *dst)
{
	return pit_ioport_read(edev->priv, offset, dst);
}

static int i8254_emulator_write8(struct vmm_emudev *edev,
				 physical_addr_t offset, 
				 u8 src)
{
	return pit_ioport_write(edev->priv, offset, 0xFFFFFF00, src);
}

static int i8254_emulator_write16(struct vmm_emudev *edev,
				  physical_addr_t offset, 
				  u16 src)
{
	return pit_ioport_write(edev->priv, offset, 0xFFFF0000, src);
}

static int i8254_emulator_write32(struct vmm_emudev *edev,
				  physical_addr_t offset, 
				  u32 src)
{
	return pit_ioport_write(edev->priv, offset, 0x00000000, src);
}

static int i8254_emulator_remove(struct vmm_emudev *edev)
{
	struct pit_common_state_t *s = edev->priv;

	vmm_free(s);

	return VMM_OK;
}

static int i8254_emulator_probe(struct vmm_guest *guest,
				struct vmm_emudev *edev,
				const struct vmm_devtree_nodeid *eid)
{
	int rc = VMM_OK;
	pit_common_state_t *s;

	s = vmm_zalloc(sizeof(pit_common_state_t));
	if (!s) {
		rc = VMM_ENOMEM;
		goto i8254_emulator_probe_done;
	}

	rc = vmm_devtree_read_u32_atindex(edev->node,
					  VMM_DEVTREE_INTERRUPTS_ATTR_NAME,
					  &s->channels[0].irq, 0);
	if (rc) {
		goto i8254_emulator_probe_freestate_fail;
	}

	INIT_SPIN_LOCK(&s->channels[0].channel_lock);
	INIT_SPIN_LOCK(&s->channels[1].channel_lock);
	INIT_SPIN_LOCK(&s->channels[2].channel_lock);

	INIT_TIMER_EVENT(&s->channels[0].irq_timer,
			 pit_irq_timer, &s->channels[0]);
	INIT_TIMER_EVENT(&s->channels[1].irq_timer,
			 pit_irq_timer, &s->channels[1]);
	INIT_TIMER_EVENT(&s->channels[2].irq_timer,
			 pit_irq_timer, &s->channels[2]);

	s->channels[0].guest = guest;
	s->channels[1].guest = guest;
	s->channels[2].guest = guest;

	edev->priv = s;

	goto i8254_emulator_probe_done;

 i8254_emulator_probe_freestate_fail:
	vmm_free(s);

 i8254_emulator_probe_done:
	return rc;
}

static struct vmm_devtree_nodeid i8254_emulator_emuid_table[] = {
	{
		.type = "pit",
		.compatible = "i8253,i8254",
	},
	{ /* end of list */ },
};

static struct vmm_emulator i8254_emulator = {
	.name =        "i8254",
	.match_table = i8254_emulator_emuid_table,
	.endian =      VMM_DEVEMU_LITTLE_ENDIAN,
	.probe =       i8254_emulator_probe,
	.read8 =       i8254_emulator_read8,
	.write8 =      i8254_emulator_write8,
	.read16 =      i8254_emulator_read16,
	.write16 =     i8254_emulator_write16,
	.read32 =      i8254_emulator_read32,
	.write32 =     i8254_emulator_write32,
	.reset =       i8254_emulator_reset,
	.remove =      i8254_emulator_remove,
};

static int __init i8254_emulator_init(void)
{
	return vmm_devemu_register_emulator(&i8254_emulator);
}

static void __exit i8254_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&i8254_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
