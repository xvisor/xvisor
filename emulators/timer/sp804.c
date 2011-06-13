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
 * @brief source file for SP804 Dual-Mode Timer.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_modules.h>
#include <vmm_host_io.h>
#include <vmm_devtree.h>
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
#define TIMER_CTRL_DIV1			(0 << 2)
#define TIMER_CTRL_DIV16		(1 << 2)
#define TIMER_CTRL_DIV256		(2 << 2)
#define TIMER_CTRL_IE			(1 << 5)
#define TIMER_CTRL_PERIODIC		(1 << 6)
#define TIMER_CTRL_ENABLE		(1 << 7)

struct sp804_timer {
	vmm_guest_t *guest;
	vmm_spinlock_t lock;
	/* Configuration */
	u32 freq;
	u32 irq;
	u32 tick_usecs;
	/* Registers */
	u32 control;
	u32 value;
	u32 value_mod;
	u32 value_dec;
	u32 limit;
	u32 irq_level;
};

static void sp804_timer_setirq(struct sp804_timer *t)
{
	if ((t->control & TIMER_CTRL_IE) && t->irq_level) {
		vmm_devemu_emulate_irq(t->guest, t->irq, 1);
	} else {
		vmm_devemu_emulate_irq(t->guest, t->irq, 0);
	}
}

/* Check if timer active and schedule the next timer interrupt.  */
static void sp804_timer_tick(struct sp804_timer *t)
{
	if ((t->control & TIMER_CTRL_ENABLE) &&
	    !(t->irq_level)) {
		vmm_spin_lock(&t->lock);
		if (t->value % t->value_mod == 0) {
			if (t->value < t->value_dec) {
				t->value = 0;
			} else {
				t->value -= t->value_dec;
			}
		}
		if (!t->value) {
			t->value = t->limit;
			t->irq_level = 1;
			if (t->control & TIMER_CTRL_ONESHOT) {
				t->control &= ~TIMER_CTRL_ENABLE;
			}
			sp804_timer_setirq(t);
		}
		vmm_spin_unlock(&t->lock);
	}
}

