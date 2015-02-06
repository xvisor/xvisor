/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation
 * 51 Franklin Street, Fifth Floor
 * Boston, MA  02110-1301, USA.
 *
 * @file cmd_backlight.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Backlight commands
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_cmdmgr.h>
#include <vmm_modules.h>
#include <libs/list.h>

#include <linux/backlight.h>

#define MODULE_DESC			"Backlight command"
#define MODULE_AUTHOR			"Jimmy Durand Wesolowski"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_backlight_init
#define	MODULE_EXIT			cmd_backlight_exit

static void cmd_backlight_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   backlight list - Display backlight device list\n");
	vmm_cprintf(cdev, "   backlight brightness <name> [value] - Get or set the "
		    "backlight 'name' brightness\n");
}

static int cmd_backlight_help(struct vmm_chardev *cdev,
			 int __unused argc,
			 char __unused **argv)
{
	cmd_backlight_usage(cdev);
	return VMM_OK;
}

static int cmd_backlight_list(struct vmm_chardev *cdev,
			int __unused argc,
			char __unused **argv)
{
	struct backlight_device *bd;

	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_cprintf(cdev, "%-16s %-12s %-16s %-6s %-9s %-9s %-6s\n",
		    "Name", "Brightness", "Max brightness", "Power",
		    "Blanking", "Type", "State");
	list_for_each_entry(bd, &backlight_dev_list, entry) {
		vmm_cprintf(cdev, "%-16s %-12d %-16d %-6d %-9d %-9d %-6d\n",
			    bd->dev.name, bd->props.brightness,
			    bd->props.max_brightness, bd->props.power,
			    bd->props.fb_blank, bd->props.type,
			    bd->props.state);
	}
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");

	return 0;
}

static int cmd_backlight_brightness(struct vmm_chardev *cdev,
			      int __unused argc,
			      char __unused **argv)
{
	long brightness = 0;
	struct backlight_device *bd_elt;
	struct backlight_device *bd = NULL;

	if (argc <= 2) {
		cmd_backlight_usage(cdev);
		return VMM_EFAIL;
	}
	list_for_each_entry(bd_elt, &backlight_dev_list, entry) {
		if (!strcmp(bd_elt->dev.name, argv[2])) {
			bd = bd_elt;
			break;
		}
	}
	if (NULL == bd) {
		cmd_backlight_usage(cdev);
		return VMM_EFAIL;
	}

	if (argc == 3) {
		vmm_cprintf(cdev, "%s brightness: %d (max: %d)\n",
			    bd->dev.name, bd->props.brightness,
			    bd->ops->get_brightness(bd));
		return VMM_OK;
	}

	if (argc < 4)
		return VMM_OK;

	brightness = strtol(argv[3], NULL, 10);
	if (brightness > bd->props.max_brightness) {
		vmm_cprintf(cdev, "Warning: Setting \"%s\" to maximum "
			    "value (%d)\n", bd->dev.name,
			    bd->props.max_brightness);
		brightness = bd->props.max_brightness;
	}
	if (brightness < 0) {
		vmm_cprintf(cdev, "Warning: Setting \"%s\" off\n",
			    bd->dev.name);
		brightness = 0;
	}
	bd->props.brightness = brightness;
	bd->ops->update_status(bd);

	return VMM_OK;
}

static const struct {
	char *name;
	int (*function) (struct vmm_chardev *, int, char **);
} const command[] = {
	{"help", cmd_backlight_help},
	{"list", cmd_backlight_list},
	{"brightness", cmd_backlight_brightness},
	{NULL, NULL},
};

static int cmd_backlight_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	int index = 0;

	while (command[index].name) {
		if (strcmp(argv[1], command[index].name) == 0) {
			return command[index].function(cdev, argc, argv);
		}
		index++;
	}

	cmd_backlight_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_backlight = {
	.name = "backlight",
	.desc = "control commands for backlight devices",
	.usage = cmd_backlight_usage,
	.exec = cmd_backlight_exec,
};

static int __init cmd_backlight_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_backlight);
}

static void __exit cmd_backlight_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_backlight);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
