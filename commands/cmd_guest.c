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
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of guest command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_manager.h>
#include <vmm_host_aspace.h>
#include <vmm_guest_aspace.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"Command guest"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_guest_init
#define	MODULE_EXIT			cmd_guest_exit

void cmd_guest_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   guest help\n");
	vmm_cprintf(cdev, "   guest list\n");
	vmm_cprintf(cdev, "   guest create  <guest_name>\n");
	vmm_cprintf(cdev, "   guest destroy <guest_id>\n");
	vmm_cprintf(cdev, "   guest reset   <guest_id>\n");
	vmm_cprintf(cdev, "   guest kick    <guest_id>\n");
	vmm_cprintf(cdev, "   guest pause   <guest_id>\n");
	vmm_cprintf(cdev, "   guest resume  <guest_id>\n");
	vmm_cprintf(cdev, "   guest halt    <guest_id>\n");
	vmm_cprintf(cdev, "   guest dumpreg <guest_id>\n");
	vmm_cprintf(cdev, "   guest dumpmem <guest_id> <gphys_addr> "
			  "[mem_sz]\n");
	vmm_cprintf(cdev, "Note:\n");
	vmm_cprintf(cdev, "   <guest_id> = if -1 implies all guests "
						"else guest id.\n");
	vmm_cprintf(cdev, "   <guest_name> = node name under /guests "
							"device tree node\n");
}

void cmd_guest_list(struct vmm_chardev *cdev)
{
	int id, count;
	char path[256];
	struct vmm_guest *guest;
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_cprintf(cdev, " %-6s %-18s %-53s\n", 
			 "ID ", "Name", "Device Path");
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	count = vmm_manager_max_guest_count();
	for (id = 0; id < count; id++) {
		if (!(guest = vmm_manager_guest(id))) {
			continue;
		}
		vmm_devtree_getpath(path, guest->node);
		vmm_cprintf(cdev, " %-6d %-18s %-53s\n", 
				  id, guest->node->name, path);
	}
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
}

int cmd_guest_create(struct vmm_chardev *cdev, char *name)
{
	struct vmm_guest * guest = NULL;
	struct vmm_devtree_node * node = NULL;

	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
					VMM_DEVTREE_GUESTINFO_NODE_NAME);

	node = vmm_devtree_getchild(node, name);
	if (!node) {
		vmm_cprintf(cdev, "Error: failed to find %s node under %s\n",
				  name, VMM_DEVTREE_PATH_SEPARATOR_STRING
					VMM_DEVTREE_GUESTINFO_NODE_NAME);
		return VMM_EFAIL;
	}

	guest = vmm_manager_guest_create(node);
	if (!guest) {
		vmm_cprintf(cdev, "Error: failed to create %s\n", name);
		return VMM_EFAIL;
	}

	vmm_cprintf(cdev, "Created %s successfully\n", name);

	return VMM_OK;
}

int cmd_guest_destroy(struct vmm_chardev *cdev, int id)
{
	int ret = VMM_EFAIL;
	char name[64];
	struct vmm_guest *guest = vmm_manager_guest(id);
	if (guest) {
		strlcpy(name, guest->node->name, sizeof(name));
		if ((ret = vmm_manager_guest_destroy(guest))) {
			vmm_cprintf(cdev, "%s: Failed to destroy\n", name);
		} else {
			vmm_cprintf(cdev, "%s: Destroyed\n", name);
		}
	} else {
		vmm_cprintf(cdev, "Failed to find guest\n");
	}
	return ret;
}

