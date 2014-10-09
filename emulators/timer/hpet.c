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
 * @file hpet.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief High Precision Event Timer Emulator
 *
 * This work is derived from Qemu/hw/timer/hpet.c
 *
 *  Copyright (c) 2007 Alexander Graf
 *  Copyright (c) 2008 IBM Corporation
 *
 *  Authors: Beth Kon <bkon@us.ibm.com>
 *
 * This driver attempts to emulate an HPET device in software.
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

#include <emu/hpet.h>
#include <emu/rtc/mc146818rtc.h>
#include <emu/i8254.h>

//#define HPET_DEBUG
#ifdef HPET_DEBUG
#define DPRINTF vmm_printf
#else
#define DPRINTF(...)
#endif

#define HPET_MSI_SUPPORT        0

#define MODULE_DESC			"High Precision Event Timer Emulator"
#define MODULE_AUTHOR			"Himanshu Chauhan"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			hpet_emulator_init
#define	MODULE_EXIT			hpet_emulator_exit

struct hpet_state;

typedef struct hpet_timer {  /* timers */
	u8 tn;             /*timer number*/
	struct vmm_timer_event timer;
	struct hpet_state *state;
	/* Memory-mapped, software visible timer registers */
	u64 config;        /* configuration/cap */
	u64 cmp;           /* comparator */
	u64 fsb;           /* FSB route */
	/* Hidden register state */
	u64 period;        /* Last value written to comparator */
	u8 wrap_flag;      /* timer pop will indicate wrap for one-shot 32-bit
			    * mode. Next pop will be actual timer expiration.
			    */
} hpet_timer_t;

typedef struct hpet_state {
	struct vmm_guest *guest;

	u64 hpet_offset;
	u32 irqs[HPET_NUM_IRQ_ROUTES];
	u32 flags;
	u8 rtc_irq_level;
	u32 pit_enabled;
	u32 num_timers;
	u32 intcap;
	struct hpet_timer timer[HPET_MAX_TIMERS];

	/* Memory-mapped, software visible registers */
	u64 capability;        /* capabilities */
	u64 config;            /* configuration */
	u64 isr;               /* interrupt status reg */
	u64 hpet_counter;      /* main counter */
	u32  hpet_id;           /* instance id */
} hpet_state_t;

static u32 hpet_in_legacy_mode(struct hpet_state *s)
{
	return s->config & HPET_CFG_LEGACY;
}

static u32 timer_int_route(struct hpet_timer *timer)
{
	return (timer->config & HPET_TN_INT_ROUTE_MASK) >> HPET_TN_INT_ROUTE_SHIFT;
}

static u32 timer_fsb_route(struct hpet_timer *t)
{
	return t->config & HPET_TN_FSB_ENABLE;
}

static u32 hpet_enabled(struct hpet_state *s)
{
	return s->config & HPET_CFG_ENABLE;
}

static u32 timer_is_periodic(struct hpet_timer *t)
{
	return t->config & HPET_TN_PERIODIC;
}

static u32 timer_enabled(struct hpet_timer *t)
{
	return t->config & HPET_TN_ENABLE;
}

static u32 hpet_time_after(u64 a, u64 b)
{
	return ((s32)(b) - (s32)(a) < 0);
}

static u32 hpet_time_after64(u64 a, u64 b)
{
	return ((s64)(b) - (s64)(a) < 0);
}

static u64 ticks_to_ns(u64 value)
{
	return (muldiv64(value, HPET_CLK_PERIOD, FS_PER_NS));
}

static u64 ns_to_ticks(u64 value)
{
	return (muldiv64(value, FS_PER_NS, HPET_CLK_PERIOD));
}

static u64 hpet_fixup_reg(u64 new, u64 old, u64 mask)
{
	new &= mask;
	new |= old & ~mask;
	return new;
}

static int activating_bit(u64 old, u64 new, u64 mask)
{
	return (!(old & mask) && (new & mask));
}

static int deactivating_bit(u64 old, u64 new, u64 mask)
{
	return ((old & mask) && !(new & mask));
}

static u64 hpet_get_ticks(struct hpet_state *s)
{
	return ns_to_ticks(vmm_timer_timestamp() + s->hpet_offset);
}

