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
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of devtree command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_heap.h>
#include <vmm_devtree.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>

#define MODULE_NAME			"Command devtree"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_devtree_init
#define	MODULE_EXIT			cmd_devtree_exit

void cmd_devtree_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   devtree help\n");
	vmm_cprintf(cdev, "   devtree attr show <node_path>\n");
	vmm_cprintf(cdev, "   devtree attr set  <node_path> <attr_name> "
						"<attr_type> <attr_val0> "
						"<attr_val1> ...\n");
	vmm_cprintf(cdev, "   devtree attr get  <node_path> <attr_name>\n");
	vmm_cprintf(cdev, "   devtree attr del  <node_path> <attr_name>\n");
	vmm_cprintf(cdev, "   devtree node show <node_path>\n");
	vmm_cprintf(cdev, "   devtree node dump <node_path>\n");
	vmm_cprintf(cdev, "   devtree node add  <node_path> <node_name>\n");
	vmm_cprintf(cdev, "   devtree node copy <node_path> <node_name> "
						"<src_node_path>\n");
	vmm_cprintf(cdev, "   devtree node del  <node_path>\n");
	vmm_cprintf(cdev, "Note:\n");
	vmm_cprintf(cdev, "   <node_path> = unix like path of node "
			  "(e.g. / or /host/cpus or /guests/guest0)\n");
	vmm_cprintf(cdev, "   <attr_type> = unknown|string|uint32|uint64|"
			  		   "physaddr|physsize|"
					   "virtaddr|virtsize\n");
}

void cmd_devtree_print_attribute(struct vmm_chardev *cdev, 
				 struct vmm_devtree_attr * attr, 
				 int indent)
{
	int i;

	for (i = 0; i < indent; i++)
		vmm_cprintf(cdev, "\t");

	if (attr->type == VMM_DEVTREE_ATTRTYPE_STRING) {
		vmm_cprintf(cdev, "\t%s = \"%s\";\n", 
					attr->name, (char *)attr->value);
	} else if (attr->type == VMM_DEVTREE_ATTRTYPE_UINT64) {
		vmm_cprintf(cdev, "\t%s = <", attr->name);
		for (i = 0; i < attr->len; i += sizeof(u64)) {
			if (i > 0) {
				vmm_cprintf(cdev, " ");
			}
			vmm_cprintf(cdev, "0x%llx", 
					((u64 *)attr->value)[i >> 3]);
		}
		vmm_cprintf(cdev, ">;\n", attr->name);
	} else if (attr->type == VMM_DEVTREE_ATTRTYPE_PHYSADDR) {
		vmm_cprintf(cdev, "\t%s = <", attr->name);
		for (i = 0; i < attr->len; i += sizeof(physical_addr_t)) {
			if (i > 0) {
				vmm_cprintf(cdev, " ");
			}
			if (sizeof(u64) == sizeof(physical_addr_t)) {
				vmm_cprintf(cdev, "0x%llxULL", 
				((physical_addr_t *)attr->value)[i >> 3]);
			} else {
				vmm_cprintf(cdev, "0x%x", 
				((physical_addr_t *)attr->value)[i >> 2]);
			}
		}
		vmm_cprintf(cdev, ">;\n", attr->name);
	} else if (attr->type == VMM_DEVTREE_ATTRTYPE_PHYSSIZE) {
		vmm_cprintf(cdev, "\t%s = <", attr->name);
		for (i = 0; i < attr->len; i += sizeof(physical_size_t)) {
			if (i > 0) {
				vmm_cprintf(cdev, " ");
			}
			if (sizeof(u64) == sizeof(physical_size_t)) {
				vmm_cprintf(cdev, "0x%llxULL", 
				((physical_size_t *)attr->value)[i >> 3]);
			} else {
				vmm_cprintf(cdev, "0x%x", 
				((physical_size_t *)attr->value)[i >> 2]);
			}
		}
		vmm_cprintf(cdev, ">;\n", attr->name);
	} else if (attr->type == VMM_DEVTREE_ATTRTYPE_VIRTADDR) {
		vmm_cprintf(cdev, "\t%s = <", attr->name);
		for (i = 0; i < attr->len; i += sizeof(virtual_addr_t)) {
			if (i > 0) {
				vmm_cprintf(cdev, " ");
			}
			if (sizeof(u64) == sizeof(virtual_addr_t)) {
				vmm_cprintf(cdev, "0x%llxULL", 
				((virtual_addr_t *)attr->value)[i >> 3]);
			} else {
				vmm_cprintf(cdev, "0x%x", 
				((virtual_addr_t *)attr->value)[i >> 2]);
			}
		}
		vmm_cprintf(cdev, ">;\n", attr->name);
	} else if (attr->type == VMM_DEVTREE_ATTRTYPE_VIRTSIZE) {
		vmm_cprintf(cdev, "\t%s = <", attr->name);
		for (i = 0; i < attr->len; i += sizeof(virtual_size_t)) {
			if (i > 0) {
				vmm_cprintf(cdev, " ");
			}
			if (sizeof(u64) == sizeof(virtual_size_t)) {
				vmm_cprintf(cdev, "0x%llxULL", 
				((virtual_size_t *)attr->value)[i >> 3]);
			} else {
				vmm_cprintf(cdev, "0x%x", 
				((virtual_size_t *)attr->value)[i >> 2]);
			}
		}
		vmm_cprintf(cdev, ">;\n", attr->name);
	} else {
		vmm_cprintf(cdev, "\t%s = <", attr->name);
		for (i = 0; i < attr->len; i += sizeof(u32)) {
			if (i > 0) {
				vmm_cprintf(cdev, " ");
			}
			vmm_cprintf(cdev, "0x%x", 
					((u32 *)attr->value)[i >> 2]);
		}
		vmm_cprintf(cdev, ">;\n", attr->name);
	}
}

