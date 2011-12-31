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
 * @file vmm_devtree.c
 * @version 0.1
 * @author Anup Patel (anup@brainfault.org)
 * @brief Device Tree Implementation.
 */

#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_board.h>
#include <vmm_devtree.h>

struct vmm_devtree_ctrl {
        char *str_buf;
        size_t str_buf_size;
        vmm_devtree_node_t *root;
};

static struct vmm_devtree_ctrl dtree_ctrl;

const char *vmm_devtree_attrval(vmm_devtree_node_t * node, const char *attrib)
{
	vmm_devtree_attr_t *attr;
	struct dlist *l;

	if (!node) {
		return NULL;
	}

	list_for_each(l, &node->attr_list) {
		attr = list_entry(l, vmm_devtree_attr_t, head);
		if (vmm_strcmp(attr->name, attrib) == 0) {
			return attr->value;
		}
	}

	return NULL;
}

u32 vmm_devtree_attrlen(vmm_devtree_node_t * node, const char *attrib)
{
	vmm_devtree_attr_t *attr;
	struct dlist *l;

	if (!node) {
		return 0;
	}

	list_for_each(l, &node->attr_list) {
		attr = list_entry(l, vmm_devtree_attr_t, head);
		if (vmm_strcmp(attr->name, attrib) == 0) {
			return attr->len;
		}
	}

	return 0;
}

void recursive_getpath(char **out, vmm_devtree_node_t * node)
{
	if (!node)
		return;

	if (node->parent) {
		recursive_getpath(out, node->parent);
		**out = VMM_DEVTREE_PATH_SEPARATOR;
		(*out) += 1;
		**out = '\0';
	}

	vmm_strcat(*out, node->name);
	(*out) += vmm_strlen(node->name);
}

int vmm_devtree_getpath(char *out, vmm_devtree_node_t * node)
{
	char *out_ptr = out;

	if (!node)
		return VMM_EFAIL;

	vmm_strcpy(out, "");

	recursive_getpath(&out_ptr, node);

	if (vmm_strcmp(out, "") == 0) {
		out[0] = VMM_DEVTREE_PATH_SEPARATOR;
		out[1] = '\0';
	}

	return VMM_OK;
}

vmm_devtree_node_t *vmm_devtree_getchildnode(vmm_devtree_node_t * node,
					     const char *path)
{
	bool found;
	struct dlist *lentry;
	vmm_devtree_node_t *child;

	if (!path || !node)
		return NULL;

	while (*path) {
		found = FALSE;
		list_for_each(lentry, &node->child_list) {
			child = list_entry(lentry, vmm_devtree_node_t, head);
			if (vmm_strncmp(child->name, path,
					vmm_strlen(child->name)) == 0) {
				found = TRUE;
				path += vmm_strlen(child->name);
				if (*path) {
					if (*path != VMM_DEVTREE_PATH_SEPARATOR
					    && *(path + 1) != '\0')
						return NULL;
					if (*path == VMM_DEVTREE_PATH_SEPARATOR)
						path++;
				}
				break;
			}
		}
		if (!found)
			return NULL;
		node = child;
	};

	return node;
}

vmm_devtree_node_t *vmm_devtree_getnode(const char *path)
{
	vmm_devtree_node_t *node = dtree_ctrl.root;

	if (!path || !node)
		return NULL;

	if (vmm_strncmp(node->name, path, vmm_strlen(node->name)) != 0)
		return NULL;

	path += vmm_strlen(node->name);

	if (*path) {
		if (*path != VMM_DEVTREE_PATH_SEPARATOR && *(path + 1) != '\0')
			return NULL;
		if (*path == VMM_DEVTREE_PATH_SEPARATOR)
			path++;
	}

	return vmm_devtree_getchildnode(node, path);
}

vmm_devtree_node_t *vmm_devtree_rootnode(void)
{
	return dtree_ctrl.root;
}

int __init vmm_devtree_init(void)
{
	/* Reset the control structure */
	vmm_memset(&dtree_ctrl, 0, sizeof(dtree_ctrl));

	/* Populate Device Tree */
	return vmm_devtree_populate(&dtree_ctrl.root,
				    &dtree_ctrl.str_buf,
				    &dtree_ctrl.str_buf_size);
}
