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
 * @file cmd_vapool.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of vapool command
 */

#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_math.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>

#define MODULE_VARID			cmd_vapool_module
#define MODULE_NAME			"Command vapool"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_vapool_init
#define	MODULE_EXIT			cmd_vapool_exit

void cmd_vapool_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   vapool help\n");
	vmm_cprintf(cdev, "   vapool stats\n");
	vmm_cprintf(cdev, "   vapool bitmap [<column count>]\n");
}

void cmd_vapool_stats(struct vmm_chardev *cdev)
{
	u32 free = vmm_host_vapool_free_page_count();
	u32 total = vmm_host_vapool_total_page_count();
	virtual_addr_t base = vmm_host_vapool_base();
	vmm_cprintf(cdev, "Base Address : 0x%08x\n", base);
	vmm_cprintf(cdev, "Page Size    : %d (0x%08x)\n", 
					VMM_PAGE_SIZE, VMM_PAGE_SIZE);
	vmm_cprintf(cdev, "Free Pages   : %d (0x%08x)\n", free, free);
	vmm_cprintf(cdev, "Total Pages  : %d (0x%08x)\n", total, total);
}

void cmd_vapool_bitmap(struct vmm_chardev *cdev, int colcnt)
{
	u32 ite, total = vmm_host_vapool_total_page_count();
	virtual_addr_t base = vmm_host_vapool_base();
	vmm_cprintf(cdev, "0 : free\n");
	vmm_cprintf(cdev, "1 : used");
	for (ite = 0; ite < total; ite++) {
		if (vmm_umod32(ite, colcnt) == 0) {
			vmm_cprintf(cdev, "\n0x%08x: ", base + ite * VMM_PAGE_SIZE);
		}
		if (vmm_host_vapool_page_isfree(base + ite * VMM_PAGE_SIZE)) {
			vmm_cprintf(cdev, "0");
		} else {
			vmm_cprintf(cdev, "1");
		}
	}
	vmm_cprintf(cdev, "\n");
}

int cmd_vapool_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	int colcnt;
	if (1 < argc) {
		if (vmm_strcmp(argv[1], "help") == 0) {
			cmd_vapool_usage(cdev);
			return VMM_OK;
		} else if (vmm_strcmp(argv[1], "stats") == 0) {
			cmd_vapool_stats(cdev);
			return VMM_OK;
		} else if (vmm_strcmp(argv[1], "bitmap") == 0) {
			if (2 < argc) {
				colcnt = vmm_str2int(argv[2], 10);
			} else {
				colcnt = 64;
			}
			cmd_vapool_bitmap(cdev, colcnt);
			return VMM_OK;
		}
	}
	cmd_vapool_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_vapool = {
	.name = "vapool",
	.desc = "virtual address space pool status",
	.usage = cmd_vapool_usage,
	.exec = cmd_vapool_exec,
};

static int __init cmd_vapool_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_vapool);
}

static void cmd_vapool_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_vapool);
}

VMM_DECLARE_MODULE(MODULE_VARID, 
			MODULE_NAME, 
			MODULE_AUTHOR, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