static int sp804_timer_read(struct sp804_timer *t, u32 offset, u32 *dst)
{
	int rc = VMM_OK;

	vmm_spin_lock(&t->lock);

	switch (offset >> 2) {
	case 0: /* TimerLoad */
	case 6: /* TimerBGLoad */
		*dst = t->limit;
		break;
	case 1: /* TimerValue */
		*dst = t->value;
		break;
	case 2: /* TimerControl */
		*dst = t->control;
		break;
	case 4: /* TimerRIS */
		*dst = t->irq_level;
		break;
	case 5: /* TimerMIS */
 		if ((t->control & TIMER_CTRL_IE) == 0)
			*dst = 0;
		*dst = t->irq_level;
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
	u32 freq;

	vmm_spin_lock(&t->lock);

	switch (offset >> 2) {
	case 0: /* TimerLoad */
		t->limit = (t->limit & src_mask) | (src & ~src_mask);
		if ((t->control & 
		    (TIMER_CTRL_PERIODIC | TIMER_CTRL_ONESHOT)) == 0) {
			/* Free running */
			if (t->control & TIMER_CTRL_32BIT) {
				t->limit = 0xFFFFFFFF;
			} else {
				t->limit = 0xFFFF;
			}
		}
		t->value = t->limit;
		break;
	case 1: /* TimerValue */
		/* ??? Guest seems to want to write to readonly register.
		 * Ignore it. 
		 */
		break;
	case 2: /* TimerControl */
		t->control = (t->control & src_mask) | (src & ~src_mask);
		freq = t->freq;
		/* ??? Need to recalculate expiry time 
		 * after changing divisor. */
		switch ((t->control >> 2) & 3) {
		case 1: 
			freq >>= 4; 
			break;
		case 2: 
			freq >>= 8; 
			break;
		};
		if (freq < 1000000) {
			t->value_dec = 1;
			t->value_mod = 1000000 / (freq * t->tick_usecs);
		} else {
			t->value_dec = (freq * t->tick_usecs) / 1000000;
			t->value_mod = 1;
		}
		if ((t->control &
		    (TIMER_CTRL_PERIODIC | TIMER_CTRL_ONESHOT)) == 0) {
			/* Free running */
			if (t->control & TIMER_CTRL_32BIT) {
				t->limit = 0xFFFFFFFF;
			} else {
				t->limit = 0xFFFF;
			}
		}
		if (t->control & TIMER_CTRL_ENABLE) {
			t->value = t->limit;
		}
		break;
	case 3: /* TimerIntClr */
 		t->irq_level = 0;
		sp804_timer_setirq(t);
		break;
	case 6: /* TimerBGLoad */
		t->limit = (t->limit & src_mask) | (src & ~src_mask);
		if ((t->control & 
		    (TIMER_CTRL_PERIODIC | TIMER_CTRL_ONESHOT)) == 0) {
			/* Free running */
			if (t->control & TIMER_CTRL_32BIT) {
				t->limit = 0xFFFFFFFF;
			} else {
				t->limit = 0xFFFF;
			}
		}
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
	u32 freq;
	vmm_spin_lock(&t->lock);

	t->value = 0xFFFFFFFF;
	t->control = TIMER_CTRL_IE;
	t->irq_level = 0;
	sp804_timer_setirq(t);
	freq = t->freq;
	switch ((t->control >> 2) & 3) {
	case 1: 
		freq >>= 4; 
		break;
	case 2: 
		freq >>= 8; 
		break;
	};
	if (freq < 1000000) {
		t->value_dec = 1;
		t->value_mod = 1000000 / (freq * t->tick_usecs);
	} else {
		t->value_dec = (freq * t->tick_usecs) / 1000000;
		t->value_mod = 1;
	}

	vmm_spin_unlock(&t->lock);

	return VMM_OK;
}

static int sp804_timer_init(struct sp804_timer *t, vmm_guest_t *guest,
			    u32 freq, u32 irq, u32 tick_usecs)
{
	INIT_SPIN_LOCK(&t->lock);
	t->guest = guest;
	t->freq = freq;
	t->irq = irq;
	t->tick_usecs = tick_usecs;

	return VMM_OK;
}

struct sp804_state {
	vmm_emuclk_t *clk;
	struct sp804_timer t[2];
};

static void sp804_emulator_tick(vmm_emuclk_t *eclk)
{
	struct sp804_state * s = eclk->priv;
	
	sp804_timer_tick(&s->t[0]);
	sp804_timer_tick(&s->t[1]);
}

static int sp804_emulator_read(vmm_emudev_t *edev,
			       physical_addr_t offset, 
			       void *dst, u32 dst_len)
{
	int rc = VMM_OK;
	u32 regval = 0x0;
	struct sp804_state * s = edev->priv;

	if (offset < 0x20) {
		rc = sp804_timer_read(&s->t[0], offset, &regval);
	} else {
		rc = sp804_timer_read(&s->t[1], offset - 0x20, &regval);
	}

	if (!rc) {
		switch (dst_len) {
		case 1:
			vmm_out_8(dst, regval);
			break;
		case 2:
			vmm_out_le16(dst, regval);
			break;
		case 4:
			vmm_out_le32(dst, regval);
			break;
		default:
			rc = VMM_EFAIL;
			break;
		};
	}

	return rc;
}

static int sp804_emulator_write(vmm_emudev_t *edev,
				physical_addr_t offset, 
				void *src, u32 src_len)
{
	int rc = VMM_OK;
	u32 regmask = 0x0, regval = 0x0;
	struct sp804_state * s = edev->priv;

	switch (src_len) {
	case 1:
		regmask = 0xFFFFFF00;
		regval = vmm_in_8(src);
		break;
	case 2:
		regmask = 0xFFFF0000;
		regval = vmm_in_le16(src);
		break;
	case 4:
		regmask = 0x00000000;
		regval = vmm_in_le32(src);
		break;
	default:
		rc = VMM_EFAIL;
		break;
	};

	if (!rc) {
		if (offset < 0x20) {
			rc = sp804_timer_write(&s->t[0], 
					       offset, regmask, regval);
		} else {
			rc = sp804_timer_write(&s->t[1], 
					       offset - 0x20, regmask, regval);
		}
	}

	return rc;
}

static int sp804_emulator_reset(vmm_emudev_t *edev)
{
	int rc;
	struct sp804_state * s = edev->priv;

	rc = sp804_timer_reset(&s->t[0]);
	if (rc) {
		return rc;
	}
	rc = sp804_timer_reset(&s->t[1]);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

static int sp804_emulator_probe(vmm_guest_t *guest,
				vmm_emudev_t *edev,
				const vmm_emuid_t *eid)
{
	int rc = VMM_OK;
	u32 irq, tick_usecs;
	const char *attr;
	struct sp804_state * s;

	s = vmm_malloc(sizeof(struct sp804_state));
	if (!s) {
		rc = VMM_EFAIL;
		goto sp804_emulator_probe_done;
	}
	vmm_memset(s, 0x0, sizeof(struct sp804_state));

	s->clk = vmm_malloc(sizeof(vmm_emuclk_t));
	if (!s->clk) {
		rc = VMM_EFAIL;
		goto sp804_emulator_probe_freestate_fail;
	}
	vmm_strcpy(s->clk->name, "sp804-clk");
	s->clk->tick = sp804_emulator_tick;
	s->clk->priv = s;
	if ((rc = vmm_devemu_register_clk(guest, s->clk))) {
		rc = VMM_EFAIL;
		goto sp804_emulator_probe_freeclk_fail;
	}

	attr = vmm_devtree_attrval(edev->node, "irq");
	if (attr) {
		irq = *((u32 *)attr);
	} else {
		rc = VMM_EFAIL;
		goto sp804_emulator_probe_freeclk_fail;
	}

	/* ??? The timers are actually configurable between 32kHz and 1MHz, 
	 * but we don't implement that.  */
	tick_usecs = vmm_devemu_clk_microsecs();
	if ((rc = sp804_timer_init(&s->t[0], guest, 
				   1000000, irq, tick_usecs))) {
		goto sp804_emulator_probe_freeclk_fail;
	}
	if ((rc = sp804_timer_init(&s->t[1], guest, 
				   1000000, irq, tick_usecs))) {
		goto sp804_emulator_probe_freeclk_fail;
	}

	edev->priv = s;

	goto sp804_emulator_probe_done;

sp804_emulator_probe_freeclk_fail:
	vmm_free(s->clk);
sp804_emulator_probe_freestate_fail:
	vmm_free(s);
sp804_emulator_probe_done:
	return rc;
}

static int sp804_emulator_remove(vmm_emudev_t *edev)
{
	struct sp804_state * s = edev->priv;

	vmm_free(s);

	return VMM_OK;
}

static vmm_emuid_t sp804_emuid_table[] = {
	{ .type = "timer", 
	  .compatible = "primecell,sp804", 
	},
	{ /* end of list */ },
};

static vmm_emulator_t sp804_emulator = {
	.name = "sp804",
	.match_table = sp804_emuid_table,
	.probe = sp804_emulator_probe,
	.read = sp804_emulator_read,
	.write = sp804_emulator_write,
	.reset = sp804_emulator_reset,
	.remove = sp804_emulator_remove,
};

static int sp804_emulator_init(void)
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
