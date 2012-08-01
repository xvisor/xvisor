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
#include <vmm_string.h>
#include <vmm_heap.h>
#include <vmm_host_aspace.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>

#define MODULE_DESC			"Command heap"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_heap_init
#define	MODULE_EXIT			cmd_heap_exit

void cmd_heap_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   heap help\n");
	vmm_cprintf(cdev, "   heap info\n");
	vmm_cprintf(cdev, "   heap state\n");
}

int cmd_heap_info(struct vmm_chardev *cdev)
{
	int rc;
	char heap_alloc_name[32];
	virtual_addr_t heap_va;
	physical_addr_t heap_pa;
	virtual_size_t heap_sz, heap_hksz;

	if ((rc = vmm_heap_allocator_name(heap_alloc_name, 32))) {
		vmm_cprintf(cdev, "Error: Failed to get "
						"heap allocator name\n");
		return rc;
	}

	heap_va = vmm_heap_start_va();
	heap_sz = vmm_heap_size();
	heap_hksz = vmm_heap_hksize();

	if ((rc = vmm_host_page_va2pa(heap_va, &heap_pa))) {
		vmm_cprintf(cdev, "Error: Failed to get heap base PA\n");
		return rc;
	}

	vmm_cprintf(cdev, "Heap Allocator : %s\n", heap_alloc_name);

	if (sizeof(virtual_addr_t) == sizeof(u64)) {
		vmm_cprintf(cdev, "Heap Base VA   : 0x%016llx\n", heap_va);
	} else {
		vmm_cprintf(cdev, "Heap Base VA   : 0x%08x\n", heap_va);
	}

	if (sizeof(physical_addr_t) == sizeof(u64)) {
		vmm_cprintf(cdev, "Heap Base PA   : 0x%016llx\n", heap_pa);
	} else {
		vmm_cprintf(cdev, "Heap Base PA   : 0x%08x\n", heap_pa);
	}

	if (sizeof(virtual_size_t) == sizeof(u64)) {
		vmm_cprintf(cdev, "Heap Size      : 0x%016llx\n", heap_sz);
		vmm_cprintf(cdev, "Heap HK-Size   : 0x%016llx\n", heap_hksz);
	} else {
		vmm_cprintf(cdev, "Heap Size      : 0x%08x\n", heap_sz);
		vmm_cprintf(cdev, "Heap HK-Size   : 0x%08x\n", heap_hksz);
	}

	return VMM_OK;
}

int cmd_heap_state(struct vmm_chardev *cdev)
{
	return vmm_heap_print_state(cdev);
}

int cmd_heap_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	if (argc == 2) {
		if (vmm_strcmp(argv[1], "help") == 0) {
			cmd_heap_usage(cdev);
			return VMM_OK;
		} else if (vmm_strcmp(argv[1], "info") == 0) {
			return cmd_heap_info(cdev);
		} else if (vmm_strcmp(argv[1], "state") == 0) {
			return cmd_heap_state(cdev);
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
