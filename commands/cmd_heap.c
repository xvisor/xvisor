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
 * @file cmd_heap.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief command for heap status.
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_version.h>
#include <vmm_heap.h>
#include <vmm_host_aspace.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>

#define MODULE_DESC			"Command heap"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_heap_init
#define	MODULE_EXIT			cmd_heap_exit

static void cmd_heap_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   heap help\n");
	vmm_cprintf(cdev, "   heap info\n");
	vmm_cprintf(cdev, "   heap state\n");
	vmm_cprintf(cdev, "   heap dma_info\n");
	vmm_cprintf(cdev, "   heap dma_state\n");
}

static int heap_info(struct vmm_chardev *cdev,
		     bool is_normal, virtual_addr_t heap_va,
		     u64 heap_sz, u64 heap_hksz, u64 heap_freesz)
{
	int rc;
	physical_addr_t heap_pa;
	u64 pre, heap_usesz;

	if (is_normal) {
		heap_usesz = heap_sz - heap_hksz - heap_freesz;
	} else {
		heap_usesz = heap_sz - heap_freesz;
	}

	if ((rc = vmm_host_va2pa(heap_va, &heap_pa))) {
		vmm_cprintf(cdev, "Error: Failed to get heap base PA\n");
		return rc;
	}

	vmm_cprintf(cdev, "Base Virtual Addr  : ");
	vmm_cprintf(cdev, "0x%"PRIADDR"\n", heap_va);

	vmm_cprintf(cdev, "Base Physical Addr : ");
	vmm_cprintf(cdev, "0x%"PRIPADDR"\n", heap_pa);

	pre = 1000; /* Division correct upto 3 decimal points */

	vmm_cprintf(cdev, "House-Keeping Size : ");
	heap_hksz = (heap_hksz * pre) >> 10;
	vmm_cprintf(cdev, "%lld.%03lld KB\n",
			udiv64(heap_hksz, pre), umod64(heap_hksz, pre));

	vmm_cprintf(cdev, "Used Space Size    : ");
	heap_usesz = (heap_usesz * pre) >> 10;
	vmm_cprintf(cdev, "%lld.%03lld KB\n",
			udiv64(heap_usesz, pre), umod64(heap_usesz, pre));

	vmm_cprintf(cdev, "Free Space Size    : ");
	heap_freesz = (heap_freesz * pre) >> 10;
	vmm_cprintf(cdev, "%lld.%03lld KB\n",
			udiv64(heap_freesz, pre), umod64(heap_freesz, pre));

	vmm_cprintf(cdev, "Total Size         : ");
	heap_sz = (heap_sz * pre) >> 10;
	vmm_cprintf(cdev, "%lld.%03lld KB\n",
			udiv64(heap_sz, pre), umod64(heap_sz, pre));

	return VMM_OK;
}

static int cmd_heap_info(struct vmm_chardev *cdev)
{
	return heap_info(cdev, TRUE,
			 vmm_normal_heap_start_va(),
			 vmm_normal_heap_size(),
			 vmm_normal_heap_hksize(),
			 vmm_normal_heap_free_size());
}

static int cmd_heap_state(struct vmm_chardev *cdev)
{
	return vmm_normal_heap_print_state(cdev);
}

static int cmd_heap_dma_info(struct vmm_chardev *cdev)
{
	return heap_info(cdev, FALSE,
			 vmm_dma_heap_start_va(),
			 vmm_dma_heap_size(),
			 vmm_dma_heap_hksize(),
			 vmm_dma_heap_free_size());
}

static int cmd_heap_dma_state(struct vmm_chardev *cdev)
{
	return vmm_dma_heap_print_state(cdev);
}

static int cmd_heap_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	if (argc == 2) {
		if (strcmp(argv[1], "help") == 0) {
			cmd_heap_usage(cdev);
			return VMM_OK;
		} else if (strcmp(argv[1], "info") == 0) {
			return cmd_heap_info(cdev);
		} else if (strcmp(argv[1], "state") == 0) {
			return cmd_heap_state(cdev);
		} else if (strcmp(argv[1], "dma_info") == 0) {
			return cmd_heap_dma_info(cdev);
		} else if (strcmp(argv[1], "dma_state") == 0) {
			return cmd_heap_dma_state(cdev);
		}
	}
	cmd_heap_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_heap = {
	.name = "heap",
	.desc = "show heap status",
	.usage = cmd_heap_usage,
	.exec = cmd_heap_exec,
};

static int __init cmd_heap_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_heap);
}

static void __exit cmd_heap_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_heap);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
