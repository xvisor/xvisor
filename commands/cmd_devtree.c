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
 * @file cmd_devtree.c
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of devtree command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_devtree.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>

#define MODULE_VARID			cmd_devtree_module
#define MODULE_NAME			"Command devtree"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_devtree_init
#define	MODULE_EXIT			cmd_devtree_exit

#define VMM_DEVTREE_MAX_PATH_LEN		256

static char dtree_curpath[VMM_DEVTREE_MAX_PATH_LEN];

void cmd_devtree_usage(vmm_chardev_t *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   devtree help\n");
	vmm_cprintf(cdev, "   devtree curpath\n");
	vmm_cprintf(cdev, "   devtree chpath <path>\n");
	vmm_cprintf(cdev, "   devtree attrib [<path>]\n");
	vmm_cprintf(cdev, "   devtree print  [<path>]\n");
}

void cmd_devtree_print_attributes(vmm_chardev_t *cdev, 
				  vmm_devtree_node_t * node, int indent)
{
	int i;
	struct dlist *lentry;
	vmm_devtree_attr_t *attr;

	if (!node)
		return;

	list_for_each(lentry, &node->attr_list) {
		attr = list_entry(lentry, vmm_devtree_attr_t, head);
		for (i = 0; i < indent; i++)
			vmm_cprintf(cdev, "\t");
		if (attr->value[attr->len - 1] == '\0' && 4 < attr->len) {
			vmm_cprintf(cdev, "\t%s = \"%s\";\n", 
						attr->name, attr->value);
		} else {
			vmm_cprintf(cdev, "\t%s = 0x%x;\n", attr->name,
				   *((u32 *) attr->value));
		}
	}
}

void cmd_devtree_print_node(vmm_chardev_t *cdev, 
			    vmm_devtree_node_t * node, int indent)
{
	int i;
	struct dlist *lentry;
	vmm_devtree_node_t *child;

	for (i = 0; i < indent; i++)
		vmm_cprintf(cdev, "\t");
	if (node->name[0] == '\0' && indent == 0) {
		vmm_cprintf(cdev, "%c", VMM_DEVTREE_PATH_SEPRATOR);
	} else {
		vmm_cprintf(cdev, "%s", node->name);
	}

	switch (node->type) {
	case VMM_DEVTREE_NODETYPE_UNKNOWN:
		break;
	case VMM_DEVTREE_NODETYPE_DEVICE:
		vmm_cprintf(cdev, " [device]");
		break;
	case VMM_DEVTREE_NODETYPE_EDEVICE:
		vmm_cprintf(cdev, " [edevice]");
		break;
	default:
		break;
	};

	if (!list_empty(&node->child_list)) {
		vmm_cprintf(cdev, " {\n");
	}

	list_for_each(lentry, &node->child_list) {
		child = list_entry(lentry, vmm_devtree_node_t, head);
		cmd_devtree_print_node(cdev, child, indent + 1);
	}

	if (!list_empty(&node->child_list)) {
		for (i = 0; i < indent; i++)
			vmm_cprintf(cdev, "\t");

		vmm_cprintf(cdev, "}");
	}
	vmm_cprintf(cdev, ";\n");
}

int cmd_devtree_curpath(vmm_chardev_t *cdev)
{
	vmm_cprintf(cdev, "%s\r\n", dtree_curpath);
	return VMM_OK;
}

int cmd_devtree_chpath(vmm_chardev_t *cdev, char *path)
{
	vmm_devtree_node_t *node;

	if (*path == VMM_DEVTREE_PATH_SEPRATOR) {
		node = vmm_devtree_getnode(path);
	} else {
		node =
		    vmm_devtree_getchildnode(vmm_devtree_getnode(dtree_curpath),
					     path);
	}

	if (node) {
		vmm_devtree_getpath(dtree_curpath, node);
		vmm_cprintf(cdev, "New path: %s\n", dtree_curpath);
	} else {
		vmm_cprintf(cdev, "Invalid path: %s\n", path);
	}

	return VMM_OK;
}

int cmd_devtree_attrib(vmm_chardev_t *cdev, char *path)
{
	vmm_devtree_node_t *node = vmm_devtree_getnode(path);

	if (!node) {
		vmm_cprintf(cdev, "Failed to print attributes\n");
		return VMM_EFAIL;
	}

	cmd_devtree_print_attributes(cdev, node, 0);

	return VMM_OK;
}

int cmd_devtree_print(vmm_chardev_t *cdev, char *path)
{
	vmm_devtree_node_t *node = vmm_devtree_getnode(path);

	if (!node) {
		vmm_cprintf(cdev, "Failed to print device tree\n");
		return VMM_EFAIL;
	}

	cmd_devtree_print_node(cdev, node, 0);

	return VMM_OK;
}

int cmd_devtree_exec(vmm_chardev_t *cdev, int argc, char **argv)
{
	if (argc < 2) {
		cmd_devtree_usage(cdev);
		return VMM_EFAIL;
	}
	if (argc == 2) {
		if (vmm_strcmp(argv[1], "help") == 0) {
			cmd_devtree_usage(cdev);
			return VMM_OK;
		}
	}
	if (vmm_strcmp(argv[1], "curpath") == 0) {
		return cmd_devtree_curpath(cdev);
	} else if (vmm_strcmp(argv[1], "chpath") == 0) {
		if (argc < 3) {
			cmd_devtree_usage(cdev);
			return VMM_EFAIL;
		}
		return cmd_devtree_chpath(cdev, argv[2]);
	} else if (vmm_strcmp(argv[1], "attrib") == 0) {
		if (argc < 3) {
			return cmd_devtree_attrib(cdev, dtree_curpath);
		} else {
			return cmd_devtree_attrib(cdev, argv[2]);
		}
	} else if (vmm_strcmp(argv[1], "print") == 0) {
		if (argc < 3) {
			return cmd_devtree_print(cdev, dtree_curpath);
		} else {
			return cmd_devtree_print(cdev, argv[2]);
		}
	} else {
		cmd_devtree_usage(cdev);
		return VMM_EFAIL;
	}
	return VMM_OK;
}

static vmm_cmd_t cmd_devtree = {
	.name = "devtree",
	.desc = "traverse the device tree",
	.usage = cmd_devtree_usage,
	.exec = cmd_devtree_exec,
};

static int __init cmd_devtree_init(void)
{
	int ret;
	vmm_memset(dtree_curpath, 0, VMM_DEVTREE_MAX_PATH_LEN);
	ret = vmm_devtree_getpath(dtree_curpath, vmm_devtree_rootnode());
	if (ret) {
		return ret;
	}
	return vmm_cmdmgr_register_cmd(&cmd_devtree);
}

static void cmd_devtree_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_devtree);
}

VMM_DECLARE_MODULE(MODULE_VARID, 
			MODULE_NAME, 
			MODULE_AUTHOR, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);