/*
 * calculate diff between comparator value and current ticks
 */
static inline u64 hpet_calculate_diff(struct hpet_timer *t, u64 current)
{
	if (t->config & HPET_TN_32BIT) {
		u32 diff, cmp;

		cmp = (u32)t->cmp;
		diff = cmp - (u32)current;
		diff = (s32)diff > 0 ? diff : (u32)1;
		return (u64)diff;
	} else {
		u64 diff, cmp;

		cmp = t->cmp;
		diff = cmp - current;
		diff = (s64)diff > 0 ? diff : (u64)1;
		return diff;
	}
}

static void update_irq(struct hpet_timer *timer, int set)
{
	u64 mask;
	struct hpet_state *s;
	int route;

	if (timer->tn <= 1 && hpet_in_legacy_mode(timer->state)) {
		/* if LegacyReplacementRoute bit is set, HPET specification requires
		 * timer0 be routed to IRQ0 in NON-APIC or IRQ2 in the I/O APIC,
		 * timer1 be routed to IRQ8 in NON-APIC or IRQ8 in the I/O APIC.
		 */
		route = (timer->tn == 0) ? 0 : RTC_ISA_IRQ;
	} else {
		route = timer_int_route(timer);
	}
	s = timer->state;
	mask = 1 << timer->tn;
	if (!set || !timer_enabled(timer) || !hpet_enabled(timer->state)) {
		s->isr &= ~mask;
		if (!timer_fsb_route(timer)) {
#define ISA_NUM_IRQS 16
			/* fold the ICH PIRQ# pin's internal inversion logic into hpet */
			if (route >= ISA_NUM_IRQS) {
				vmm_devemu_emulate_irq(s->guest, s->irqs[route], 1);
			} else {
				vmm_devemu_emulate_irq(s->guest, s->irqs[route], 0);
			}
		}
	} else if (timer_fsb_route(timer)) {
#if 0
		stl_le_phys(&address_space_memory,
			    timer->fsb >> 32, timer->fsb & 0xffffffff);
#endif
		vmm_panic("Write to fsb route\n");
	} else if (timer->config & HPET_TN_TYPE_LEVEL) {
		s->isr |= mask;
		/* fold the ICH PIRQ# pin's internal inversion logic into hpet */
		if (route >= ISA_NUM_IRQS) {
			vmm_devemu_emulate_irq(s->guest, s->irqs[route], 0);
		} else {
			vmm_devemu_emulate_irq(s->guest, s->irqs[route], 1);
		}
	} else {
		s->isr &= ~mask;
		vmm_devemu_emulate_irq(s->guest, s->irqs[route], 1);
		vmm_devemu_emulate_irq(s->guest, s->irqs[route], 0);
	}
}

/*
 * timer expiration callback
 */
static void hpet_timer(struct vmm_timer_event *event)
{
	struct hpet_timer *t = event->priv;
	u64 diff;

	u64 period = t->period;
	u64 cur_tick = hpet_get_ticks(t->state);

	if (timer_is_periodic(t) && period != 0) {
		if (t->config & HPET_TN_32BIT) {
			while (hpet_time_after(cur_tick, t->cmp)) {
				t->cmp = (u32)(t->cmp + t->period);
			}
		} else {
			while (hpet_time_after64(cur_tick, t->cmp)) {
				t->cmp += period;
			}
		}
		diff = hpet_calculate_diff(t, cur_tick);
		vmm_timer_event_stop(&t->timer);
		vmm_timer_event_start(&t->timer,
				      vmm_timer_timestamp()
				      + (s64)ticks_to_ns(diff));
	} else if (t->config & HPET_TN_32BIT && !timer_is_periodic(t)) {
		if (t->wrap_flag) {
			diff = hpet_calculate_diff(t, cur_tick);
			vmm_timer_event_stop(&t->timer);
			vmm_timer_event_start(&t->timer, vmm_timer_timestamp() +
					      (s64)ticks_to_ns(diff));
			t->wrap_flag = 0;
		}
	}
	update_irq(t, 1);
}

