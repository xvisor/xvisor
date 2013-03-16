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
 * @file zero.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Zero read-only memory emulator.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_host_io.h>
#include <vmm_devemu.h>

#define MODULE_DESC			"Zero Device Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			zero_emulator_init
#define	MODULE_EXIT			zero_emulator_exit

static int zero_emulator_read(struct vmm_emudev *edev,
			       physical_addr_t offset, 
			       void *dst, u32 dst_len)
{
	int rc = VMM_OK;

	switch (dst_len) {
	case 1:
		*(u8 *)dst = 0x0;
		break;
	case 2:
		*(u16 *)dst = 0x0;
		break;
	case 4:
		*(u32 *)dst = 0x0;
		break;
	default:
		rc = VMM_EFAIL;
		break;
	};

	return rc;
}

static int zero_emulator_write(struct vmm_emudev *edev,
				physical_addr_t offset, 
				void *src, u32 src_len)
{
	return VMM_OK;
}

static int zero_emulator_reset(struct vmm_emudev *edev)
{
	return VMM_OK;
}

static int zero_emulator_probe(struct vmm_guest *guest,
				struct vmm_emudev *edev,
				const struct vmm_devtree_nodeid *eid)
{
	edev->priv = NULL;

	return VMM_OK;
}

static int zero_emulator_remove(struct vmm_emudev *edev)
{
	return VMM_OK;
}

static struct vmm_devtree_nodeid zero_emuid_table[] = {
	{ .type = "misc", 
	  .compatible = "zero", 
	},
	{ /* end of list */ },
};

static struct vmm_emulator zero_emulator = {
	.name = "zero",
	.match_table = zero_emuid_table,
	.probe = zero_emulator_probe,
	.read = zero_emulator_read,
	.write = zero_emulator_write,
	.reset = zero_emulator_reset,
	.remove = zero_emulator_remove,
};

static int __init zero_emulator_init(void)
{
	return vmm_devemu_register_emulator(&zero_emulator);
}

static void __exit zero_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&zero_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
