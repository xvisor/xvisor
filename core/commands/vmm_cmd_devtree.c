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
 * @file vmm_cmd_devtree.c
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of devtree command
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_devtree.h>
#include <vmm_mterm.h>

#define VMM_DEVTREE_MAX_PATH_LEN		256

char *dtree_curpath;

void cmd_devtree_usage(void)
{
	vmm_printf("Usage:\n");
	vmm_printf("   devtree help\n");
	vmm_printf("   devtree curpath\n");
	vmm_printf("   devtree chpath <path>\n");
	vmm_printf("   devtree attrib [<path>]\n");
	vmm_printf("   devtree print  [<path>]\n");
}

void cmd_devtree_print_attributes(vmm_devtree_node_t * node, int indent)
{
	int i;
	struct dlist *lentry;
	vmm_devtree_attr_t *attr;

	if (!node)
		return;

	list_for_each(lentry, &node->attr_list) {
		attr = list_entry(lentry, vmm_devtree_attr_t, head);
		for (i = 0; i < indent; i++)
			vmm_printf("\t");
		if (attr->value[attr->len - 1] == '\0' && 4 < attr->len) {
			vmm_printf("\t%s = \"%s\";\n", attr->name, attr->value);
		} else {
			vmm_printf("\t%s = 0x%x;\n", attr->name,
				   *((u32 *) attr->value));
		}
	}
}

void cmd_devtree_print_node(vmm_devtree_node_t * node, int indent)
{
	int i;
	struct dlist *lentry;
	vmm_devtree_node_t *child;

	for (i = 0; i < indent; i++)
		vmm_printf("\t");
	if (node->name[0] == '\0' && indent == 0) {
		vmm_printf("%c", VMM_DEVTREE_PATH_SEPRATOR);
	} else {
		vmm_printf("%s", node->name);
	}

	switch (node->type) {
	case VMM_DEVTREE_NODETYPE_UNKNOWN:
		break;
	case VMM_DEVTREE_NODETYPE_DEVICE:
		vmm_printf(" [device]");
		break;
	case VMM_DEVTREE_NODETYPE_EDEVICE:
		vmm_printf(" [edevice]");
		break;
	default:
		break;
	};

	if (!list_empty(&node->child_list)) {
		vmm_printf(" {\n");
	}

	list_for_each(lentry, &node->child_list) {
		child = list_entry(lentry, vmm_devtree_node_t, head);
		cmd_devtree_print_node(child, indent + 1);
	}

	if (!list_empty(&node->child_list)) {
		for (i = 0; i < indent; i++)
			vmm_printf("\t");

		vmm_printf("}");
	}
	vmm_printf(";\n");
}

int cmd_devtree_curpath(void)
{
	vmm_printf("%s\r\n", dtree_curpath);
	return VMM_OK;
}

int cmd_devtree_chpath(char *path)
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
		vmm_printf("New path: %s\n", dtree_curpath);
	} else {
		vmm_printf("Invalid path: %s\n", path);
	}

	return VMM_OK;
}

int cmd_devtree_attrib(char *path)
{
	vmm_devtree_node_t *node = vmm_devtree_getnode(path);

	if (!node) {
		vmm_printf("Failed to print attributes\n");
		return VMM_EFAIL;
	}

	cmd_devtree_print_attributes(node, 0);

	return VMM_OK;
}

int cmd_devtree_print(char *path)
{
	vmm_devtree_node_t *node = vmm_devtree_getnode(path);

	if (!node) {
		vmm_printf("Failed to print device tree\n");
		return VMM_EFAIL;
	}

	cmd_devtree_print_node(node, 0);

	return VMM_OK;
}

int cmd_devtree_exec(int argc, char **argv)
{
	if (argc < 2) {
		cmd_devtree_usage();
		return VMM_EFAIL;
	}
	if (argc == 2) {
		if (vmm_strcmp(argv[1], "help") == 0) {
			cmd_devtree_usage();
			return VMM_OK;
		}
	}
	if (vmm_strcmp(argv[1], "curpath") == 0) {
		return cmd_devtree_curpath();
	} else if (vmm_strcmp(argv[1], "chpath") == 0) {
		if (argc < 3) {
			cmd_devtree_usage();
			return VMM_EFAIL;
		}
		return cmd_devtree_chpath(argv[2]);
	} else if (vmm_strcmp(argv[1], "attrib") == 0) {
		if (argc < 3) {
			return cmd_devtree_attrib(dtree_curpath);
		} else {
			return cmd_devtree_attrib(argv[2]);
		}
	} else if (vmm_strcmp(argv[1], "print") == 0) {
		if (argc < 3) {
			return cmd_devtree_print(dtree_curpath);
		} else {
			return cmd_devtree_print(argv[2]);
		}
	} else {
		cmd_devtree_usage();
		return VMM_EFAIL;
	}
	return VMM_OK;
}

int cmd_devtree_init(void)
{
	int ret;
	dtree_curpath = vmm_malloc(VMM_DEVTREE_MAX_PATH_LEN);
	vmm_memset(dtree_curpath, 0, VMM_DEVTREE_MAX_PATH_LEN);
	ret = vmm_devtree_getpath(dtree_curpath, vmm_devtree_rootnode());
	return ret;
}

VMM_DECLARE_CMD(devtree, "traverse the device tree", cmd_devtree_exec,
		cmd_devtree_init);