void cmd_devtree_print_node(struct vmm_chardev *cdev, 
			    struct vmm_devtree_node * node, 
			    bool showattr,
			    int indent)
{
	int i;
	bool braceopen;
	struct dlist *l;
	struct vmm_devtree_attr * attr;
	struct vmm_devtree_node *child;

	for (i = 0; i < indent; i++)
		vmm_cprintf(cdev, "\t");

	if (node->name[0] == '\0' && indent == 0) {
		vmm_cprintf(cdev, "%c", VMM_DEVTREE_PATH_SEPARATOR);
	} else {
		vmm_cprintf(cdev, "%s", node->name);
	}

	switch (node->type) {
	case VMM_DEVTREE_NODETYPE_UNKNOWN:
		break;
	case VMM_DEVTREE_NODETYPE_DEVICE:
		vmm_cprintf(cdev, " /* [device] */");
		break;
	case VMM_DEVTREE_NODETYPE_EDEVICE:
		vmm_cprintf(cdev, " /* [edevice] */");
		break;
	default:
		break;
	};

	braceopen = FALSE;
	if (showattr) {
		if (!list_empty(&node->child_list) ||
		    !list_empty(&node->attr_list)) {
			vmm_cprintf(cdev, " {\n");
			braceopen = TRUE;
		}
		list_for_each(l, &node->attr_list) {
			attr = list_entry(l, struct vmm_devtree_attr, head);
			cmd_devtree_print_attribute(cdev, attr, indent);
		}

	} else {
		if (!list_empty(&node->child_list)) {
			vmm_cprintf(cdev, " {\n");
			braceopen = TRUE;
		}
	}

	list_for_each(l, &node->child_list) {
		child = list_entry(l, struct vmm_devtree_node, head);
		cmd_devtree_print_node(cdev, child, showattr, indent + 1);
	}

	if (braceopen) {
		for (i = 0; i < indent; i++)
			vmm_cprintf(cdev, "\t");

		vmm_cprintf(cdev, "}");
	}

	vmm_cprintf(cdev, ";\n");
}

int cmd_devtree_attr_show(struct vmm_chardev *cdev, char *path)
{
	struct dlist *l;
	struct vmm_devtree_attr *attr;
	struct vmm_devtree_node *node = vmm_devtree_getnode(path);

	if (!node) {
		vmm_cprintf(cdev, "Error: Unable to find node at %s\n", path);
		return VMM_EFAIL;
	}

	list_for_each(l, &node->attr_list) {
		attr = list_entry(l, struct vmm_devtree_attr, head);
		cmd_devtree_print_attribute(cdev, attr, 0);
	}

	return VMM_OK;
}

