/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file cmd_ram.c
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of ram command
 */

#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>

#define MODULE_VARID			cmd_ram_module
#define MODULE_NAME			"Command ram"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_ram_init
#define	MODULE_EXIT			cmd_ram_exit

void cmd_ram_usage(vmm_chardev_t *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   ram help\n");
	vmm_cprintf(cdev, "   ram stats\n");
	vmm_cprintf(cdev, "   ram bitmap [<column count>]\n");
}

void cmd_ram_stats(vmm_chardev_t *cdev)
{
	u32 free = vmm_host_ram_free_frame_count();
	u32 total = vmm_host_ram_total_frame_count();
	physical_addr_t base = vmm_host_ram_base();
	vmm_cprintf(cdev, "Base Address : 0x%08x\n", base);
	vmm_cprintf(cdev, "Frame Size   : %d (0x%08x)\n", 
					VMM_PAGE_SIZE, VMM_PAGE_SIZE);
	vmm_cprintf(cdev, "Free Frames  : %d (0x%08x)\n", free, free);
	vmm_cprintf(cdev, "Total Frames : %d (0x%08x)\n", total, total);
}

void cmd_ram_bitmap(vmm_chardev_t *cdev, int colcnt)
{
	u32 ite, total = vmm_host_ram_total_frame_count();
	physical_addr_t base = vmm_host_ram_base();
	vmm_cprintf(cdev, "0 : free\n");
	vmm_cprintf(cdev, "1 : used");
	for (ite = 0; ite < total; ite++) {
		if (ite % colcnt == 0) {
			vmm_cprintf(cdev, "\n0x%08x: ", base + ite * VMM_PAGE_SIZE);
		}
		if (vmm_host_ram_frame_isfree(base + ite * VMM_PAGE_SIZE)) {
			vmm_cprintf(cdev, "0");
		} else {
			vmm_cprintf(cdev, "1");
		}
	}
	vmm_cprintf(cdev, "\n");
}

int cmd_ram_exec(vmm_chardev_t *cdev, int argc, char **argv)
{
	int colcnt;
	if (1 < argc) {
		if (vmm_strcmp(argv[1], "help") == 0) {
			cmd_ram_usage(cdev);
			return VMM_OK;
		} else if (vmm_strcmp(argv[1], "stats") == 0) {
			cmd_ram_stats(cdev);
			return VMM_OK;
		} else if (vmm_strcmp(argv[1], "bitmap") == 0) {
			if (2 < argc) {
				colcnt = vmm_str2int(argv[2], 10);
			} else {
				colcnt = 64;
			}
			cmd_ram_bitmap(cdev, colcnt);
			return VMM_OK;
		}
	}
	cmd_ram_usage(cdev);
	return VMM_EFAIL;
}

static vmm_cmd_t cmd_ram = {
	.name = "ram",
	.desc = "RAM status",
	.usage = cmd_ram_usage,
	.exec = cmd_ram_exec,
};

static int cmd_ram_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_ram);
}

static void cmd_ram_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_ram);
}

VMM_DECLARE_MODULE(MODULE_VARID, 
			MODULE_NAME, 
			MODULE_AUTHOR, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
