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
 * @file cmd_guest.c
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of guest command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_devtree.h>
#include <vmm_manager.h>
#include <vmm_host_aspace.h>
#include <vmm_guest_aspace.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>

#define MODULE_VARID			cmd_guest_module
#define MODULE_NAME			"Command guest"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_guest_init
#define	MODULE_EXIT			cmd_guest_exit

void cmd_guest_usage(vmm_chardev_t *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   guest help\n");
	vmm_cprintf(cdev, "   guest list\n");
	vmm_cprintf(cdev, "   guest load    <guest_id> "
			  "<src_hphys_addr> <dest_gphys_addr> <img_sz>\n");
	vmm_cprintf(cdev, "   guest reset   <guest_id>\n");
	vmm_cprintf(cdev, "   guest kick    <guest_id>\n");
	vmm_cprintf(cdev, "   guest pause   <guest_id>\n");
	vmm_cprintf(cdev, "   guest resume  <guest_id>\n");
	vmm_cprintf(cdev, "   guest halt    <guest_id>\n");
	vmm_cprintf(cdev, "   guest dumpreg <guest_id>\n");
	vmm_cprintf(cdev, "Note:\n");
	vmm_cprintf(cdev, "   if guest_id is -1 then it means all guests\n");
}

void cmd_guest_list(vmm_chardev_t *cdev)
{
	int id, count;
	char path[256];
	vmm_guest_t *guest;
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_cprintf(cdev, "| %-5s| %-16s| %-52s|\n", 
			 "Num", "Name", "Device Path");
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	count = vmm_manager_guest_count();
	for (id = 0; id < count; id++) {
		guest = vmm_manager_guest(id);
		vmm_devtree_getpath(path, guest->node);
		vmm_cprintf(cdev, "| %-5d| %-16s| %-52s|\n", 
				  id, guest->node->name, path);
	}
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
}

int cmd_guest_load(vmm_chardev_t *cdev, int id, 
		   physical_addr_t src_hphys_addr, 
		   physical_addr_t dest_gphys_addr, 
		   u32 img_sz)
{
	vmm_guest_t *guest;
	vmm_guest_region_t *guest_region;
	virtual_addr_t src_hvaddr, dest_gvaddr;

	guest = vmm_manager_guest(id);
	if (guest) {
		guest_region = vmm_guest_aspace_getregion(guest, 
							dest_gphys_addr);
		if (!guest_region) {
			vmm_cprintf(cdev, "Error: Cannot find a guest reqion "
					  "containing address 0x%X\n", 
					  dest_gphys_addr);
			return VMM_EFAIL;
		}

		if (img_sz > guest_region->phys_size) {
			vmm_cprintf(cdev, "(%s) Error: Image size is greater "
					  "than the size of the requested "
					  "guest region.\n", __FUNCTION__);
			return VMM_EFAIL;
		}

		dest_gvaddr = vmm_host_iomap(guest_region->hphys_addr, 
					     guest_region->phys_size);
		if (!dest_gvaddr) {
			vmm_cprintf(cdev, "(%s) Error: Cannot map host physical"
					  " to host virtual.\n", __FUNCTION__);
			return VMM_EFAIL;
		}

		src_hvaddr = vmm_host_iomap(src_hphys_addr, 
					    guest_region->phys_size);
		if (!src_hvaddr) {
			vmm_cprintf(cdev, "(%s) Error: Cannot map host source "
					  "physical to host virtual.\n", 
					  __FUNCTION__);
			return VMM_EFAIL;
		}

		vmm_memcpy((void *)dest_gvaddr, (void *)src_hvaddr, img_sz);

		return VMM_OK;
	}

	return VMM_EFAIL;
}

int cmd_guest_reset(vmm_chardev_t *cdev, int id)
{
	int ret = VMM_EFAIL;
	vmm_guest_t *guest = vmm_manager_guest(id);
	if (guest) {
		if ((ret = vmm_manager_guest_reset(guest))) {
			vmm_cprintf(cdev, "%s: Failed to reset\n", 
					  guest->node->name);
		} else {
			vmm_cprintf(cdev, "%s: Reset done\n", 
					  guest->node->name);
		}
	} else {
		vmm_cprintf(cdev, "Failed to find guest\n");
	}
	return ret;
}

int cmd_guest_kick(vmm_chardev_t *cdev, int id)
{
	int ret = VMM_EFAIL;
	vmm_guest_t *guest = vmm_manager_guest(id);
	if (guest) {
		if ((ret = vmm_manager_guest_kick(guest))) {
			vmm_cprintf(cdev, "%s: Failed to kick\n", 
					  guest->node->name);
		} else {
			vmm_cprintf(cdev, "%s: Kicked\n", 
					  guest->node->name);
		}
	} else {
		vmm_cprintf(cdev, "Failed to find guest\n");
	}
	return ret;
}

int cmd_guest_pause(vmm_chardev_t *cdev, int id)
{
	int ret = VMM_EFAIL;
	vmm_guest_t *guest = vmm_manager_guest(id);
	if (guest) {
		;
		if ((ret = vmm_manager_guest_pause(guest))) {
			vmm_cprintf(cdev, "%s: Failed to pause\n", 
					  guest->node->name);
		} else {
			vmm_cprintf(cdev, "%s: Paused\n", 
					  guest->node->name);
		}
	} else {
		vmm_cprintf(cdev, "Failed to find guest\n");
	}
	return ret;
}

