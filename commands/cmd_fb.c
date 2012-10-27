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
 * @file cmd_fb.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of fb command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <fb/vmm_fb.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"Command fb"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_fb_init
#define	MODULE_EXIT			cmd_fb_exit

void cmd_fb_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   fb help\n");
	vmm_cprintf(cdev, "   fb list\n");
	vmm_cprintf(cdev, "   fb info <fb_name>\n");
}

void cmd_fb_list(struct vmm_chardev *cdev)
{
	int num, count;
	char path[1024];
	struct vmm_fb_info *info;
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_cprintf(cdev, " %-16s %-20s %-40s\n", 
			  "Name", "ID", "Device Path");
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	count = vmm_fb_count();
	for (num = 0; num < count; num++) {
		info = vmm_fb_get(num);
		vmm_devtree_getpath(path, info->dev->node);
		vmm_cprintf(cdev, " %-16s %-20s %-40s\n", 
				  info->dev->node->name, info->fix.id, path);
	}
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
}

int cmd_fb_info(struct vmm_chardev *cdev, const char *fb_name)
{
	struct vmm_fb_info *info;
	const char *str;

	info = vmm_fb_find(fb_name);
	if (!info) {
		vmm_cprintf(cdev, "Error: Invalid FB %s\n", fb_name);
		return VMM_EFAIL;
	}

	vmm_cprintf(cdev, "Name   : %s\n", info->dev->node->name);
	vmm_cprintf(cdev, "ID     : %s\n", info->fix.id);

	switch (info->fix.type) {
	case FB_TYPE_PACKED_PIXELS:
		str = "Packed Pixels";
		break;
	case FB_TYPE_PLANES:
		str = "Non interleaved planes";
		break;
	case FB_TYPE_INTERLEAVED_PLANES:
		str = "Interleaved planes";
		break;
	case FB_TYPE_TEXT:
		str = "Text/attributes";
		break;
	case FB_TYPE_VGA_PLANES:
		str = "EGA/VGA planes";
		break;
	default:
		str = "Unknown";
		break;
	};
	vmm_cprintf(cdev, "Type   : %s\n", str);

	switch (info->fix.visual) {
	case FB_VISUAL_MONO01:
		str = "Monochrome 1=Black 0=White";
		break;
	case FB_VISUAL_MONO10:
		str = "Monochrome 0=Black 1=White";
		break;
	case FB_VISUAL_TRUECOLOR:
		str = "True color";
		break;
	case FB_VISUAL_PSEUDOCOLOR:
		str = "Pseudo color";
		break;
	case FB_VISUAL_DIRECTCOLOR:
		str = "Direct color";
		break;
	case FB_VISUAL_STATIC_PSEUDOCOLOR:
		str = "Pseudo color readonly";
		break;
	default:
		str = "Unknown";
		break;
	};
	vmm_cprintf(cdev, "Visual : %s\n", str);

	vmm_cprintf(cdev, "Xres   : %d\n", info->var.xres);
	vmm_cprintf(cdev, "Yres   : %d\n", info->var.yres);
	vmm_cprintf(cdev, "BPP    : %d\n", info->var.bits_per_pixel);

	return VMM_OK;
}

int cmd_fb_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	if (argc == 2) {
		if (strcmp(argv[1], "help") == 0) {
			cmd_fb_usage(cdev);
			return VMM_OK;
		} else if (strcmp(argv[1], "list") == 0) {
			cmd_fb_list(cdev);
			return VMM_OK;
		}
	}
	if (argc > 2) {
		if (strcmp(argv[1], "info") == 0) {
			return cmd_fb_info(cdev, argv[2]);
		}
	}
	cmd_fb_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_fb = {
	.name = "fb",
	.desc = "frame buffer commands",
	.usage = cmd_fb_usage,
	.exec = cmd_fb_exec,
};

static int __init cmd_fb_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_fb);
}

static void __exit cmd_fb_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_fb);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
