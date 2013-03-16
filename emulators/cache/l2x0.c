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
 * @file arm_l2x0.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief L2C-210, L2C-220, L2C-310 cache controller
 * @details This source file implements the L2C-210, L2C-220, L2C-310 as 
 * dummy L2 cache controllers
 *
 * The source has been adapted from QEMU hw/arm_l2x0.c
 * 
 * ARM dummy L210, L220, PL310 cache controller.
 *
 * Copyright (c) 2010-2012 Calxeda
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_manager.h>
#include <vmm_scheduler.h>
#include <vmm_host_io.h>
#include <vmm_devemu.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"L2X0 Cache Emulator"
#define MODULE_AUTHOR			"Sukanto Ghosh"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			l2x0_cc_emulator_init
#define	MODULE_EXIT			l2x0_cc_emulator_exit

enum l2x0_id {
	L2C_210_R0P5_CACHE_ID,
	L2C_220_R1P7_CACHE_ID,
	L2C_310_R3P2_CACHE_ID,
};

static u32 l2x0_cacheid[] = {
	0x4100004F,	   	/* L2C-210 r0p5 */
	0x41000086,		/* L2C-220 r1p7 */
	0x410000C8,		/* L2C-310 r3p2 */
};

struct l2x0_state {
	vmm_spinlock_t lock;
	enum l2x0_id id;

	u32 cache_type;
	u32 ctrl;
	u32 aux_ctrl;
	u32 data_ctrl;
	u32 tag_ctrl;
	u32 filter_start;
	u32 filter_end;
};

static int l2x0_cc_emulator_read(struct vmm_emudev *edev,
				 physical_addr_t offset, 
				 void *dst, u32 dst_len)
{
	struct l2x0_state *s = edev->priv;
	int rc = VMM_OK;
	u32 cache_data;
	u32 regval = 0x0;

	offset &= 0xfff;
	if (offset >= 0x730 && offset < 0x800) {
		return rc; /* cache ops complete */
	}
	switch (offset) {
	case 0:
		regval = l2x0_cacheid[s->id];
		break;
	case 0x4:
		/* aux_ctrl values affect cache_type values */
		cache_data = (s->aux_ctrl & (7 << 17)) >> 15;
		cache_data |= (s->aux_ctrl & (1 << 16)) >> 16;
		regval = s->cache_type |= (cache_data << 18) | (cache_data << 6);
		break;
	case 0x100:
		regval = s->ctrl;
		break;
	case 0x104:
		regval = s->aux_ctrl;
		break;
	case 0x108:
		regval = s->tag_ctrl;
		break;
	case 0x10C:
		regval = s->data_ctrl;
		break;
	case 0xC00:
		regval = s->filter_start;
		break;
	case 0xC04:
		regval = s->filter_end;
		break;
	case 0xF40:
	case 0xF60:
	case 0xF80:
		regval = 0;
	default:
		rc = VMM_EFAIL;
		break;
	}

	if (!rc) {
		regval = (regval >> ((offset & 0x3) * 8));
		switch (dst_len) {
		case 1:
			*(u8 *)dst = regval & 0xFF;
			break;
		case 2:
			*(u16 *)dst = regval & 0xFFFF;
			break;
		case 4:
			*(u32 *)dst = regval;
			break;
		default:
			rc = VMM_EFAIL;
			break;
		};
	}

	return rc;
}

static int l2x0_cc_emulator_write(struct vmm_emudev *edev,
				  physical_addr_t offset, 
				  void *src, u32 src_len)
{
	struct l2x0_state *s = edev->priv;
	int rc = VMM_OK, i;
	u32 regmask = 0x0, regval = 0x0;

	switch (src_len) {
	case 1:
		regmask = 0xFFFFFF00;
		regval = *(u8 *)src;
		break;
	case 2:
		regmask = 0xFFFF0000;
		regval = *(u16 *)src;
		break;
	case 4:
		regmask = 0x00000000;
		regval = *(u32 *)src;
		break;
	default:
		return VMM_EFAIL;
		break;
	};

	for (i = 0; i < (offset & 0x3); i++) {
		regmask = (regmask << 8) | ((regmask >> 24) & 0xFF);
	}
	regval = (regval << ((offset & 0x3) * 8));

	offset &= 0xfff;
	if (offset >= 0x730 && offset < 0x800) {
		/* ignore */
		return rc;
	}
	switch (offset) {
	case 0x100:
		s->ctrl = regval & 1;
		break;
	case 0x104:
		s->aux_ctrl = regval;
		break;
	case 0x108:
		s->tag_ctrl = regval;
		break;
	case 0x10C:
		s->data_ctrl = regval;
		break;
	case 0x900:
	case 0x904:
		break;
	case 0xC00:
		s->filter_start = regval;
		break;
	case 0xC04:
		s->filter_end = regval;
		break;
	case 0xF40:
	case 0xF60:
	case 0xF80:
		break;
	default:
		rc = VMM_EFAIL; 
		break;
	}

	return rc;
}

static int l2x0_cc_emulator_reset(struct vmm_emudev *edev)
{
	struct l2x0_state *s = edev->priv;

	s->ctrl = 0;
	s->aux_ctrl = 0x02020000;
	s->tag_ctrl = 0;
	s->data_ctrl = 0;
	s->filter_start = 0;
	s->filter_end = 0;

	return VMM_OK;
}

static int l2x0_cc_emulator_probe(struct vmm_guest *guest,
				  struct vmm_emudev *edev,
				  const struct vmm_devtree_nodeid *eid)
{
	int rc = VMM_OK;
	struct l2x0_state *s;

	s = vmm_zalloc(sizeof(struct l2x0_state));
	if (!s) {
		rc = VMM_EFAIL;
		goto l2x0_probe_done;
	}

	INIT_SPIN_LOCK(&s->lock);
	s->id = (enum l2x0_id)(eid->data);

	edev->priv = s;

l2x0_probe_done:
	return rc;
}

static int l2x0_cc_emulator_remove(struct vmm_emudev *edev)
{
	struct l2x0_state *s = edev->priv;

	if (s) {
		vmm_free(s);
	}

	return VMM_OK;
}

static struct vmm_devtree_nodeid l2x0_cc_emuid_table[] = {
	{ 
	   .type = "cache", 
	   .compatible = "corelink,l2c-210", 
	   .data = (void *)L2C_210_R0P5_CACHE_ID,
	},
	{ 
	   .type = "cache", 
	   .compatible = "corelink,l2c-220", 
	   .data = (void *)L2C_220_R1P7_CACHE_ID,
	},
	{ 
	   .type = "cache", 
	   .compatible = "corelink,l2c-310", 
	   .data = (void *)L2C_310_R3P2_CACHE_ID,
	},
	{ /* end of list */ },
};

static struct vmm_emulator l2x0_cc_emulator = {
	.name = "l2x0_cc",
	.match_table = l2x0_cc_emuid_table,
	.probe = l2x0_cc_emulator_probe,
	.read = l2x0_cc_emulator_read,
	.write = l2x0_cc_emulator_write,
	.reset = l2x0_cc_emulator_reset,
	.remove = l2x0_cc_emulator_remove,
};

static int __init l2x0_cc_emulator_init(void)
{
	return vmm_devemu_register_emulator(&l2x0_cc_emulator);
}

static void __exit l2x0_cc_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&l2x0_cc_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);