static void hpet_set_timer(struct hpet_timer *t)
{
	u64 diff;
	u32 wrap_diff;  /* how many ticks until we wrap? */
	u64 cur_tick = hpet_get_ticks(t->state);

	/* whenever new timer is being set up, make sure wrap_flag is 0 */
	t->wrap_flag = 0;
	diff = hpet_calculate_diff(t, cur_tick);

	/* hpet spec says in one-shot 32-bit mode, generate an interrupt when
	 * counter wraps in addition to an interrupt with comparator match.
	 */
	if (t->config & HPET_TN_32BIT && !timer_is_periodic(t)) {
		wrap_diff = 0xffffffff - (u32)cur_tick;
		if (wrap_diff < (u32)diff) {
			diff = wrap_diff;
			t->wrap_flag = 1;
		}
	}
	vmm_timer_event_stop(&t->timer);
	vmm_timer_event_start(&t->timer,
			      vmm_timer_timestamp() + (s64)ticks_to_ns(diff));
}

static void hpet_del_timer(struct hpet_timer *t)
{
	vmm_timer_event_stop(&t->timer);
	update_irq(t, 0);
}

static int hpet_ram_read(struct hpet_state *s, physical_addr_t addr,
			 u64 *dst)
{
	u64 cur_tick, index, retval;

	DPRINTF("qemu: Enter hpet_ram_readl at 0x%lx\n", addr);
	index = addr;
	/*address range of all TN regs*/
	if (index >= 0x100 && index <= 0x3ff) {
		u8 timer_id = (addr - 0x100) / 0x20;
		struct hpet_timer *timer = &s->timer[timer_id];

		if (timer_id > s->num_timers) {
			DPRINTF("qemu: timer id out of range\n");
			return VMM_ERANGE;
		}

		switch ((addr - 0x100) % 0x20) {
		case HPET_TN_CFG:
			retval = timer->config;
			break;
		case HPET_TN_CFG + 4: // Interrupt capabilities
			retval = timer->config >> 32;
			break;
		case HPET_TN_CMP: // comparator register
			retval = timer->cmp;
			break;
		case HPET_TN_CMP + 4:
			retval = timer->cmp >> 32;
			break;
		case HPET_TN_ROUTE:
			retval = timer->fsb;
			break;
		case HPET_TN_ROUTE + 4:
			retval = timer->fsb >> 32;
			break;
		default:
			DPRINTF("qemu: invalid hpet_ram_readl\n");
			return VMM_EINVALID;
		}
	} else {
		switch (index) {
		case HPET_ID:
			retval= s->capability;
			break;
		case HPET_PERIOD:
			retval = s->capability >> 32;
			break;
		case HPET_CFG:
			retval = s->config;
			break;
		case HPET_CFG + 4:
			DPRINTF("qemu: invalid HPET_CFG + 4 hpet_ram_readl\n");
			return VMM_EINVALID;
		case HPET_COUNTER:
			if (hpet_enabled(s)) {
				cur_tick = hpet_get_ticks(s);
			} else {
				cur_tick = s->hpet_counter;
			}
			DPRINTF("qemu: reading counter  = 0x%llx", cur_tick);
			retval = cur_tick;
			break;
		case HPET_COUNTER + 4:
			if (hpet_enabled(s)) {
				cur_tick = hpet_get_ticks(s);
			} else {
				cur_tick = s->hpet_counter;
			}
			DPRINTF("qemu: reading counter + 4  = 0x%llx\n", cur_tick);
			retval = (cur_tick >> 32);
			break;
		case HPET_STATUS:
			retval =  s->isr;
			break;
		default:
			DPRINTF("qemu: invalid hpet_ram_readl\n");
			return VMM_EINVALID;
		}
	}

	if (dst)
		*dst = retval;

	return VMM_OK;
 }

