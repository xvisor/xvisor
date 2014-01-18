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
 * @file cmd_vscreen.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of vscreen command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_modules.h>
#include <vmm_manager.h>
#include <vmm_cmdmgr.h>
#include <libs/stringlib.h>
#include <libs/vscreen.h>

#define MODULE_DESC			"Command vscreen"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_vscreen_init
#define	MODULE_EXIT			cmd_vscreen_exit

static void cmd_vscreen_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   vscreen help\n");
	vmm_cprintf(cdev, "   vscreen device_list <guest_name>\n");
	vmm_cprintf(cdev, "   vscreen hard_bind <guest_name> "
			  "[<fb_name>] [<vdisplay_name>] "
			  "[<vkeyboard_name>] [<vmouse_name>]\n");
	vmm_cprintf(cdev, "   vscreen soft_bind <guest_name> "
			  "[<refresh_rate>] [<fb_name>] [<vdisplay_name>] "
			  "[<vkeyboard_name>] [<vmouse_name>]\n");
}

static int cmd_vscreen_device_list(struct vmm_chardev *cdev,
				   const char *guest_name)
{
	bool found;
	int num, count;
	struct vmm_guest *guest = NULL;
	struct vmm_vdisplay *vdis = NULL;
	struct vmm_vkeyboard *vkbd = NULL;
	struct vmm_vmouse *vmou = NULL;

	guest = vmm_manager_guest_find(guest_name);
	if (!guest) {
		vmm_cprintf(cdev, "Failed to find guest %s\n",
			    guest_name);
		return VMM_ENOTAVAIL;
	}

	found = FALSE;
	count = vmm_vdisplay_count();
	vmm_cprintf(cdev, "Virtual Display List\n");
	for (num = 0; num < count; num++) {
		vdis = vmm_vdisplay_get(num);
		if (!strncmp(vdis->name, guest->name,
			     strlen(guest->name))) {
			if (!found) {
				vmm_cprintf(cdev, " (default)");
			} else {
				vmm_cprintf(cdev, "          ");
			}
			vmm_cprintf(cdev, " %s\n", vdis->name);
			found = TRUE;
		}
	}
	vmm_cprintf(cdev, "\n");

	found = FALSE;
	count = vmm_vkeyboard_count();
	vmm_cprintf(cdev, "Virtual Keyboard List\n");
	for (num = 0; num < count; num++) {
		vkbd = vmm_vkeyboard_get(num);
		if (!strncmp(vkbd->name, guest->name,
			     strlen(guest->name))) {
			if (!found) {
				vmm_cprintf(cdev, " (default)");
			} else {
				vmm_cprintf(cdev, "          ");
			}
			vmm_cprintf(cdev, " %s\n", vkbd->name);
			found = TRUE;
		}
	}
	vmm_cprintf(cdev, "\n");

	found = FALSE;
	count = vmm_vmouse_count();
	vmm_cprintf(cdev, "Virtual Mouse List\n");
	for (num = 0; num < count; num++) {
		vmou = vmm_vmouse_get(num);
		if (!strncmp(vmou->name, guest->name,
			     strlen(guest->name))) {
			if (!found) {
				vmm_cprintf(cdev, " (default)");
			} else {
				vmm_cprintf(cdev, "          ");
			}
			vmm_cprintf(cdev, " %s\n", vmou->name);
			found = TRUE;
		}
	}
	vmm_cprintf(cdev, "\n");

	return VMM_OK;
}