int cmd_guest_reset(struct vmm_chardev *cdev, int id)
{
	int ret = VMM_EFAIL;
	struct vmm_guest *guest = vmm_manager_guest(id);
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

int cmd_guest_kick(struct vmm_chardev *cdev, int id)
{
	int ret = VMM_EFAIL;
	struct vmm_guest *guest = vmm_manager_guest(id);
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

int cmd_guest_pause(struct vmm_chardev *cdev, int id)
{
	int ret = VMM_EFAIL;
	struct vmm_guest *guest = vmm_manager_guest(id);
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

int cmd_guest_resume(struct vmm_chardev *cdev, int id)
{
	int ret = VMM_EFAIL;
	struct vmm_guest *guest = vmm_manager_guest(id);
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

int cmd_guest_halt(struct vmm_chardev *cdev, int id)
{
	int ret = VMM_EFAIL;
	struct vmm_guest *guest = vmm_manager_guest(id);
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

int cmd_guest_dumpreg(struct vmm_chardev *cdev, int id)
{
	int ret = VMM_EFAIL;
	struct vmm_guest *guest = vmm_manager_guest(id);
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

int cmd_guest_dumpmem(struct vmm_chardev *cdev, int id,
		      physical_addr_t gphys_addr, u32 len)
{
#define BYTES_PER_LINE 16
	u8 buf[BYTES_PER_LINE];
	u32 total_loaded = 0, loaded = 0, *mem;
	struct vmm_guest *guest;

	len = (len + (BYTES_PER_LINE - 1)) & ~(BYTES_PER_LINE - 1);

	guest = vmm_manager_guest(id);
	if (guest) {
		vmm_cprintf(cdev, "Guest %d physical memory ", id);
		if (sizeof(u64) == sizeof(physical_addr_t)) {
			vmm_cprintf(cdev, "0x%016llx - 0x%016llx:\n", 
				(u64)gphys_addr, (u64)(gphys_addr + len));
		} else {
			vmm_cprintf(cdev, "0x%08x - 0x%08x:\n", 
				(u32)gphys_addr, (u32)(gphys_addr + len));
		}
		while (total_loaded < len) {
			loaded = vmm_guest_memory_read(guest, gphys_addr,
                                                       buf, BYTES_PER_LINE);
			if (loaded != BYTES_PER_LINE)
				break;

			mem = (u32 *)buf;
			if (sizeof(u64) == sizeof(physical_addr_t)) {
				vmm_cprintf(cdev, "%016llx:", (u64)gphys_addr);
			} else {
				vmm_cprintf(cdev, "%08x:", gphys_addr);
			}
			vmm_cprintf(cdev, " %08x %08x %08x %08x\n"
					  , mem[0], mem[1], mem[2], mem[3]);

			gphys_addr += BYTES_PER_LINE;
			total_loaded += BYTES_PER_LINE;
		}
#undef BYTES_PER_LINE
		if (total_loaded == len)
			return VMM_OK;
	}
	return VMM_EFAIL;
}


int cmd_guest_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	int id, count;
	physical_addr_t src_addr;
	u32 size;
	int ret;
	if (argc == 2) {
		if (strcmp(argv[1], "help") == 0) {
			cmd_guest_usage(cdev);
			return VMM_OK;
		} else if (strcmp(argv[1], "list") == 0) {
			cmd_guest_list(cdev);
			return VMM_OK;
		}
	}
	if (argc < 3) {
		cmd_guest_usage(cdev);
		return VMM_EFAIL;
	}
	count = vmm_manager_max_guest_count();
	if (strcmp(argv[1], "create") == 0) {
		return cmd_guest_create(cdev, argv[2]);
	} else if (strcmp(argv[1], "destroy") == 0) {
		id = str2int(argv[2], 10);
		if (id == -1) {
			for (id = 0; id < count; id++) {
				if (!vmm_manager_guest(id)) {
					continue;
				}
				if ((ret = cmd_guest_destroy(cdev, id))) {
					return ret;
				}
			}
		} else {
			return cmd_guest_destroy(cdev, id);
		}
	} else if (strcmp(argv[1], "reset") == 0) {
		id = str2int(argv[2], 10);
		if (id == -1) {
			for (id = 0; id < count; id++) {
				if (!vmm_manager_guest(id)) {
					continue;
				}
				if ((ret = cmd_guest_reset(cdev, id))) {
					return ret;
				}
			}
		} else {
			return cmd_guest_reset(cdev, id);
		}
	} else if (strcmp(argv[1], "kick") == 0) {
		id = str2int(argv[2], 10);
		if (id == -1) {
			for (id = 0; id < count; id++) {
				if (!vmm_manager_guest(id)) {
					continue;
				}
				if ((ret = cmd_guest_kick(cdev, id))) {
					return ret;
				}
			}
		} else {
			return cmd_guest_kick(cdev, id);
		}
	} else if (strcmp(argv[1], "pause") == 0) {
		id = str2int(argv[2], 10);
		if (id == -1) {
			for (id = 0; id < count; id++) {
				if (!vmm_manager_guest(id)) {
					continue;
				}
				if ((ret = cmd_guest_pause(cdev, id))) {
					return ret;
				}
			}
		} else {
			return cmd_guest_pause(cdev, id);
		}
	} else if (strcmp(argv[1], "resume") == 0) {
		id = str2int(argv[2], 10);
		if (id == -1) {
			for (id = 0; id < count; id++) {
				if (!vmm_manager_guest(id)) {
					continue;
				}
				if ((ret = cmd_guest_resume(cdev, id))) {
					return ret;
				}
			}
		} else {
			return cmd_guest_resume(cdev, id);
		}
	} else if (strcmp(argv[1], "halt") == 0) {
		id = str2int(argv[2], 10);
		if (id == -1) {
			for (id = 0; id < count; id++) {
				if (!vmm_manager_guest(id)) {
					continue;
				}
				if ((ret = cmd_guest_halt(cdev, id))) {
					return ret;
				}
			}
		} else {
			return cmd_guest_halt(cdev, id);
		}
	} else if (strcmp(argv[1], "dumpreg") == 0) {
		id = str2int(argv[2], 10);
		if (id == -1) {
			for (id = 0; id < count; id++) {
				if (!vmm_manager_guest(id)) {
					continue;
				}
				if ((ret = cmd_guest_dumpreg(cdev, id))) {
					return ret;
				}
			}
		} else {
			return cmd_guest_dumpreg(cdev, id);
		}
	} else if (strcmp(argv[1], "dumpmem") == 0) {
		id = str2int(argv[2], 10);
		if (id == -1) {
			vmm_cprintf(cdev, "Error: Cannot dump memory in "
					  "all guests simultaneously.\n");
			return VMM_EFAIL;
		}
		if (argc < 4) {
			vmm_cprintf(cdev, "Error: Insufficient argument for "
					  "command dumpmem.\n");
			cmd_guest_usage(cdev);
			return VMM_EFAIL;
		}
		src_addr = (physical_addr_t)str2ulonglong(argv[3], 10);
		if (argc > 4)
			size = (physical_size_t)str2ulonglong(argv[4], 10);
		else
			size = 64;
		return cmd_guest_dumpmem(cdev, id, src_addr, size);
	} else {
		cmd_guest_usage(cdev);
		return VMM_EFAIL;
	}
	return VMM_OK;
}

static struct vmm_cmd cmd_guest = {
	.name = "guest",
	.desc = "control commands for guest",
	.usage = cmd_guest_usage,
	.exec = cmd_guest_exec,
};

static int __init cmd_guest_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_guest);
}

static void __exit cmd_guest_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_guest);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
