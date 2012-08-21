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
 * @file pl061.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief PrimeCell PL061 GPIO Controller Emulator.
 *
 * The source has been largely adapted from QEMU 0.14.xx hw/pl061.c 
 *
 * Arm PrimeCell PL061 General Purpose IO with additional
 * Luminary Micro Stellaris bits.
 *
 * Copyright (c) 2007 CodeSourcery.
 * Written by Paul Brook
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_host_io.h>
#include <vmm_devemu.h>
#include <stringlib.h>

#define MODULE_DESC			"PL061 GPIO Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			pl061_emulator_init
#define	MODULE_EXIT			pl061_emulator_exit

struct pl061_state {
	struct vmm_guest *guest;
	struct vmm_emupic *pic;
	vmm_spinlock_t lock;

	u8 id[12];
	u32 locked;
	u32 data;
	u32 old_data;
	u32 dir;
	u32 isense;
	u32 ibe;
	u32 iev;
	u32 im;
	u32 istate;
	u32 afsel;
	u32 dr2r;
	u32 dr4r;
	u32 dr8r;
	u32 odr;
	u32 pur;
	u32 pdr;
	u32 slr;
	u32 den;
	u32 cr;
	u32 float_high;
	u32 amsel;

	u32 irq;
	u32 in_invert[8];
	u32 in_irq[8];
	u32 out_irq[8];
};

/* Call update function with lock held */
static void pl061_update(struct pl061_state *s)
{
	int line;
	u8 changed, mask, out;

	/* Outputs float high.  */
	/* FIXME: This is board dependent.  */
	out = (s->data & s->dir) | ~s->dir;
	changed = s->old_data ^ out;
	if (!changed)
		return;

	s->old_data = out;
	for (line = 0; line < 8; line++) {
		mask = 1 << line;
		if (changed & mask) {
			vmm_devemu_emulate_irq(s->guest, 
					       s->out_irq[line], 
					       (out & mask) != 0);
		}
	}

	/* FIXME: Implement input interrupts.  */
}

