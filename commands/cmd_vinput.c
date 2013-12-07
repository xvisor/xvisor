/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file cmd_vinput.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of vinput command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <vio/vmm_vinput.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"Command vinput"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_vinput_init
#define	MODULE_EXIT			cmd_vinput_exit

void cmd_vinput_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   vinput help\n");
	vmm_cprintf(cdev, "   vinput keyboards\n");
	vmm_cprintf(cdev, "   vinput mouses\n");
}

int cmd_vinput_keyboards(struct vmm_chardev *cdev)
{
	int num, count, ledstate;
	const char *num_lock, *caps_lock, *scroll_lock;
	struct vmm_vkeyboard *vk;

	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_cprintf(cdev, " %-10s %-34s %-10s %-10s %-10s\n", 
			  "Num", "Name", "NumLock", "CapsLock", "ScrollLock");
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");

	count = vmm_vkeyboard_count();
	for (num = 0; num < count; num++) {
		vk = vmm_vkeyboard_get(num);
		ledstate = vmm_vkeyboard_get_ledstate(vk);
		if (ledstate & VMM_NUM_LOCK_LED) {
			num_lock = "ON";
		} else {
			num_lock = "OFF";
		}
		if (ledstate & VMM_CAPS_LOCK_LED) {
			caps_lock = "ON";
		} else {
			caps_lock = "OFF";
		}
		if (ledstate & VMM_SCROLL_LOCK_LED) {
			scroll_lock = "ON";
		} else {
			scroll_lock = "OFF";
		}
		vmm_cprintf(cdev, " %-10d %-34s %-10s %-10s %-10s\n",
			    num, vk->name, num_lock, caps_lock, scroll_lock);
	}

	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");

	return VMM_OK;
}

int cmd_vinput_mouses(struct vmm_chardev *cdev)
{
	u32 gw, gh, gr;
	int num, count;
	const char *is_abs;
	struct vmm_vmouse *vm;

	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_cprintf(cdev, " %-10s %-34s %-8s %-6s %-7s %-8s\n", 
			  "Num", "Name", "Absolute", "Width", "Height", "Rotation");
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");

	count = vmm_vmouse_count();
	for (num = 0; num < count; num++) {
		vm = vmm_vmouse_get(num);
		if (vmm_vmouse_is_absolute(vm)) {
			is_abs = "Yes";
		} else {
			is_abs = "No";
		}
		gw = vmm_vmouse_get_graphics_width(vm);
		gh = vmm_vmouse_get_graphics_height(vm);
		gr = vmm_vmouse_get_graphics_rotation(vm);
		vmm_cprintf(cdev, " %-10d %-34s %-8s %-6d %-7d %-8d\n",
			    num, vm->name, is_abs, gw, gh, gr);
	}

	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");

	return VMM_OK;
}

int cmd_vinput_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	if (argc == 2) {
		if (strcmp(argv[1], "help") == 0) {
			cmd_vinput_usage(cdev);
			return VMM_OK;
		} else if (strcmp(argv[1], "keyboards") == 0) {
			return cmd_vinput_keyboards(cdev);
		} else if (strcmp(argv[1], "mouses") == 0) {
			return cmd_vinput_mouses(cdev);
		}
	}
	cmd_vinput_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_vinput = {
	.name = "vinput",
	.desc = "virtual input device commands",
	.usage = cmd_vinput_usage,
	.exec = cmd_vinput_exec,
};

static int __init cmd_vinput_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_vinput);
}

static void __exit cmd_vinput_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_vinput);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