static int hpet_ram_write(struct hpet_state *s, physical_addr_t addr,
			  u64 mask, u64 value)
{
	int i;
	u64 old_val, new_val, val, index;

	DPRINTF("qemu: Enter hpet_ram_writel at 0x%llx = 0x%llx\n", addr, value);
	index = addr;
	hpet_ram_read(s, addr, &old_val);
	new_val = value;

	/*address range of all TN regs*/
	if (index >= 0x100 && index <= 0x3ff) {
		u8 timer_id = (addr - 0x100) / 0x20;
		struct hpet_timer *timer = &s->timer[timer_id];

		DPRINTF("qemu: hpet_ram_writel timer_id = %lx\n", timer_id);
		if (timer_id > s->num_timers) {
			DPRINTF("qemu: timer id out of range\n");
			return VMM_ERANGE;
		}
		switch ((addr - 0x100) % 0x20) {
		case HPET_TN_CFG:
			DPRINTF("qemu: hpet_ram_writel HPET_TN_CFG\n");
			if (activating_bit(old_val, new_val, HPET_TN_FSB_ENABLE)) {
				update_irq(timer, 0);
			}
			val = hpet_fixup_reg(new_val, old_val, HPET_TN_CFG_WRITE_MASK);
			timer->config = (timer->config & 0xffffffff00000000ULL) | val;
			if (new_val & HPET_TN_32BIT) {
				timer->cmp = (u32)timer->cmp;
				timer->period = (u32)timer->period;
			}
			if (activating_bit(old_val, new_val, HPET_TN_ENABLE) &&
			    hpet_enabled(s)) {
				hpet_set_timer(timer);
			} else if (deactivating_bit(old_val, new_val, HPET_TN_ENABLE)) {
				hpet_del_timer(timer);
			}
			break;
		case HPET_TN_CFG + 4: // Interrupt capabilities
			DPRINTF("qemu: invalid HPET_TN_CFG+4 write\n");
			break;
		case HPET_TN_CMP: // comparator register
			DPRINTF("qemu: hpet_ram_writel HPET_TN_CMP\n");
			if (timer->config & HPET_TN_32BIT) {
				new_val = (u32)new_val;
			}
			if (!timer_is_periodic(timer)
			    || (timer->config & HPET_TN_SETVAL)) {
				timer->cmp = (timer->cmp & 0xffffffff00000000ULL) | new_val;
			}
			if (timer_is_periodic(timer)) {
				/*
				 * FIXME: Clamp period to reasonable min value?
				 * Clamp period to reasonable max value
				 */
				new_val &= (timer->config & HPET_TN_32BIT ? ~0u : ~0ull) >> 1;
				timer->period =
					(timer->period & 0xffffffff00000000ULL) | new_val;
			}
			timer->config &= ~HPET_TN_SETVAL;
			if (hpet_enabled(s)) {
				hpet_set_timer(timer);
			}
			break;
		case HPET_TN_CMP + 4: // comparator register high order
			DPRINTF("qemu: hpet_ram_writel HPET_TN_CMP + 4\n");
			if (!timer_is_periodic(timer)
			    || (timer->config & HPET_TN_SETVAL)) {
				timer->cmp = (timer->cmp & 0xffffffffULL) | new_val << 32;
			} else {
				/*
				 * FIXME: Clamp period to reasonable min value?
				 * Clamp period to reasonable max value
				 */
				new_val &= (timer->config & HPET_TN_32BIT ? ~0u : ~0ull) >> 1;
				timer->period =
					(timer->period & 0xffffffffULL) | new_val << 32;
			}
			timer->config &= ~HPET_TN_SETVAL;
			if (hpet_enabled(s)) {
				hpet_set_timer(timer);
			}
			break;
		case HPET_TN_ROUTE:
			timer->fsb = (timer->fsb & 0xffffffff00000000ULL) | new_val;
			break;
		case HPET_TN_ROUTE + 4:
			timer->fsb = (new_val << 32) | (timer->fsb & 0xffffffff);
			break;
		default:
			DPRINTF("qemu: invalid hpet_ram_writel\n");
			return VMM_EINVALID;
		}
		return VMM_OK;;
	} else {
		switch (index) {
		case HPET_ID:
			return VMM_OK;
		case HPET_CFG:
			val = hpet_fixup_reg(new_val, old_val, HPET_CFG_WRITE_MASK);
			s->config = (s->config & 0xffffffff00000000ULL) | val;
			if (activating_bit(old_val, new_val, HPET_CFG_ENABLE)) {
				/* Enable main counter and interrupt generation. */
				s->hpet_offset =
					ticks_to_ns(s->hpet_counter) - vmm_timer_timestamp();
				for (i = 0; i < s->num_timers; i++) {
					if ((&s->timer[i])->cmp != ~0ULL) {
						hpet_set_timer(&s->timer[i]);
					}
				}
			} else if (deactivating_bit(old_val, new_val, HPET_CFG_ENABLE)) {
				/* Halt main counter and disable interrupt generation. */
				s->hpet_counter = hpet_get_ticks(s);
				for (i = 0; i < s->num_timers; i++) {
					hpet_del_timer(&s->timer[i]);
				}
			}
			/* i8254 and RTC output pins are disabled
			 * when HPET is in legacy mode */
			if (activating_bit(old_val, new_val, HPET_CFG_LEGACY)) {
				vmm_devemu_emulate_irq(s->guest, s->pit_enabled, 1);
				vmm_devemu_emulate_irq(s->guest, s->irqs[0], 0);
				vmm_devemu_emulate_irq(s->guest, s->irqs[RTC_ISA_IRQ], 0);
			} else if (deactivating_bit(old_val, new_val, HPET_CFG_LEGACY)) {
				vmm_devemu_emulate_irq(s->guest, s->irqs[0], 0);
				vmm_devemu_emulate_irq(s->guest, s->pit_enabled, 1);
				vmm_devemu_emulate_irq(s->guest, s->irqs[RTC_ISA_IRQ], s->rtc_irq_level);
			}
			break;
		case HPET_CFG + 4:
			DPRINTF("qemu: invalid HPET_CFG+4 write\n");
			break;
		case HPET_STATUS:
			val = new_val & s->isr;
			for (i = 0; i < s->num_timers; i++) {
				if (val & (1 << i)) {
					update_irq(&s->timer[i], 0);
				}
			}
			break;
		case HPET_COUNTER:
			if (hpet_enabled(s)) {
				DPRINTF("qemu: Writing counter while HPET enabled!\n");
			}
			s->hpet_counter =
				(s->hpet_counter & 0xffffffff00000000ULL) | value;
			DPRINTF("qemu: HPET counter written. ctr = %#x -> %" PRIx64 "\n",
				value, s->hpet_counter);
			break;
		case HPET_COUNTER + 4:
			if (hpet_enabled(s)) {
				DPRINTF("qemu: Writing counter while HPET enabled!\n");
			}
			s->hpet_counter =
				(s->hpet_counter & 0xffffffffULL) | (((u64)value) << 32);
			DPRINTF("qemu: HPET counter + 4 written. ctr = %#x -> %" PRIx64 "\n",
				value, s->hpet_counter);
			break;
		default:
			DPRINTF("qemu: invalid hpet_ram_writel\n");
			return VMM_EINVALID;
		}
	}

	return VMM_OK;
}

