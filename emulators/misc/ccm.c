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
 * @file ccm.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief i.MX6 CCM emulator.
 */

#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>

#define MODULE_DESC			"i.MX CCM Emulator"
#define MODULE_AUTHOR			"Jimmy Durand Wesolowski"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			imx_ccm_emulator_init
#define	MODULE_EXIT			imx_ccm_emulator_exit

#define ANADIG_DIGPROG		0x260
#define ANADIG_DIGPROG_IMX6SL	0x280

const unsigned int	reg_reset[] = {
	/* 0x00 */
	0x040116FF,
	0x00000000,
	0x00000010,
	0x00000100,
	/* 0x10 */
	0x00000000,
	0x00018D00,
	0x00020324,
	0x00F00000,
	/* 0x20 */
	0x2B92F060,
	0x00490B00,
	0x0EC102C1,
	0x000736C1,
	/* 0x30 */
	0x33F71F92,
	0x0002A150,
	0x0002A150,
	0x00010841,
	/* 0x40 */
	0x00000000, /* RESERVED */
	0x00000000,
	0x00000000,
	0x00000000, /* RESERVED */
	/* 0x50 */
	0x00000000, /* RESERVED */
	0x00000079,
	0x00000000,
	0xFFFFFFFF,
	/* 0x60 */
	0x000A0001,
	0x0000FE62,
	0xFFFFFFFF,
	0xFFFFFFFF,
	/* 0x70 */
	0xFC3FFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF,
	/* 0x80 */
	0xFFFFFFFF,
	0x00000000, /* RESERVED */
	0xFFFFFFFF
};

typedef struct		ccm_t {
	unsigned int	regs[sizeof (reg_reset) / sizeof (reg_reset[0])];
	/* Add reg masks? */
	vmm_spinlock_t	lock;
}			ccm_t;

static int imx_ccm_emulator_read(struct vmm_emudev *edev,
				 physical_addr_t offset,
				 u32 *dst)
{
	ccm_t *ccm = edev->priv;
	u16 reg = offset >> 2;

	vmm_spin_lock(&ccm->lock);
	*dst = ccm->regs[reg];
	vmm_spin_unlock(&ccm->lock);
	/* vmm_printf("i.MX CCM read at 0x%08x: 0x%08x\n", offset, *(u32 *)dst); */
	return VMM_OK;
}

static int imx_ccm_emulator_write(struct vmm_emudev *edev,
				  physical_addr_t offset,
				  u32 regmask,
				  u32 regval)
{
	ccm_t *ccm = edev->priv;
	u16 reg = offset >> 2;

	/* vmm_printf("i.MX CCM write 0x%08x at 0x%08x, mask 0x%08x\n", */
	/* 	   regval, offset, regmask); */

	vmm_spin_lock(&ccm->lock);
	ccm->regs[reg] = (ccm->regs[reg] & regmask) | (regval & ~regmask);
	vmm_spin_unlock(&ccm->lock);

	return VMM_OK;
}

static int imx_ccm_emulator_reset(struct vmm_emudev *edev)
{
	ccm_t *ccm = edev->priv;

	vmm_printf("i.MX CCM reset\n");
	vmm_spin_lock(&ccm->lock);
	memcpy(ccm->regs, reg_reset, sizeof (ccm->regs));
	vmm_spin_unlock(&ccm->lock);
	return VMM_OK;
}

static int imx_ccm_emulator_probe(struct vmm_guest *guest,
				  struct vmm_emudev *edev,
				  const struct vmm_devtree_nodeid *eid)
{
	ccm_t *ccm = NULL;

	ccm = vmm_malloc(sizeof (ccm_t));
	if (NULL == ccm) {
		vmm_lerror("CCM: Failed to allocate structure memory\n");
		return VMM_ENOMEM;
	}
	edev->priv = ccm;
	memcpy(ccm->regs, reg_reset, sizeof (ccm->regs));
	INIT_SPIN_LOCK(&ccm->lock);

	return VMM_OK;
}

static int imx_ccm_emulator_remove(struct vmm_emudev *edev)
{
	vmm_free(edev->priv);
	return VMM_OK;
}

static struct vmm_devtree_nodeid imx_ccm_emuid_table[] = {
	{ .type = "misc",
	  .compatible = "fsl,imx6q-ccm",
	},
	{ /* end of list */ },
};

VMM_DECLARE_EMULATOR_SIMPLE(imx_ccm,
			    imx_ccm_emuid_table,
			    VMM_DEVEMU_LITTLE_ENDIAN,
			    imx_ccm_emulator_probe,
			    imx_ccm_emulator_remove,
			    imx_ccm_emulator_reset,
			    imx_ccm_emulator_read,
			    imx_ccm_emulator_write);

static int __init imx_ccm_emulator_init(void)
{
	return vmm_devemu_register_emulator(&imx_ccm_emulator);
}

static void __exit imx_ccm_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&imx_ccm_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