int cmd_devtree_attr_set(struct vmm_chardev *cdev, char *path, char *name,
			 char * type, int valc, char **valv)
{
	int i, rc = VMM_OK;
	u32 val_type = VMM_DEVTREE_ATTRTYPE_UNKNOWN, val_len = 0;
	void * val = NULL;
	struct vmm_devtree_node *node = vmm_devtree_getnode(path);

	if (!node) {
		vmm_cprintf(cdev, "Error: Unable to find node at %s\n", path);
		return VMM_EFAIL;
	}

	if (!vmm_strcmp(type, "unknown")) {
		val = NULL;
		val_len = 0;
		val_type = VMM_DEVTREE_ATTRTYPE_UNKNOWN;
	} else if (!vmm_strcmp(type, "string")) {
		for (i = 0; i < valc; i++) {
			val_len += vmm_strlen(valv[i]);
			val_len += 1;
		}
		val = vmm_malloc(val_len);
		vmm_strcpy(val, valv[0]);
		for (i = 1; i < valc; i++) {
			vmm_strcat(val, " ");
			vmm_strcat(val, valv[i]);
		}
		val_len = vmm_strlen((char *)val);
		val_type = VMM_DEVTREE_ATTRTYPE_STRING;
	} else if (!vmm_strcmp(type, "uint32")) {
		val_len = valc * sizeof(u32);
		val = vmm_malloc(val_len);
		for (i = 0; i < valc; i++) {
			((u32 *)val)[i] = vmm_str2uint(valv[i], 10);
		}
		val_type = VMM_DEVTREE_ATTRTYPE_UINT32;
	} else if (!vmm_strcmp(type, "uint64")) {
		val_len = valc * sizeof(u64);
		val = vmm_malloc(val_len);
		for (i = 0; i < valc; i++) {
			((u64 *)val)[i] = vmm_str2ulonglong(valv[i], 10);
		}
		val_type = VMM_DEVTREE_ATTRTYPE_UINT64;
	} else if (!vmm_strcmp(type, "physaddr")) {
		val_len = valc * sizeof(physical_addr_t);
		val = vmm_malloc(val_len);
		for (i = 0; i < valc; i++) {
			((physical_addr_t *)val)[i] = 
			(physical_addr_t)vmm_str2ulonglong(valv[i], 10);
		}
		val_type = VMM_DEVTREE_ATTRTYPE_PHYSADDR;
	} else if (!vmm_strcmp(type, "physsize")) {
		val_len = valc * sizeof(physical_size_t);
		val = vmm_malloc(val_len);
		for (i = 0; i < valc; i++) {
			((physical_size_t *)val)[i] = 
			(physical_size_t)vmm_str2ulonglong(valv[i], 10);
		}
		val_type = VMM_DEVTREE_ATTRTYPE_PHYSSIZE;
	} else if (!vmm_strcmp(type, "virtaddr")) {
		val_len = valc * sizeof(virtual_addr_t);
		val = vmm_malloc(val_len);
		for (i = 0; i < valc; i++) {
			((virtual_addr_t *)val)[i] = 
			(virtual_addr_t)vmm_str2ulonglong(valv[i], 10);
		}
		val_type = VMM_DEVTREE_ATTRTYPE_VIRTADDR;
	} else if (!vmm_strcmp(type, "virtsize")) {
		val_len = valc * sizeof(virtual_size_t);
		val = vmm_malloc(val_len);
		for (i = 0; i < valc; i++) {
			((virtual_size_t *)val)[i] = 
			(virtual_size_t)vmm_str2ulonglong(valv[i], 10);
		}
		val_type = VMM_DEVTREE_ATTRTYPE_VIRTSIZE;
	} else {
		vmm_cprintf(cdev, "Error: Invalid attribute type %s\n", type);
		return VMM_EFAIL;
	}

	if (val && (val_len > 0)) {
		rc = vmm_devtree_setattr(node, name, val, val_type, val_len);
		vmm_free(val);
	}

	return rc;
}

int cmd_devtree_attr_get(struct vmm_chardev *cdev, char *path, char *name)
{
	struct vmm_devtree_attr *attr;
	struct vmm_devtree_node *node = vmm_devtree_getnode(path);

	if (!node) {
		vmm_cprintf(cdev, "Error: Unable to find node at %s\n", path);
		return VMM_EFAIL;
	}

	attr = vmm_devtree_getattr(node, name);
	if (!attr) {
		vmm_cprintf(cdev, "Error: Unable to find attr %s\n", name);
		return VMM_EFAIL;
	}

	cmd_devtree_print_attribute(cdev, attr, 0);

	return VMM_OK;
}

int cmd_devtree_attr_del(struct vmm_chardev *cdev, char *path, char *name)
{
	int rc;
	struct vmm_devtree_node *node = vmm_devtree_getnode(path);

	if (!node) {
		vmm_cprintf(cdev, "Error: Unable to find node at %s\n", path);
		return VMM_EFAIL;
	}

	rc = vmm_devtree_delattr(node, name);
	if (rc) {
		vmm_cprintf(cdev, "Error: Unable to delete attr %s\n", name);
		return rc;
	}

	return VMM_OK;
}

int cmd_devtree_node_show(struct vmm_chardev *cdev, char *path)
{
	struct vmm_devtree_node *node = vmm_devtree_getnode(path);

	if (!node) {
		vmm_cprintf(cdev, "Error: Unable to find node at %s\n", path);
		return VMM_EFAIL;
	}

	cmd_devtree_print_node(cdev, node, FALSE, 0);

	return VMM_OK;
}

