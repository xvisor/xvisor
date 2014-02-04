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
 * @file sp810.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief PrimeCell SP810 System Controller Emulator.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>

#define MODULE_DESC			"SP810 Serial Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			sp810_emulator_init
#define	MODULE_EXIT			sp810_emulator_exit

struct sp810_state {
	struct vmm_guest *guest;
	vmm_spinlock_t lock;
	u32 sysctrl;
};

static int sp810_reg_read(struct sp810_state *s,
			  u32 offset, u32 *dst)
{
	int rc = VMM_OK;

	vmm_spin_lock(&s->lock);

	if (offset >= 0xfe0 && offset < 0x1000) {
		/* it is not clear what ID shall be returned by the sp810 */
		/* for now we return 0. It seems to work for linux */
		*dst = 0;
	} else {
		switch (offset >> 2) {
		case 0x00: /* SYSCTRL */
			*dst = s->sysctrl;
			break;
		default:
			rc = VMM_EFAIL;
			break;
		}
	}

	vmm_spin_unlock(&s->lock);

	return rc;
}

static int sp810_reg_write(struct sp810_state *s,
			   u32 offset, u32 regmask, u32 regval)
{
	int rc = VMM_OK;

	vmm_spin_lock(&s->lock);

	switch (offset >> 2) {
	case 0x00: /* SYSCTRL */
		s->sysctrl &= regmask;
		s->sysctrl |= regval;
		break;
	default:
		rc = VMM_EFAIL;
		break;
	}

	vmm_spin_unlock(&s->lock);

	return rc;
}

static int sp810_emulator_read8(struct vmm_emudev *edev,
				physical_addr_t offset, 
				u8 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = sp810_reg_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFF;
	}

	return rc;
}

static int sp810_emulator_read16(struct vmm_emudev *edev,
				 physical_addr_t offset, 
				 u16 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = sp810_reg_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFFFF;
	}

	return rc;
}

static int sp810_emulator_read32(struct vmm_emudev *edev,
				 physical_addr_t offset, 
				 u32 *dst)
{
	return sp810_reg_read(edev->priv, offset, dst);
}

static int sp810_emulator_write8(struct vmm_emudev *edev,
				 physical_addr_t offset, 
				 u8 src)
{
	return sp810_reg_write(edev->priv, offset, 0xFFFFFF00, src);
}

static int sp810_emulator_write16(struct vmm_emudev *edev,
				  physical_addr_t offset, 
				  u16 src)
{
	return sp810_reg_write(edev->priv, offset, 0xFFFF0000, src);
}

static int sp810_emulator_write32(struct vmm_emudev *edev,
				  physical_addr_t offset, 
				  u32 src)
{
	return sp810_reg_write(edev->priv, offset, 0x00000000, src);
}

static int sp810_emulator_reset(struct vmm_emudev *edev)
{
	struct sp810_state * s = edev->priv;

	vmm_spin_lock(&s->lock);

	s->sysctrl = 0;

	vmm_spin_unlock(&s->lock);

	return VMM_OK;
}

static int sp810_emulator_probe(struct vmm_guest *guest,
				struct vmm_emudev *edev,
				const struct vmm_devtree_nodeid *eid)
{
	int rc = VMM_OK;
	struct sp810_state *s;

	s = vmm_zalloc(sizeof(struct sp810_state));
	if (!s) {
		rc = VMM_EFAIL;
		goto sp810_emulator_probe_done;
	}

	s->guest = guest;
	INIT_SPIN_LOCK(&s->lock);

	edev->priv = s;

sp810_emulator_probe_done:
	return rc;
}

static int sp810_emulator_remove(struct vmm_emudev *edev)
{
	struct sp810_state *s = edev->priv;

	if (s) {
		vmm_free(s);
		edev->priv = NULL;
	}

	return VMM_OK;
}

static struct vmm_devtree_nodeid sp810_emuid_table[] = {
	{ .type = "sys", 
	  .compatible = "primecell,sp810", 
	},
	{ /* end of list */ },
};

static struct vmm_emulator sp810_emulator = {
	.name = "sp810",
	.match_table = sp810_emuid_table,
	.endian = VMM_DEVEMU_LITTLE_ENDIAN,
	.probe = sp810_emulator_probe,
	.read8 = sp810_emulator_read8,
	.write8 = sp810_emulator_write8,
	.read16 = sp810_emulator_read16,
	.write16 = sp810_emulator_write16,
	.read32 = sp810_emulator_read32,
	.write32 = sp810_emulator_write32,
	.reset = sp810_emulator_reset,
	.remove = sp810_emulator_remove,
};

static int __init sp810_emulator_init(void)
{
	return vmm_devemu_register_emulator(&sp810_emulator);
}

static void __exit sp810_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&sp810_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
