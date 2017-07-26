/**
 * Copyright (c) 2017 Anup Patel.
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
 * @file cmd_shmem.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of shmem command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_host_aspace.h>
#include <vmm_shmem.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"Command shmem"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_shmem_init
#define	MODULE_EXIT			cmd_shmem_exit

static void cmd_shmem_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   shmem help\n");
	vmm_cprintf(cdev, "   shmem list\n");
	vmm_cprintf(cdev, "   shmem create <name> <phys_size> [<align_order>]\n");
	vmm_cprintf(cdev, "   shmem destroy <name>\n");
}

static int cmd_shmem_list_iter(struct vmm_shmem *shm, void *priv)
{
	struct vmm_chardev *cdev = priv;
	char addr[32], size[32], aorder[32], rcount[32];

	vmm_snprintf(addr, sizeof(addr), "0x%"PRIPADDR,
		     vmm_shmem_get_addr(shm));
	vmm_snprintf(size, sizeof(size), "0x%"PRIPADDR,
		     vmm_shmem_get_size(shm));
	vmm_snprintf(aorder, sizeof(aorder), "%d",
		     vmm_shmem_get_align_order(shm));
	vmm_snprintf(rcount, sizeof(rcount), "%d",
		     vmm_shmem_get_ref_count(shm));
	vmm_cprintf(cdev, "%-16s %-18s %-18s %-12s %-12s\n",
		    vmm_shmem_get_name(shm), addr, size, aorder, rcount);

	return VMM_OK;
}

static int cmd_shmem_list(struct vmm_chardev *cdev)
{
	int rc;

	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_cprintf(cdev, "%-16s %-18s %-18s %-12s %-12s\n",
			  "Name", "Physical Address", "Physical Size",
			  "Align Order", "Ref Count");
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	rc = vmm_shmem_iterate(cmd_shmem_list_iter, cdev);
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");

	return rc;
}

static int cmd_shmem_create(struct vmm_chardev *cdev, const char *name,
			  physical_size_t size, u32 align_order)
{
	struct vmm_shmem *shm;

	shm = vmm_shmem_create(name, size, align_order, NULL);
	if (VMM_IS_ERR_OR_NULL(shm)) {
		vmm_cprintf(cdev, "Failed to create %s shared memory\n",
			    name);
		return VMM_EFAIL;
	}

	vmm_cprintf(cdev, "Created %s shared memory\n", name);

	return VMM_OK;
}

static int cmd_shmem_destroy(struct vmm_chardev *cdev, const char *name)
{
	struct vmm_shmem *shm = vmm_shmem_find_byname(name);

	if (!shm) {
		vmm_cprintf(cdev, "Failed to find %s shared memory\n",
			    name);
		return VMM_ENOTAVAIL;
	}

	vmm_shmem_dref(shm);
	vmm_shmem_destroy(shm);

	vmm_cprintf(cdev, "Destroyed %s shared memory\n", name);

	return VMM_OK;
}

static int cmd_shmem_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	physical_size_t size;
	u32 align_order = VMM_PAGE_SHIFT;

	if (argc <= 1) {
		goto fail;
	}

	if (strcmp(argv[1], "help") == 0) {
		cmd_shmem_usage(cdev);
		return VMM_OK;
	} else if ((strcmp(argv[1], "list") == 0) && (argc == 2)) {
		return cmd_shmem_list(cdev);
	} else if ((strcmp(argv[1], "create") == 0) &&
		   ((argc == 5) || (argc == 4))) {
		size = (physical_size_t)strtoull(argv[3], NULL, 0);
		if (argc == 5) {
			align_order = atoi(argv[4]);
		}
		return cmd_shmem_create(cdev, argv[2], size, align_order);
	} else if ((strcmp(argv[1], "destroy") == 0) && (argc == 3)) {
		return cmd_shmem_destroy(cdev, argv[2]);
	}

fail:
	cmd_shmem_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_shmem = {
	.name = "shmem",
	.desc = "shared memory commands",
	.usage = cmd_shmem_usage,
	.exec = cmd_shmem_exec,
};

static int __init cmd_shmem_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_shmem);
}

static void __exit cmd_shmem_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_shmem);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
