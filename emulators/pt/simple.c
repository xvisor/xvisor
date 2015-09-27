/**
 * Copyright (c) 2015 Anup Patel.
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
 * @file zero.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Simple pass-through emulator.
 *
 * This emulator to should be use for pass-through access to non-DMA
 * capable device which do not require IOMMU, CLK, and PINMUX configuration.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>

#define MODULE_DESC			"Simple Pass-through Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			simple_emulator_init
#define	MODULE_EXIT			simple_emulator_exit

static int simple_emulator_reset(struct vmm_emudev *edev)
{
	/* Nothing to do here. */
	return VMM_OK;
}

static int simple_emulator_probe(struct vmm_guest *guest,
				 struct vmm_emudev *edev,
				 const struct vmm_devtree_nodeid *eid)
{
	/* Nothing to do here. */
	edev->priv = NULL;

	return VMM_OK;
}

static int simple_emulator_remove(struct vmm_emudev *edev)
{
	/* Nothing to do here. */
	return VMM_OK;
}

static struct vmm_devtree_nodeid simple_emuid_table[] = {
	{ .type = "pt",
	  .compatible = "simple",
	},
	{ /* end of list */ },
};

static struct vmm_emulator simple_emulator = {
	.name = "simple",
	.match_table = simple_emuid_table,
	.endian = VMM_DEVEMU_NATIVE_ENDIAN,
	.probe = simple_emulator_probe,
	.reset = simple_emulator_reset,
	.remove = simple_emulator_remove,
};

static int __init simple_emulator_init(void)
{
	return vmm_devemu_register_emulator(&simple_emulator);
}

static void __exit simple_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&simple_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
