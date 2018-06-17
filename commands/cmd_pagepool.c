/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file cmd_pagepool.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief command for page pool managment.
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_version.h>
#include <vmm_pagepool.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>

#define MODULE_DESC			"Command pagepool"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_pagepool_init
#define	MODULE_EXIT			cmd_pagepool_exit

static void cmd_pagepool_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   pagepool help\n");
	vmm_cprintf(cdev, "   pagepool info\n");
	vmm_cprintf(cdev, "   pagepool state\n");
}

static int cmd_pagepool_info(struct vmm_chardev *cdev)
{
	int i;
	u32 entry_count = 0;
	u32 hugepage_count = 0;
	u32 page_count = 0;
	u32 page_avail_count = 0;
	virtual_size_t space = 0;
	u64 pre, sz;

	for (i = 0; i < VMM_PAGEPOOL_MAX; i++) {
		space += vmm_pagepool_space(i);
		entry_count += vmm_pagepool_entry_count(i);
		hugepage_count += vmm_pagepool_hugepage_count(i);
		page_count += vmm_pagepool_page_count(i);
		page_avail_count += vmm_pagepool_page_avail_count(i);
	}

	vmm_cprintf(cdev, "Entry Count      : %d (0x%08x)\n",
		    entry_count, entry_count);
	vmm_cprintf(cdev, "Hugepage Count   : %d (0x%08x)\n",
		    hugepage_count, hugepage_count);
	vmm_cprintf(cdev, "Avail Page Count : %d (0x%08x)\n",
		    page_avail_count, page_avail_count);
	vmm_cprintf(cdev, "Total Page Count : %d (0x%08x)\n",
		    page_count, page_count);
	sz = space;
	pre = 1000;
	sz = (sz * pre) >> 10;
	vmm_cprintf(cdev, "Total Space      : %"PRId64".%03"PRId64" KB\n",
		    udiv64(sz, pre), umod64(sz, pre));

	return VMM_OK;
}

static int cmd_pagepool_state(struct vmm_chardev *cdev)
{
	int i;
	u32 _entry_count, entry_count = 0;
	u32 _hugepage_count, hugepage_count = 0;
	u32 _page_count, page_count = 0;
	u32 _page_avail_count, page_avail_count = 0;
	virtual_size_t _space, space = 0;

	vmm_cprintf(cdev, "----------------------------------------"
			  "---------------------------------------\n");

	vmm_cprintf(cdev, " %-20s %-11s %-10s %-10s %-11s %-11s\n",
		    "Name", "Space (KB)", "Entries", "Hugepages",
		    "AvailPages", "TotalPages");

	vmm_cprintf(cdev, "----------------------------------------"
			  "---------------------------------------\n");

	for (i = 0; i < VMM_PAGEPOOL_MAX; i++) {
		_space = vmm_pagepool_space(i);
		_entry_count = vmm_pagepool_entry_count(i);
		_hugepage_count = vmm_pagepool_hugepage_count(i);
		_page_count = vmm_pagepool_page_count(i);
		_page_avail_count = vmm_pagepool_page_avail_count(i);

		vmm_cprintf(cdev, " %-20s %-11d %-10d %-10d %-11d %-11d\n",
			    vmm_pagepool_name(i), (u32)(_space >> 10),
			    _entry_count, _hugepage_count,
			    _page_avail_count, _page_count);

		space += _space;
		entry_count += _entry_count;
		hugepage_count += _hugepage_count;
		page_count += _page_count;
		page_avail_count += _page_avail_count;
	}

	vmm_cprintf(cdev, "----------------------------------------"
			  "---------------------------------------\n");

	vmm_cprintf(cdev, " %-20s %-11d %-10d %-10d %-11d %-11d\n",
		    "TOTAL", (u32)(space >> 10),
		    entry_count, hugepage_count,
		    page_avail_count, page_count);

	vmm_cprintf(cdev, "----------------------------------------"
			  "---------------------------------------\n");

	return VMM_OK;
}

static int cmd_pagepool_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	if (argc == 2) {
		if (strcmp(argv[1], "help") == 0) {
			cmd_pagepool_usage(cdev);
			return VMM_OK;
		} else if (strcmp(argv[1], "info") == 0) {
			return cmd_pagepool_info(cdev);
		} else if (strcmp(argv[1], "state") == 0) {
			return cmd_pagepool_state(cdev);
		}
	}
	cmd_pagepool_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_pagepool = {
	.name = "pagepool",
	.desc = "pagepool commands",
	.usage = cmd_pagepool_usage,
	.exec = cmd_pagepool_exec,
};

static int __init cmd_pagepool_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_pagepool);
}

static void __exit cmd_pagepool_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_pagepool);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
