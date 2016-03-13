/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
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
 * @file anatop.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief i.MX6 anatop emulator.
 */

#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_devtree.h>
#include <vmm_host_io.h>

#define MODULE_DESC			"i.MX Anatop Emulator"
#define MODULE_AUTHOR			"Jimmy Durand Wesolowski"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			imx_anatop_emulator_init
#define	MODULE_EXIT			imx_anatop_emulator_exit

#define ANADIG_DIGPROG		0x260
#define ANADIG_DIGPROG_IMX6SL	0x280
#define ANADIG_DIGPROG_FAKE	0x00630002

struct anatop_priv_state {
	struct vmm_guest *guest;
	u16 digprog_offset;
	u32 digprog;
};

static int imx_anatop_emulator_read(struct vmm_emudev *edev,
				    physical_addr_t offset,
				    u32 *dst)
{
	struct anatop_priv_state *priv = edev->priv;
	u32 val = 0;
	u16 reg = offset & ~0x3;

	if (reg == priv->digprog_offset) {
		val = priv->digprog;
	} else {
		switch (reg) {
		case 0x0:
		case 0x4:
		case 0x8:
		case 0xC:
			val = 0x00013042;
			break;
		case 0x10:
		case 0x14:
		case 0x18:
		case 0x1C:
		case 0x20:
		case 0x24:
		case 0x28:
		case 0x2C:
			val = 0x00012000;
			break;
		case 0x30:
		case 0x34:
		case 0x38:
		case 0x3C:
			val = 0x00013001;
			break;
		case 0x40:
		case 0x50:
		case 0x160:
		case 0x164:
		case 0x168:
		case 0x16C:
			val = 0;
			break;
		case 0x60:
			val = 0x00000012;
			break;
		case 0x70:
		case 0x74:
		case 0x78:
		case 0x7C:
			val = 0x00011006;
			break;
		case 0x80:
		case 0xB0:
			val = 0x05F5E100;
			break;
		case 0x90:
			val = 0x2964619C;
			break;
		case 0xa0:
		case 0xa4:
		case 0xa8:
		case 0xac:
			val = 0x0001100C;
			break;
		case 0xC0:
			val = 0x10A24447;
			break;
		case 0xD0:
		case 0xD4:
		case 0xD8:
		case 0xDC:
			val = 0x00001000;
			break;
		case 0xE0:
		case 0xE4:
		case 0xE8:
		case 0xEC:
			val = 0x00011001;
			break;
		case 0xF0:
		case 0xF4:
		case 0xF8:
		case 0xFC:
			val = 0x1311100C;
			break;
		case 0x100:
		case 0x104:
		case 0x108:
		case 0x10C:
			val = 0x1018101B;
			break;
		case 0x110:
			val = 0x00001073;
			break;
		case 0x120:
			val = 0x00000F74;
			break;
		case 0x130:
			val = 0x00000F74;
			break;
		case 0x140:
			val = 0x40002010;
			break;
		case 0x150:
		case 0x154:
		case 0x158:
		case 0x15C:
			val = 0x40000000;
			break;
		case 0x170:
		case 0x174:
		case 0x178:
		case 0x17C:
			val = 0x00272727;
			break;
		default:
			vmm_printf("i.MX Anatop read at unknown register 0x%08x"
				   "\n", offset);

		}
	}

	*(u32 *)dst = val >> (8 * (offset & 0x3));
	/* vmm_printf("i.MX Anatop read at 0x%08x: 0x%08x\n", offset, *(u32 *)dst); */
	return VMM_OK;
}

static int imx_anatop_emulator_write(struct vmm_emudev *edev,
				     physical_addr_t offset,
				     u32 regmask,
				     u32 regval)
{
	/* struct anatop_priv_state *priv = arg; */

	/* vmm_printf("i.MX Anatop write 0x%08x at 0x%08x, mask 0x%08x\n", */
	/* 	   regval, offset, regmask); */

	return VMM_OK;
}

static int imx_anatop_emulator_reset(struct vmm_emudev *edev)
{
	vmm_printf("i.MX ANATOP reset\n");
	return VMM_OK;
}

static u32 imx_anatop_digprog(void)
{
	virtual_addr_t anatop;
	u32 val = ANADIG_DIGPROG_FAKE;
	struct vmm_devtree_node *root = NULL;
	struct vmm_devtree_node *node = NULL;

	root = vmm_devtree_getnode("/");
	if (NULL == node) {
		return ANADIG_DIGPROG_FAKE;
	}

	/* Is the native system not an i.MX6? */
	if (FALSE == vmm_devtree_is_compatible(root, "freescale,imx6")) {
		vmm_linfo("Anatop: Not native i.MX6 system, emulating"
			  "digprog\n");
		goto out;
	}

	/* Native i.MX6 system: Get the real value */
	node = vmm_devtree_find_compatible(root, NULL, "fsl,imx6q-anatop");
	if (NULL == node) {
		vmm_lwarning("Anatop: Failed to find anatop node\n");
		goto out;
	}

	if (VMM_OK != vmm_devtree_regmap(node, &anatop, 0)) {
		vmm_lwarning("Anatop: Failed to find anatop node\n");
	}

	/* Native test, do not use priv->digprog_offset */
	if (NULL == vmm_devtree_find_compatible(node, NULL,
						"fsl,imx6sl-anatop")) {
		val = vmm_readl((virtual_addr_t *)(anatop + ANADIG_DIGPROG));
	} else {
		val = vmm_readl((virtual_addr_t *)(anatop +
						   ANADIG_DIGPROG_IMX6SL));
	}
	vmm_devtree_regunmap(node, anatop, 0);

out:
	vmm_devtree_dref_node(root);
	vmm_linfo("Anatop: Digprog 0x%08x\n", val);
	return val;
}

static int imx_anatop_emulator_probe(struct vmm_guest *guest,
				  struct vmm_emudev *edev,
				  const struct vmm_devtree_nodeid *eid)
{
	struct anatop_priv_state *priv = NULL;

	priv = vmm_zalloc(sizeof (struct anatop_priv_state));
	if (NULL == priv) {
		vmm_printf("Failed to allocate %s private structure\n",
			   edev->node->name);
		return VMM_ENOMEM;
	}

	priv->guest = guest;
	if (vmm_devtree_is_compatible(edev->node, "fsl,imx6sl-anatop")) {
		priv->digprog_offset = ANADIG_DIGPROG_IMX6SL;
	} else {
		priv->digprog_offset = ANADIG_DIGPROG;
	}
	priv->digprog = imx_anatop_digprog();
	edev->priv = priv;

	return VMM_OK;
}

static int imx_anatop_emulator_remove(struct vmm_emudev *edev)
{
	return VMM_OK;
}

static struct vmm_devtree_nodeid imx_anatop_emuid_table[] = {
	{ .type = "misc",
	  .compatible = "fsl,imx6q-anatop",
	},
	{ /* end of list */ },
};

VMM_DECLARE_EMULATOR_SIMPLE(imx_anatop,
			    imx_anatop_emuid_table,
			    VMM_DEVEMU_LITTLE_ENDIAN,
			    imx_anatop_emulator_probe,
			    imx_anatop_emulator_remove,
			    imx_anatop_emulator_reset,
			    imx_anatop_emulator_read,
			    imx_anatop_emulator_write);

static int __init imx_anatop_emulator_init(void)
{
	return vmm_devemu_register_emulator(&imx_anatop_emulator);
}

static void __exit imx_anatop_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&imx_anatop_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
