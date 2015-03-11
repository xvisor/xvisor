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
#include <vio/vmm_keymaps.h>
#include <vio/vmm_vinput.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"Command vinput"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_vinput_init
#define	MODULE_EXIT			cmd_vinput_exit

static void cmd_vinput_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   vinput help\n");
	vmm_cprintf(cdev, "   vinput keyboards\n");
	vmm_cprintf(cdev, "   vinput keyboard_event <vkeyboard_name> "
			  "<keycode>\n");
	vmm_cprintf(cdev, "   vinput mouses\n");
	vmm_cprintf(cdev, "   vinput mouse_event <vmouse_name> "
			  "<dx> <dy> <dz> <left|right|middle|none>\n");
}

static int cmd_vinput_keyboards_iter(struct vmm_vkeyboard *vk, void *data)
{
	int ledstate;
	const char *num_lock, *caps_lock, *scroll_lock;
	struct vmm_chardev *cdev = data;

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
	vmm_cprintf(cdev, " %-45s %-10s %-10s %-10s\n",
		    vk->name, num_lock, caps_lock, scroll_lock);

	return VMM_OK;
}

static int cmd_vinput_keyboards(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_cprintf(cdev, " %-45s %-10s %-10s %-10s\n",
			  "Name", "NumLock", "CapsLock", "ScrollLock");
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_vkeyboard_iterate(NULL, cdev, cmd_vinput_keyboards_iter);
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	return VMM_OK;
}

static int cmd_vinput_keyboard_event(struct vmm_chardev *cdev,
				     const char *vkeyboard_name,
				     int keyc, char **keyv)
{
	int k;
	unsigned long keycode;
	struct vmm_vkeyboard *vk = vmm_vkeyboard_find(vkeyboard_name);

	if (!vk) {
		vmm_cprintf(cdev, "Error: virtual keyboard %s not found\n",
			    vkeyboard_name);
		return VMM_ENODEV;
	}

	/* Press the Keys (or Key Down) */
	for (k = 0; k < keyc; k++) {
		keycode = strtoul(keyv[k], NULL, 0);
		if (keycode & SCANCODE_GREY) {
			vmm_vkeyboard_event(vk, SCANCODE_EMUL0);
		}
		vmm_vkeyboard_event(vk, keycode & SCANCODE_KEYCODEMASK);
	}


	/* Release the Keys (or Key Up) */
	for (k = keyc - 1; 0 <= k; k--) {
		keycode = strtoul(keyv[k], NULL, 0);
		if (keycode & SCANCODE_GREY) {
			vmm_vkeyboard_event(vk, SCANCODE_EMUL0);
		}
		vmm_vkeyboard_event(vk, keycode | SCANCODE_UP);
	}

	return VMM_OK;
}

static int cmd_vinput_mouse_event(struct vmm_chardev *cdev,
				  const char *vmouse_name,
				  const char *dxstr,
				  const char *dystr,
				  const char *dzstr,
				  const char *button)
{
	int dx, dy, dz, buttons_state;
	struct vmm_vmouse *vm = vmm_vmouse_find(vmouse_name);

	if (!vm) {
		vmm_cprintf(cdev, "Error: virtual mouse %s not found\n",
			    vmouse_name);
		return VMM_ENODEV;
	}

	/* Determine mouse displacement */
	dx = atoi(dxstr);
	dy = atoi(dystr);
	dz = atoi(dzstr);

	/* Determine button state */
	buttons_state = 0;
	if (strcmp(button, "left") == 0) {
		buttons_state |= VMM_MOUSE_LBUTTON;
	} else if (strcmp(button, "middle") == 0) {
		buttons_state |= VMM_MOUSE_MBUTTON;
	} else if (strcmp(button, "right") == 0) {
		buttons_state |= VMM_MOUSE_RBUTTON;
	}

	/* Trigger mouse event */
	vmm_vmouse_event(vm, dx, dy, dz, buttons_state);

	return VMM_OK;
}

static int cmd_vinput_mouses_iter(struct vmm_vmouse *vm, void *data)
{
	u32 gw, gh, gr;
	const char *is_abs;
	struct vmm_chardev *cdev = data;

	if (vmm_vmouse_is_absolute(vm)) {
		is_abs = "Yes";
	} else {
		is_abs = "No";
	}
	gw = vmm_vmouse_get_graphics_width(vm);
	gh = vmm_vmouse_get_graphics_height(vm);
	gr = vmm_vmouse_get_graphics_rotation(vm);
	vmm_cprintf(cdev, " %-45s %-8s %-6d %-7d %-8d\n",
		    vm->name, is_abs, gw, gh, gr);

	return VMM_OK;
}

static int cmd_vinput_mouses(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_cprintf(cdev, " %-45s %-8s %-6s %-7s %-8s\n",
			  "Name", "Absolute", "Width", "Height", "Rotation");
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_vmouse_iterate(NULL, cdev, cmd_vinput_mouses_iter);
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");

	return VMM_OK;
}

static int cmd_vinput_exec(struct vmm_chardev *cdev, int argc, char **argv)
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
	} else if (argc > 2) {
		if ((argc > 3) && strcmp(argv[1], "keyboard_event") == 0) {
			return cmd_vinput_keyboard_event(cdev, argv[2],
							 argc - 3, &argv[3]);
		} else if ((argc > 6) && strcmp(argv[1], "mouse_event") == 0) {
			return cmd_vinput_mouse_event(cdev, argv[2],
					argv[3], argv[4], argv[5], argv[6]);
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
