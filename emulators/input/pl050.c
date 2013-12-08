/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file pl050.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief PrimeCell PL050 Keyboard/Mouse Interface Emulator.
 *
 * The source has been largely adapted from QEMU hw/input/pl050.c
 * 
 * Arm PrimeCell PL050 Keyboard / Mouse Interface
 *
 * Copyright (c) 2006-2007 CodeSourcery.
 * Written by Paul Brook
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_host_io.h>
#include <vmm_devemu.h>
#include <emu/input/ps2_emu.h>

#define MODULE_DESC			"PL050 Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(PS2_EMU_IPRIORITY+1)
#define	MODULE_INIT			pl050_emulator_init
#define	MODULE_EXIT			pl050_emulator_exit

struct pl050_state {
	struct vmm_guest *guest;
	bool is_mouse;
	struct ps2_emu_keyboard *kbd;
	struct ps2_emu_mouse *mou;
	u8 id[8];
	u32 irq;

	vmm_spinlock_t lock;
	u32 cr;
	u32 clk;
	u32 last;
	int pending;
};

#define PL050_TXEMPTY         (1 << 6)
#define PL050_TXBUSY          (1 << 5)
#define PL050_RXFULL          (1 << 4)
#define PL050_RXBUSY          (1 << 3)
#define PL050_RXPARITY        (1 << 2)
#define PL050_KMIC            (1 << 1)
#define PL050_KMID            (1 << 0)

static void pl050_update(void *priv, int level)
{
	int raise;
	struct pl050_state *s = priv;

	vmm_spin_lock(&s->lock);

	s->pending = level;
	raise = (s->pending && (s->cr & 0x10) != 0)
					|| (s->cr & 0x08) != 0;

	vmm_spin_unlock(&s->lock);

	vmm_devemu_emulate_irq(s->guest, s->irq, raise);
}

static int pl050_reg_read(struct pl050_state *s, u32 offset, u32 *dst)
{
	u8 val;
	u32 stat;
	int rc = VMM_OK;

	if (offset >= 0xfe0 && offset < 0x1000) {
		*dst = s->id[(offset - 0xfe0) >> 2];
		return rc;
	}

	vmm_spin_lock(&s->lock);

	switch (offset >> 2) {
	case 0: /* KMICR */
		*dst = s->cr;
		break;
	case 1: /* KMISTAT */
		val = s->last;
		val = val ^ (val >> 4);
		val = val ^ (val >> 2);
		val = (val ^ (val >> 1)) & 1;
		stat = PL050_TXEMPTY;
		if (val) {
			stat |= PL050_RXPARITY;
		}
		if (s->pending) {
			stat |= PL050_RXFULL;
		}
		*dst = stat;
		break;
	case 2: /* KMIDATA */
		if (s->pending) {
			vmm_spin_unlock(&s->lock);
			if (s->is_mouse) {
				stat = ps2_emu_read_data(&s->mou->state);
			} else {
				stat = ps2_emu_read_data(&s->kbd->state);
			}
			vmm_spin_lock(&s->lock);
			s->last = stat;
		}
		*dst = s->last;
		break;
	case 3: /* KMICLKDIV */
		*dst = s->clk;
		break;
	case 4: /* KMIIR */
		*dst = s->pending | 2;
		break;
	default:
		rc = VMM_EFAIL;
		break;
	};

	vmm_spin_unlock(&s->lock);

	return rc;
}

static int pl050_reg_write(struct pl050_state *s, u32 offset,
			   u32 src_mask, u32 src)
{
	int rc = VMM_OK, pending = 0;
	bool update = FALSE, write = FALSE;

	vmm_spin_lock(&s->lock);

	switch (offset >> 2) {
	case 0: /* KMICR */
		s->cr = (s->cr & src_mask) | (src & ~src_mask);
		pending = s->pending;
		update = TRUE;
		/* ??? Need to implement the enable/disable bit.  */
		break;
	case 2: /* KMIDATA */
		/* ??? This should toggle the TX interrupt line.  */
		/* ??? This means kbd/mouse can block each other.  */
		write = TRUE;
		break;
	case 3: /* KMICLKDIV */
		s->clk = (s->clk & src_mask) | (src & ~src_mask);
		break;
	default:
		rc = VMM_EFAIL;
		break;
	};

	vmm_spin_unlock(&s->lock);

	if (update) {
		pl050_update(s, pending);
	}

	if (write) {
		if (s->is_mouse) {
			ps2_emu_write_mouse(s->mou, src & ~src_mask);
		} else {
			ps2_emu_write_keyboard(s->kbd, src & ~src_mask);
		}
	}

	return rc;
}