static int pl061_emulator_read(struct vmm_emudev *edev,
			       physical_addr_t offset, 
			       void *dst, u32 dst_len)
{
	int rc = VMM_OK;
	u32 regval = 0x0;
	struct pl061_state * s = edev->priv;

	vmm_spin_lock(&s->lock);

	if (offset >= 0xfd0 && offset < 0x1000) {
		regval = *((u32 *)&s->id[(offset - 0xfd0) >> 2]);
	} else if (offset < 0x400) {
		regval = s->data & (offset >> 2);
	} else {
		switch (offset & ~0x3) {
		case 0x400: /* Direction */
			regval = s->dir;
			break;
		case 0x404: /* Interrupt sense */
			regval = s->isense;
			break;
		case 0x408: /* Interrupt both edges */
			regval = s->ibe;
			break;
		case 0x40c: /* Interrupt event */
			regval = s->iev;
			break;
		case 0x410: /* Interrupt mask */
			regval = s->im;
			break;
		case 0x414: /* Raw interrupt status */
			regval = s->istate;
			break;
		case 0x418: /* Masked interrupt status */
			regval = s->istate | s->im;
			break;
		case 0x420: /* Alternate function select */
			regval = s->afsel;
			break;
		case 0x500: /* 2mA drive */
			regval = s->dr2r;
			break;
		case 0x504: /* 4mA drive */
			regval = s->dr4r;
			break;
		case 0x508: /* 8mA drive */
			regval = s->dr8r;
			break;
		case 0x50c: /* Open drain */
			regval = s->odr;
			break;
		case 0x510: /* Pull-up */
			regval = s->pur;
			break;
		case 0x514: /* Pull-down */
			regval = s->pdr;
			break;
		case 0x518: /* Slew rate control */
			regval = s->slr;
			break;
		case 0x51c: /* Digital enable */
			regval = s->den;
			break;
		case 0x520: /* Lock */
			regval = s->locked;
			break;
		case 0x524: /* Commit */
			regval = s->cr;
			break;
		case 0x528: /* Analog mode select */
			regval = s->amsel;
			break;
		default:
			rc = VMM_EFAIL;
			break;
		}
	}

	vmm_spin_unlock(&s->lock);

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

static int pl061_emulator_write(struct vmm_emudev *edev,
				physical_addr_t offset, 
				void *src, u32 src_len)
{
	u8 mask;
	int rc = VMM_OK, i;
	u32 regmask = 0x0, regval = 0x0;
	struct pl061_state * s = edev->priv;

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

	vmm_spin_lock(&s->lock);

	if (offset < 0x400) {
		mask = (offset >> 2) & s->dir;
		s->data = (s->data & ~mask) | (regval & mask);
	} else {
		switch (offset & ~0x3) {
		case 0x400: /* Direction */
			s->dir &= regmask;
			s->dir |= (regval & 0xFF);
			break;
		case 0x404: /* Interrupt sense */
			s->isense &= regmask;
			s->isense |= (regval & 0xFF);
			break;
		case 0x408: /* Interrupt both edges */
			s->ibe &= regmask;
			s->ibe |= (regval & 0xFF);
			break;
		case 0x40c: /* Interrupt event */
			s->iev &= regmask;
			s->iev |= (regval & 0xFF);
			break;
		case 0x410: /* Interrupt mask */
			s->im &= regmask;
			s->im |= (regval & 0xFF);
			break;
		case 0x41c: /* Interrupt clear */
			s->istate &= ~regval;
			break;
		case 0x420: /* Alternate function select */
			mask = s->cr;
			s->afsel = (s->afsel & ~mask) | (regval & mask);
			break;
		case 0x500: /* 2mA drive */
			s->dr2r &= regmask;
			s->dr2r |= (regval & 0xFF);
			break;
		case 0x504: /* 4mA drive */
			s->dr4r &= regmask;
			s->dr4r |= (regval & 0xFF);
			break;
		case 0x508: /* 8mA drive */
			s->dr8r &= regmask;
			s->dr8r |= (regval & 0xFF);
			break;
		case 0x50c: /* Open drain */
			s->odr &= regmask;
			s->odr |= (regval & 0xFF);
			break;
		case 0x510: /* Pull-up */
			s->pur &= regmask;
			s->pur |= (regval & 0xFF);
			break;
		case 0x514: /* Pull-down */
			s->pdr &= regmask;
			s->pdr |= (regval & 0xFF);
			break;
		case 0x518: /* Slew rate control */
			s->slr &= regmask;
			s->slr |= (regval & 0xFF);
			break;
		case 0x51c: /* Digital enable */
			s->den &= regmask;
			s->den |= (regval & 0xFF);
			break;
		case 0x520: /* Lock */
			s->locked = (regval != 0xacce551);
			break;
		case 0x524: /* Commit */
			if (!s->locked) {
				s->cr &= regmask;
				s->cr |= (regval & 0xFF);
			}
			break;
		case 0x528:
			s->amsel &= regmask;
			s->amsel |= (regval & 0xFF);
			break;
		default:
			rc = VMM_EFAIL;
			break;
		}
	}
	if (rc == VMM_OK) {
		pl061_update(s);
	}

	vmm_spin_unlock(&s->lock);

	return rc;
}

static int pl061_emulator_reset(struct vmm_emudev *edev)
{
	struct pl061_state * s = edev->priv;

	vmm_spin_lock(&s->lock);

	s->locked = 1;
	s->cr = 0xff;

	vmm_spin_unlock(&s->lock);

	return VMM_OK;
}

/* Process IRQ asserted in device emulation framework */
static int pl061_irq_handle(struct vmm_emupic *epic, 
			    u32 irq, int cpu, int level)
{
	u8 mask;
	int i, line;
	struct pl061_state * s = epic->priv;

	line = -1;
	for (i = 0; i < 8; i++) {
		if (s->in_irq[i] == irq) {
			line = i;
			break;
		}
	}
	if (line == -1) {
		return VMM_EMUPIC_GPIO_UNHANDLED;
	}

	if (s->in_invert[line]) {
		level = (level) ? 0 : 1;
	}
	mask = 1 << line;

	vmm_spin_lock(&s->lock);

	if ((s->dir & mask) == 0) {
		s->data &= ~mask;
		if (level)
			s->data |= mask;
		pl061_update(s);
	}

	vmm_spin_unlock(&s->lock);

	return VMM_EMUPIC_GPIO_HANDLED;
}

static int pl061_emulator_probe(struct vmm_guest *guest,
				struct vmm_emudev *edev,
				const struct vmm_emuid *eid)
{
	int rc = VMM_OK;
	const char *attr;
	struct pl061_state * s;

	s = vmm_malloc(sizeof(struct pl061_state));
	if (!s) {
		rc = VMM_EFAIL;
		goto pl061_emulator_probe_done;
	}
	memset(s, 0x0, sizeof(struct pl061_state));

	s->pic = vmm_malloc(sizeof(struct vmm_emupic));
	if (!s->pic) {
		goto pl061_emulator_probe_freestate_failed;
	}
	memset(s->pic, 0x0, sizeof(struct vmm_emupic));

	strcpy(s->pic->name, edev->node->name);
	s->pic->type = VMM_EMUPIC_GPIO;
	s->pic->handle = &pl061_irq_handle;
	s->pic->priv = s;
	if ((rc = vmm_devemu_register_pic(guest, s->pic))) {
		goto pl061_emulator_probe_freepic_failed;
	}

	if (eid->data) {
		s->id[0] = ((u8 *)eid->data)[0];
		s->id[1] = ((u8 *)eid->data)[1];
		s->id[2] = ((u8 *)eid->data)[2];
		s->id[3] = ((u8 *)eid->data)[3];
		s->id[4] = ((u8 *)eid->data)[4];
		s->id[5] = ((u8 *)eid->data)[5];
		s->id[6] = ((u8 *)eid->data)[6];
		s->id[7] = ((u8 *)eid->data)[7];
		s->id[8] = ((u8 *)eid->data)[8];
		s->id[9] = ((u8 *)eid->data)[9];
		s->id[10] = ((u8 *)eid->data)[10];
		s->id[11] = ((u8 *)eid->data)[11];
	}

	attr = vmm_devtree_attrval(edev->node, "irq");
	if (attr) {
		s->irq = *((u32 *)attr);
	} else {
		rc = VMM_EFAIL;
		goto pl061_emulator_probe_unregpic_failed;
	}

	attr = vmm_devtree_attrval(edev->node, "gpio_in_invert");
	if (attr) {
		s->in_invert[0] = ((u32 *)attr)[0];
		s->in_invert[1] = ((u32 *)attr)[1];
		s->in_invert[2] = ((u32 *)attr)[2];
		s->in_invert[3] = ((u32 *)attr)[3];
		s->in_invert[4] = ((u32 *)attr)[4];
		s->in_invert[5] = ((u32 *)attr)[5];
		s->in_invert[6] = ((u32 *)attr)[6];
		s->in_invert[7] = ((u32 *)attr)[7];
	} else {
		rc = VMM_EFAIL;
		goto pl061_emulator_probe_unregpic_failed;
	}

	attr = vmm_devtree_attrval(edev->node, "gpio_in_irq");
	if (attr) {
		s->in_irq[0] = ((u32 *)attr)[0];
		s->in_irq[1] = ((u32 *)attr)[1];
		s->in_irq[2] = ((u32 *)attr)[2];
		s->in_irq[3] = ((u32 *)attr)[3];
		s->in_irq[4] = ((u32 *)attr)[4];
		s->in_irq[5] = ((u32 *)attr)[5];
		s->in_irq[6] = ((u32 *)attr)[6];
		s->in_irq[7] = ((u32 *)attr)[7];
	} else {
		rc = VMM_EFAIL;
		goto pl061_emulator_probe_unregpic_failed;
	}

	attr = vmm_devtree_attrval(edev->node, "gpio_out_irq");
	if (attr) {
		s->out_irq[0] = ((u32 *)attr)[0];
		s->out_irq[1] = ((u32 *)attr)[1];
		s->out_irq[2] = ((u32 *)attr)[2];
		s->out_irq[3] = ((u32 *)attr)[3];
		s->out_irq[4] = ((u32 *)attr)[4];
		s->out_irq[5] = ((u32 *)attr)[5];
		s->out_irq[6] = ((u32 *)attr)[6];
		s->out_irq[7] = ((u32 *)attr)[7];
	} else {
		rc = VMM_EFAIL;
		goto pl061_emulator_probe_unregpic_failed;
	}

	s->guest = guest;
	INIT_SPIN_LOCK(&s->lock);

	edev->priv = s;

	goto pl061_emulator_probe_done;

pl061_emulator_probe_unregpic_failed:
	vmm_devemu_unregister_pic(s->guest, s->pic);
pl061_emulator_probe_freepic_failed:
	vmm_free(s->pic);
pl061_emulator_probe_freestate_failed:
	vmm_free(s);
pl061_emulator_probe_done:
	return rc;
}

static int pl061_emulator_remove(struct vmm_emudev *edev)
{
	int rc;
	struct pl061_state * s = edev->priv;

	if (s) {
		if (s->pic) {
			rc = vmm_devemu_unregister_pic(s->guest, s->pic);
			if (rc) {
				return rc;
			}
			vmm_free(s->pic);
		}
		vmm_free(s);
		edev->priv = NULL;
	}

	return VMM_OK;
}

static u8 pl061_id[12] =
  { 0x00, 0x00, 0x00, 0x00, 0x61, 0x10, 0x04, 0x00, 0x0d, 0xf0, 0x05, 0xb1 };

static struct vmm_emuid pl061_emuid_table[] = {
	{ .type = "gpio", 
	  .compatible = "primecell,pl061", 
	  .data = (void *)&pl061_id,
	},
	{ /* end of list */ },
};

static struct vmm_emulator pl061_emulator = {
	.name = "pl061",
	.match_table = pl061_emuid_table,
	.probe = pl061_emulator_probe,
	.read = pl061_emulator_read,
	.write = pl061_emulator_write,
	.reset = pl061_emulator_reset,
	.remove = pl061_emulator_remove,
};

static int __init pl061_emulator_init(void)
{
	return vmm_devemu_register_emulator(&pl061_emulator);
}

static void __exit pl061_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&pl061_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