int cmd_devtree_node_dump(struct vmm_chardev *cdev, char *path)
{
	struct vmm_devtree_node *node = vmm_devtree_getnode(path);

	if (!node) {
		vmm_cprintf(cdev, "Error: Unable to find node at %s\n", path);
		return VMM_EFAIL;
	}

	cmd_devtree_print_node(cdev, node, TRUE, 0);

	return VMM_OK;
}

int cmd_devtree_node_add(struct vmm_chardev *cdev, char *path, char *name)
{
	struct vmm_devtree_node *parent = vmm_devtree_getnode(path);
	struct vmm_devtree_node *node = NULL;

	if (!parent) {
		vmm_cprintf(cdev, "Error: Unable to find node at %s\n", path);
		return VMM_EFAIL;
	}

	node = vmm_devtree_addnode(parent, name, 
				   VMM_DEVTREE_NODETYPE_UNKNOWN, NULL);
	if (!node) {
		vmm_cprintf(cdev, "Error: Unable to add node %s. "
				  "Probably node already exist\n", name);
		return VMM_EFAIL;
	}

	return VMM_OK;
}

int cmd_devtree_node_copy(struct vmm_chardev *cdev, 
			  char *path, char *name, char *src_path)
{
	struct vmm_devtree_node *node = vmm_devtree_getnode(path);
	struct vmm_devtree_node *src = vmm_devtree_getnode(src_path);

	if (!node) {
		vmm_cprintf(cdev, "Error: Unable to find node at %s\n", path);
		return VMM_EFAIL;
	}

	if (!src) {
		vmm_cprintf(cdev, "Error: Unable to find node at %s\n", src_path);
		return VMM_EFAIL;
	}

	return vmm_devtree_copynode(node, name, src);
}

int cmd_devtree_node_del(struct vmm_chardev *cdev, char *path)
{
	int rc;
	struct vmm_devtree_node *node = vmm_devtree_getnode(path);

	if (!node) {
		vmm_cprintf(cdev, "Error: Unable to find node at %s\n", path);
		return VMM_EFAIL;
	}

	rc = vmm_devtree_delnode(node);
	if (rc) {
		vmm_cprintf(cdev, "Error: Unable to delete node at %s\n", path);
		return rc;
	}

	return VMM_OK;
}

int cmd_devtree_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	if (argc < 2) {
		cmd_devtree_usage(cdev);
		return VMM_EFAIL;
	} else {
		if (argc == 2) {
			if (vmm_strcmp(argv[1], "help") == 0) {
				cmd_devtree_usage(cdev);
				return VMM_OK;
			} else {
				cmd_devtree_usage(cdev);
				return VMM_EFAIL;
			}
		} else if (argc < 4) {
			cmd_devtree_usage(cdev);
			return VMM_EFAIL;
		}
	}
	if (vmm_strcmp(argv[1], "attr") == 0) {
		if (vmm_strcmp(argv[2], "show") == 0) {
			return cmd_devtree_attr_show(cdev, argv[3]);
		} else if ((vmm_strcmp(argv[2], "set") == 0) && (argc > 6)) {
			return cmd_devtree_attr_set(cdev, argv[3], argv[4], 
						argv[5], argc - 6, &argv[6]);
		} else if ((vmm_strcmp(argv[2], "get") == 0) && (argc == 5)) {
			return cmd_devtree_attr_get(cdev, argv[3], argv[4]);
		} else if ((vmm_strcmp(argv[2], "del") == 0) && (argc == 5)) {
			return cmd_devtree_attr_del(cdev, argv[3], argv[4]);
		}
	} else if (vmm_strcmp(argv[1], "node") == 0) {
		if (vmm_strcmp(argv[2], "show") == 0) {
			return cmd_devtree_node_show(cdev, argv[3]);
		} else 	if (vmm_strcmp(argv[2], "dump") == 0) {
			return cmd_devtree_node_dump(cdev, argv[3]);
		} else if ((vmm_strcmp(argv[2], "add") == 0) && (argc == 5)) {
			return cmd_devtree_node_add(cdev, argv[3], argv[4]);
		} else if ((vmm_strcmp(argv[2], "copy") == 0) && (argc == 6)) {
			return cmd_devtree_node_copy(cdev, argv[3], 
							argv[4], argv[5]);
		} else if (vmm_strcmp(argv[2], "del") == 0) {
			return cmd_devtree_node_del(cdev, argv[3]);
		}
	}
	cmd_devtree_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_devtree = {
	.name = "devtree",
	.desc = "traverse the device tree",
	.usage = cmd_devtree_usage,
	.exec = cmd_devtree_exec,
};

static int __init cmd_devtree_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_devtree);
}

static void __exit cmd_devtree_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_devtree);
}

VMM_DECLARE_MODULE(MODULE_NAME, 
			MODULE_AUTHOR, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);