static int pl050_emulator_read(struct vmm_emudev *edev,
			       physical_addr_t offset, 
			       void *dst, u32 dst_len)
{
	int rc = VMM_OK;
	u32 regval = 0x0;
	struct pl050_state *s = edev->priv;

	rc = pl050_reg_read(s, offset & ~0x3, &regval);
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

static int pl050_emulator_write(struct vmm_emudev *edev,
				physical_addr_t offset, 
				void *src, u32 src_len)
{
	int i;
	u32 regmask = 0x0, regval = 0x0;
	struct pl050_state *s = edev->priv;

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

	return pl050_reg_write(s, offset & ~0x3, regmask, regval);
}

static int pl050_emulator_reset(struct vmm_emudev *edev)
{
	int rc = VMM_OK;
	struct pl050_state *s = edev->priv;

	vmm_spin_lock(&s->lock);

	s->cr = 0;
	s->clk = 0;
	s->last = 0;
	s->pending = 0;

	vmm_spin_unlock(&s->lock);

	if (s->is_mouse) {
		rc = ps2_emu_reset_mouse(s->mou);
	} else {
		rc = ps2_emu_reset_keyboard(s->kbd);
	}

	return rc;
}

static int pl050_emulator_probe(struct vmm_guest *guest,
				struct vmm_emudev *edev,
				const struct vmm_devtree_nodeid *eid)
{
	int rc = VMM_OK;
	char name[64];
	struct pl050_state *s;

	s = vmm_zalloc(sizeof(struct pl050_state));
	if (!s) {
		rc = VMM_ENOMEM;
		goto pl050_emulator_probe_fail;
	}

	s->guest = guest;
	s->is_mouse = (eid->data) ? TRUE : FALSE;
	s->id[0] = 0x50;
	s->id[1] = 0x10;
	s->id[2] = 0x04;
	s->id[3] = 0x00;
	s->id[4] = 0x0d;
	s->id[5] = 0xf0;
	s->id[6] = 0x05;
	s->id[7] = 0xb1;
	rc = vmm_devtree_irq_get(edev->node, &s->irq, 0);
	if (rc) {
		goto pl050_emulator_probe_freestate_fail;
	}
	INIT_SPIN_LOCK(&s->lock);

	strlcpy(name, guest->name, sizeof(name));
	strlcat(name, "/", sizeof(name));
	if (strlcat(name, edev->node->name, sizeof(name)) >= sizeof(name)) {
		rc = VMM_EOVERFLOW;
		goto pl050_emulator_probe_freestate_fail;
	}

	if (s->is_mouse) {
		s->mou = ps2_emu_alloc_mouse(name, pl050_update, s);
		if (!s->mou) {
			rc = VMM_ENOMEM;
			goto pl050_emulator_probe_freestate_fail;
		}
	} else {
		s->kbd = ps2_emu_alloc_keyboard(name, pl050_update, s);
		if (!s->kbd) {
			rc = VMM_ENOMEM;
			goto pl050_emulator_probe_freestate_fail;
		}
	}

	edev->priv = s;

	return VMM_OK;

pl050_emulator_probe_freestate_fail:
	vmm_free(s);
pl050_emulator_probe_fail:
	return rc;
}

static int pl050_emulator_remove(struct vmm_emudev *edev)
{
	int rc = VMM_OK;
	struct pl050_state *s = edev->priv;

	if (s) {
		if (s->is_mouse) {
			rc = ps2_emu_free_mouse(s->mou);
			s->mou = NULL;
		} else {
			rc = ps2_emu_free_keyboard(s->kbd);
			s->kbd = NULL;
		}
		vmm_free(s);
	}

	return rc;
}

static struct vmm_devtree_nodeid pl050_emuid_table[] = {
	{ .type = "input", 
	  .compatible = "pl050,keyboard",
	  .data = (const void *)NULL,
	},
	{ .type = "input", 
	  .compatible = "pl050,mouse", 
	  .data = (const void *)1,
	},
	{ /* end of list */ },
};

static struct vmm_emulator pl050_emulator = {
	.name = "pl050",
	.match_table = pl050_emuid_table,
	.probe = pl050_emulator_probe,
	.read = pl050_emulator_read,
	.write = pl050_emulator_write,
	.reset = pl050_emulator_reset,
	.remove = pl050_emulator_remove,
};

static int __init pl050_emulator_init(void)
{
	return vmm_devemu_register_emulator(&pl050_emulator);
}

static void __exit pl050_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&pl050_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