static void hpet_reset(struct hpet_state *s)
{
	int i;

	for (i = 0; i < s->num_timers; i++) {
		struct hpet_timer *timer = &s->timer[i];

		hpet_del_timer(timer);
		timer->cmp = ~0ULL;
		timer->config = HPET_TN_PERIODIC_CAP | HPET_TN_SIZE_CAP;
		if (s->flags & (1 << HPET_MSI_SUPPORT)) {
			timer->config |= HPET_TN_FSB_CAP;
		}
		/* advertise availability of ioapic int */
		timer->config |=  (u64)s->intcap << 32;
		timer->period = 0ULL;
		timer->wrap_flag = 0;
	}

	vmm_devemu_emulate_irq(s->guest, s->pit_enabled, 1);
	s->hpet_counter = 0ULL;
	s->hpet_offset = 0ULL;
	s->config = 0ULL;

	/* to document that the RTC lowers its output on reset as well */
	s->rtc_irq_level = 0;
}

static int hpet_emulator_reset(struct vmm_emudev *edev)
{
	struct hpet_state *s = edev->priv;

	hpet_reset(s);

	return VMM_OK;
}

static int hpet_emulator_read8(struct vmm_emudev *edev,
			       physical_addr_t offset,
			       u8 *dst)
{
	int rc;
	u64 regval = 0x0;

	rc = hpet_ram_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFF;
	}

	return rc;
}

static int hpet_emulator_read16(struct vmm_emudev *edev,
				physical_addr_t offset,
				u16 *dst)
{
	int rc;
	u64 regval = 0x0;

	rc = hpet_ram_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFFFF;
	}

	return rc;
}