int cmd_guest_resume(vmm_chardev_t *cdev, int id)
{
	int ret = VMM_EFAIL;
	vmm_guest_t *guest = vmm_manager_guest(id);
	if (guest) {
		if ((ret = vmm_manager_guest_resume(guest))) {
			vmm_cprintf(cdev, "%s: Failed to resume\n", 
					  guest->node->name);
		} else {
			vmm_cprintf(cdev, "%s: Resumed\n", 
					  guest->node->name);
		}
	} else {
		vmm_cprintf(cdev, "Failed to find guest\n");
	}
	return ret;
}

int cmd_guest_halt(vmm_chardev_t *cdev, int id)
{
	int ret = VMM_EFAIL;
	vmm_guest_t *guest = vmm_manager_guest(id);
	if (guest) {
		if ((ret = vmm_manager_guest_halt(guest))) {
			vmm_cprintf(cdev, "%s: Failed to halt\n", 
					  guest->node->name);
		} else {
			vmm_cprintf(cdev, "%s: Halted\n", 
					  guest->node->name);
		}
	} else {
		vmm_cprintf(cdev, "Failed to find guest\n");
	}
	return ret;
}

int cmd_guest_dumpreg(vmm_chardev_t *cdev, int id)
{
	int ret = VMM_EFAIL;
	vmm_guest_t *guest = vmm_manager_guest(id);
	if (guest) {
		if ((ret = vmm_manager_guest_dumpreg(guest))) {
			vmm_cprintf(cdev, "%s: Failed to dumpreg\n", 
					  guest->node->name);
		}
	} else {
		vmm_cprintf(cdev, "Failed to find guest\n");
	}
	return ret;
}

int cmd_guest_exec(vmm_chardev_t *cdev, int argc, char **argv)
{
	int id, count;
	u32 src_addr, dest_addr, size;
	int ret;
	if (argc == 2) {
		if (vmm_strcmp(argv[1], "help") == 0) {
			cmd_guest_usage(cdev);
			return VMM_OK;
		} else if (vmm_strcmp(argv[1], "list") == 0) {
			cmd_guest_list(cdev);
			return VMM_OK;
		}
	}
	if (argc < 3) {
		cmd_guest_usage(cdev);
		return VMM_EFAIL;
	}
	id = vmm_str2int(argv[2], 10);
	count = vmm_manager_guest_count();
	if (vmm_strcmp(argv[1], "reset") == 0) {
		if (id == -1) {
			for (id = 0; id < count; id++) {
				ret = cmd_guest_reset(cdev, id);
				if (ret) {
					return ret;
				}
			}
		} else {
			return cmd_guest_reset(cdev, id);
		}
	} else if (vmm_strcmp(argv[1], "kick") == 0) {
		if (id == -1) {
			for (id = 0; id < count; id++) {
				ret = cmd_guest_kick(cdev, id);
				if (ret) {
					return ret;
				}
			}
		} else {
			return cmd_guest_kick(cdev, id);
		}
	} else if (vmm_strcmp(argv[1], "pause") == 0) {
		if (id == -1) {
			for (id = 0; id < count; id++) {
				ret = cmd_guest_pause(cdev, id);
				if (ret) {
					return ret;
				}
			}
		} else {
			return cmd_guest_pause(cdev, id);
		}
	} else if (vmm_strcmp(argv[1], "resume") == 0) {
		if (id == -1) {
			for (id = 0; id < count; id++) {
				ret = cmd_guest_resume(cdev, id);
				if (ret) {
					return ret;
				}
			}
		} else {
			return cmd_guest_resume(cdev, id);
		}
	} else if (vmm_strcmp(argv[1], "halt") == 0) {
		if (id == -1) {
			for (id = 0; id < count; id++) {
				ret = cmd_guest_halt(cdev, id);
				if (ret) {
					return ret;
				}
			}
		} else {
			return cmd_guest_halt(cdev, id);
		}
	} else if (vmm_strcmp(argv[1], "dumpreg") == 0) {
		if (id == -1) {
			for (id = 0; id < count; id++) {
				ret = cmd_guest_dumpreg(cdev, id);
				if (ret) {
					return ret;
				}
			}
		} else {
			return cmd_guest_dumpreg(cdev, id);
		}
	} else if (vmm_strcmp(argv[1], "load") == 0) {
		if (id == -1) {
			vmm_cprintf(cdev, "Error: Cannot load images in " 
					  "all guests simultaneously.\n");
			return VMM_EFAIL;
		}

		if (argc < 6) {
			vmm_cprintf(cdev, "Error: Insufficient argument for "
					  "command load.\n");
			cmd_guest_usage(cdev);
			return VMM_EFAIL;
		}

		src_addr = vmm_str2uint(argv[3], 16);
		dest_addr = vmm_str2uint(argv[4], 16);
		size = vmm_str2uint(argv[5], 16);

		return cmd_guest_load(cdev, id, src_addr, dest_addr, size);
	} else {
		cmd_guest_usage(cdev);
		return VMM_EFAIL;
	}
	return VMM_OK;
}

static vmm_cmd_t cmd_guest = {
	.name = "guest",
	.desc = "control commands for guest",
	.usage = cmd_guest_usage,
	.exec = cmd_guest_exec,
};

static int cmd_guest_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_guest);
}

static void cmd_guest_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_guest);
}

VMM_DECLARE_MODULE(MODULE_VARID, 
			MODULE_NAME, 
			MODULE_AUTHOR, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