static int cmd_vscreen_bind(struct vmm_chardev *cdev,
			    bool is_hard,
			    const char *guest_name,
			    const char *refresh_rate,
			    const char *fb_name,
			    const char *vdisplay_name,
			    const char *vkeyboard_name,
			    const char *vmouse_name)
{
	bool found;
	int rc, num, count;
	u32 rate, ekey[3];
	struct fb_info *info;
	struct vmm_guest *guest = NULL;
	struct vmm_vdisplay *vdis = NULL;
	struct vmm_vkeyboard *vkbd = NULL;
	struct vmm_vmouse *vmou = NULL;

	guest = vmm_manager_guest_find(guest_name);
	if (!guest) {
		vmm_cprintf(cdev, "Failed to find guest %s\n",
			    guest_name);
		return VMM_ENOTAVAIL;
	}

	if (refresh_rate) {
		rate = (u32)strtoul(refresh_rate, NULL, 10);
	} else {
		rate = VSCREEN_REFRESH_RATE_GOOD;
	}
	if ((rate < VSCREEN_REFRESH_RATE_MIN) ||
	    (VSCREEN_REFRESH_RATE_MAX < rate)) {
		vmm_cprintf(cdev, "Invalid refresh rate %d\n", rate);
		vmm_cprintf(cdev, "Refresh rate should be "
			    "between %d and %d\n",
			    VSCREEN_REFRESH_RATE_MIN,
			    VSCREEN_REFRESH_RATE_MAX);
		return VMM_EINVALID;
	}

	if (fb_name) {
		info = fb_find(fb_name);
	} else {
		info = fb_get(0);
	}
	if (!info) {
		vmm_cprintf(cdev, "Failed to find fb_info %s\n",
			    fb_name);
		return VMM_ENOTAVAIL;
	}

	if (vdisplay_name) {
		vdis = vmm_vdisplay_find(vdisplay_name);
	} else {
		found = FALSE;
		count = vmm_vdisplay_count();
		for (num = 0; num < count; num++) {
			vdis = vmm_vdisplay_get(num);
			if (!strncmp(vdis->name, guest->name,
				     strlen(guest->name))) {
				found = TRUE;
				break;
			}
		}
		if (!found) {
			vdis = NULL;
		}
	}
	if (!vdis) {
		vmm_cprintf(cdev, "Failed to find virtual display%s %s\n",
			    (vdisplay_name) ? "" : " for guest",
			    (vdisplay_name) ? vdisplay_name : guest->name);
		return VMM_ENOTAVAIL;
	}

	if (vkeyboard_name) {
		vkbd = vmm_vkeyboard_find(vkeyboard_name);
	} else {
		found = FALSE;
		count = vmm_vkeyboard_count();
		for (num = 0; num < count; num++) {
			vkbd = vmm_vkeyboard_get(num);
			if (!strncmp(vkbd->name, guest->name,
				     strlen(guest->name))) {
				found = TRUE;
				break;
			}
		}
		if (!found) {
			vkbd = NULL;
		}
	}
	if (!vkbd && vkeyboard_name) {
		vmm_cprintf(cdev, "Failed to find virtual keyboard %s\n",
			    vkeyboard_name);
		return VMM_ENOTAVAIL;
	}

	if (vmouse_name) {
		vmou = vmm_vmouse_find(vmouse_name);
	} else {
		found = FALSE;
		count = vmm_vmouse_count();
		for (num = 0; num < count; num++) {
			vmou = vmm_vmouse_get(num);
			if (!strncmp(vmou->name, guest->name,
				     strlen(guest->name))) {
				found = TRUE;
				break;
			}
		}
		if (!found) {
			vmou = NULL;
		}
	}
	if (!vmou && vmouse_name) {
		vmm_cprintf(cdev, "Failed to find virtual mouse %s\n",
			    vmouse_name);
		return VMM_ENOTAVAIL;
	}

	ekey[0] = KEY_ESC;
	ekey[1] = KEY_X;
	ekey[2] = KEY_Q;

	vmm_cprintf(cdev, "Guest name      : %s\n", guest->name);
	if (!is_hard) {
		vmm_cprintf(cdev, "Refresh rate    : %d per-second\n", rate);
	}
	vmm_cprintf(cdev, "Escape Keys     : ESC+X+Q\n");
	vmm_cprintf(cdev, "Frame buffer    : %s\n", info->name);
	vmm_cprintf(cdev, "Virtual display : %s\n", vdis->name);
	vmm_cprintf(cdev, "Virtual keyboard: %s\n",
		    (vkbd) ? vkbd->name : "---");
	vmm_cprintf(cdev, "Virtual mouse   : %s\n",
		    (vmou) ? vmou->name : "---");

	if (is_hard) {
		rc = vscreen_hard_bind(ekey[0], ekey[1], ekey[2],
					info, vdis, vkbd, vmou);
	} else {
		rc = vscreen_soft_bind(rate, ekey[0], ekey[1], ekey[2],
					info, vdis, vkbd, vmou);
	}

	return rc;
}

static int cmd_vscreen_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	if (argc < 2) {
		goto cmd_vscreen_fail;
	}
	if (strcmp(argv[1], "help") == 0) {
		cmd_vscreen_usage(cdev);
		return VMM_OK;
	} else if ((strcmp(argv[1], "device_list") == 0) && (argc == 3)) {
		return cmd_vscreen_device_list(cdev, argv[2]);
	} else if ((strcmp(argv[1], "soft_bind") == 0) && (argc > 2)) {
		return cmd_vscreen_bind(cdev, FALSE, argv[2],
					(argc > 3) ? argv[3] : NULL,
					(argc > 4) ? argv[4] : NULL,
					(argc > 5) ? argv[5] : NULL,
					(argc > 6) ? argv[6] : NULL,
					(argc > 7) ? argv[7] : NULL);
	} else if ((strcmp(argv[1], "hard_bind") == 0) && (argc > 2)) {
		return cmd_vscreen_bind(cdev, TRUE, argv[2], NULL,
					(argc > 3) ? argv[3] : NULL,
					(argc > 4) ? argv[4] : NULL,
					(argc > 5) ? argv[5] : NULL,
					(argc > 6) ? argv[6] : NULL);
	}
cmd_vscreen_fail:
	cmd_vscreen_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_vscreen = {
	.name = "vscreen",
	.desc = "virtual screen commands",
	.usage = cmd_vscreen_usage,
	.exec = cmd_vscreen_exec,
};

static int __init cmd_vscreen_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_vscreen);
}

static void __exit cmd_vscreen_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_vscreen);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