static int hpet_emulator_read32(struct vmm_emudev *edev,
				physical_addr_t offset,
				u32 *dst)
{
	int rc;
	u64 regval = 0x0;

	rc = hpet_ram_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFFFFFFFF;
	}

	return rc;
}

static int hpet_emulator_read64(struct vmm_emudev *edev,
				physical_addr_t offset,
				u64 *dst)
{
	return hpet_ram_read(edev->priv, offset, dst);
}

static int hpet_emulator_write8(struct vmm_emudev *edev,
				physical_addr_t offset,
				u8 src)
{
	return hpet_ram_write(edev->priv, offset, 0xFFFFFFFFFFFFFF00ULL, src);
}

static int hpet_emulator_write16(struct vmm_emudev *edev,
				 physical_addr_t offset,
				 u16 src)
{
	return hpet_ram_write(edev->priv, offset, 0xFFFFFFFFFFFF0000ULL, src);
}

static int hpet_emulator_write32(struct vmm_emudev *edev,
				 physical_addr_t offset,
				 u32 src)
{
	return hpet_ram_write(edev->priv, offset, 0xFFFFFFFF00000000ULL, src);
}

static int hpet_emulator_write64(struct vmm_emudev *edev,
				 physical_addr_t offset,
				 u64 src)
{
	return hpet_ram_write(edev->priv, offset, 0x0000000000000000ULL, src);
}

static int hpet_emulator_remove(struct vmm_emudev *edev)
{
	struct hpet_state *s = edev->priv;

	vmm_free(s);

	return VMM_OK;
}

static int hpet_emulator_probe(struct vmm_guest *guest,
			       struct vmm_emudev *edev,
			       const struct vmm_devtree_nodeid *eid)
{
	int rc = VMM_OK, i;
	struct hpet_state *s = NULL;
	struct hpet_timer *timer = NULL;

	s = vmm_zalloc(sizeof(struct hpet_state));
	if (!s) {
		rc = VMM_ENOMEM;
		goto hpet_emulator_probe_done;
	}

	s->guest = guest;

	rc = vmm_devtree_read_u32(edev->node, "id", &s->hpet_id);
	if (rc != VMM_OK) {
		vmm_printf("HPET ID not specified in guest device tree.\n");
		goto hpet_emulator_probe_freestate_fail;
	}

	rc = vmm_devtree_read_u32(edev->node, "num_timers", &s->num_timers);
	if (rc != VMM_OK) {
		vmm_printf("Number of timers not specified in guest device tree.\n");
		goto hpet_emulator_probe_freestate_fail;
	}

	if (s->num_timers < HPET_MIN_TIMERS) {
		s->num_timers = HPET_MIN_TIMERS;
	} else if (s->num_timers > HPET_MAX_TIMERS) {
		s->num_timers = HPET_MAX_TIMERS;
	}

	for (i = 0; i < HPET_MAX_TIMERS; i++) {
		timer = &s->timer[i];
		INIT_TIMER_EVENT(&timer->timer, hpet_timer, timer);
		timer->tn = i;
		timer->state = s;
	}

	edev->priv = s;

	goto hpet_emulator_probe_done;

 hpet_emulator_probe_freestate_fail:
	vmm_free(s);

 hpet_emulator_probe_done:
	return rc;
}

static struct vmm_devtree_nodeid hpet_emulator_emuid_table[] = {
	{
		.type = "hpet",
		.compatible = "hpet",
	},
	{ /* end of list */ },
};

static struct vmm_emulator hpet_emulator = {
	.name =        "hpet",
	.match_table = hpet_emulator_emuid_table,
	.endian =      VMM_DEVEMU_LITTLE_ENDIAN,
	.probe =       hpet_emulator_probe,
	.read8 =       hpet_emulator_read8,
	.write8 =      hpet_emulator_write8,
	.read16 =      hpet_emulator_read16,
	.write16 =     hpet_emulator_write16,
	.read32 =      hpet_emulator_read32,
	.write32 =     hpet_emulator_write32,
	.read64 =      hpet_emulator_read64,
	.write64 =     hpet_emulator_write64,
	.reset =       hpet_emulator_reset,
	.remove =      hpet_emulator_remove,
};

static int __init hpet_emulator_init(void)
{
	return vmm_devemu_register_emulator(&hpet_emulator);
}

static void __exit hpet_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&hpet_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
