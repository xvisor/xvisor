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
 * @author Anup Patel (anup@brainfault.org)
 * @brief Device Tree Implementation.
 */

#include <vmm_error.h>
#include <vmm_compiler.h>
#include <vmm_heap.h>
#include <vmm_host_aspace.h>
#include <vmm_devtree.h>
#include <arch_board.h>
#include <libs/stringlib.h>

struct vmm_devtree_ctrl {
        struct vmm_devtree_node *root;
};

static struct vmm_devtree_ctrl dtree_ctrl;

bool vmm_devtree_isliteral(u32 attrtype)
{
	bool ret = FALSE;

	switch(attrtype) {
	case VMM_DEVTREE_ATTRTYPE_UNKNOWN:
	case VMM_DEVTREE_ATTRTYPE_UINT32:
	case VMM_DEVTREE_ATTRTYPE_UINT64:
	case VMM_DEVTREE_ATTRTYPE_VIRTADDR:
	case VMM_DEVTREE_ATTRTYPE_VIRTSIZE:
	case VMM_DEVTREE_ATTRTYPE_PHYSADDR:
	case VMM_DEVTREE_ATTRTYPE_PHYSSIZE:
		ret = TRUE;
		break;
	};

	return ret;
}

u32 vmm_devtree_literal_size(u32 attrtype)
{
	u32 ret = 0;

	switch(attrtype) {
	case VMM_DEVTREE_ATTRTYPE_UNKNOWN:
		ret = sizeof(u32);
		break;
	case VMM_DEVTREE_ATTRTYPE_UINT32:
		ret = sizeof(u32);
		break;
	case VMM_DEVTREE_ATTRTYPE_UINT64:
		ret = sizeof(u64);
		break;
	case VMM_DEVTREE_ATTRTYPE_VIRTADDR:
		ret = sizeof(virtual_addr_t);
		break;
	case VMM_DEVTREE_ATTRTYPE_VIRTSIZE:
		ret = sizeof(virtual_size_t);
		break;
	case VMM_DEVTREE_ATTRTYPE_PHYSADDR:
		ret = sizeof(physical_addr_t);
		break;
	case VMM_DEVTREE_ATTRTYPE_PHYSSIZE:
		ret = sizeof(physical_size_t);
		break;
	};

	return ret;
}

u32 vmm_devtree_estimate_attrtype(const char *name)
{
	u32 ret = VMM_DEVTREE_ATTRTYPE_UNKNOWN;

	if (!name) {
		return ret;
	}

	if (!strcmp(name, VMM_DEVTREE_MODEL_ATTR_NAME)) {
		ret = VMM_DEVTREE_ATTRTYPE_STRING;
	} else if (!strcmp(name, VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME)) {
		ret = VMM_DEVTREE_ATTRTYPE_STRING;
	} else if (!strcmp(name, VMM_DEVTREE_COMPATIBLE_ATTR_NAME)) {
		ret = VMM_DEVTREE_ATTRTYPE_STRING;
	} else if (!strcmp(name, VMM_DEVTREE_CLOCK_RATE_ATTR_NAME)) {
		ret = VMM_DEVTREE_ATTRTYPE_UINT32;
	} else if (!strcmp(name, VMM_DEVTREE_REG_ATTR_NAME)) {
		ret = VMM_DEVTREE_ATTRTYPE_PHYSADDR;
	} else if (!strcmp(name, VMM_DEVTREE_VIRTUAL_REG_ATTR_NAME)) {
		ret = VMM_DEVTREE_ATTRTYPE_VIRTADDR;
	} else if (!strcmp(name, VMM_DEVTREE_CPU_FREQ_MHZ_ATTR_NAME)) {
		ret = VMM_DEVTREE_ATTRTYPE_UINT32;
	} else if (!strcmp(name, VMM_DEVTREE_MEMORY_PHYS_ADDR_ATTR_NAME)) {
		ret = VMM_DEVTREE_ATTRTYPE_PHYSADDR;
	} else if (!strcmp(name, VMM_DEVTREE_MEMORY_PHYS_SIZE_ATTR_NAME)) {
		ret = VMM_DEVTREE_ATTRTYPE_PHYSSIZE;
	} else if (!strcmp(name, VMM_DEVTREE_START_PC_ATTR_NAME)) {
		ret = VMM_DEVTREE_ATTRTYPE_VIRTADDR;
	} else if (!strcmp(name, VMM_DEVTREE_PRIORITY_ATTR_NAME)) {
		ret = VMM_DEVTREE_ATTRTYPE_UINT32;
	} else if (!strcmp(name, VMM_DEVTREE_TIME_SLICE_ATTR_NAME)) {
		ret = VMM_DEVTREE_ATTRTYPE_UINT32;
	} else if (!strcmp(name, VMM_DEVTREE_H2GIRQMAP_ATTR_NAME)) {
		ret = VMM_DEVTREE_ATTRTYPE_UINT32;
	} else if (!strcmp(name, VMM_DEVTREE_MANIFEST_TYPE_ATTR_NAME)) {
		ret = VMM_DEVTREE_ATTRTYPE_STRING;
	} else if (!strcmp(name, VMM_DEVTREE_ADDRESS_TYPE_ATTR_NAME)) {
		ret = VMM_DEVTREE_ATTRTYPE_STRING;
	} else if (!strcmp(name, VMM_DEVTREE_GUEST_PHYS_ATTR_NAME)) {
		ret = VMM_DEVTREE_ATTRTYPE_PHYSADDR;
	} else if (!strcmp(name, VMM_DEVTREE_HOST_PHYS_ATTR_NAME)) {
		ret = VMM_DEVTREE_ATTRTYPE_PHYSADDR;
	} else if (!strcmp(name, VMM_DEVTREE_ALIAS_PHYS_ATTR_NAME)) {
		ret = VMM_DEVTREE_ATTRTYPE_PHYSADDR;
	} else if (!strcmp(name, VMM_DEVTREE_PHYS_SIZE_ATTR_NAME)) {
		ret = VMM_DEVTREE_ATTRTYPE_PHYSSIZE;
	} else if (!strcmp(name, VMM_DEVTREE_SWITCH_ATTR_NAME)) {
		ret = VMM_DEVTREE_ATTRTYPE_STRING;
	} else if (!strcmp(name, VMM_DEVTREE_CONSOLE_ATTR_NAME)) {
		ret = VMM_DEVTREE_ATTRTYPE_STRING;
	} else if (!strcmp(name, VMM_DEVTREE_RTCDEV_ATTR_NAME)) {
		ret = VMM_DEVTREE_ATTRTYPE_STRING;
	} else if (!strcmp(name, VMM_DEVTREE_BLKDEV_ATTR_NAME)) {
		ret = VMM_DEVTREE_ATTRTYPE_STRING;
	}

	return ret;
}

int vmm_devtree_regmap(struct vmm_devtree_node *node, 
		       virtual_addr_t *addr, int regset)
{
	const char *aval;
	physical_addr_t pa;
	physical_size_t sz;

	if (!node || !addr || regset < 0) {
		return VMM_EFAIL;
	}

	aval = vmm_devtree_attrval(node, VMM_DEVTREE_VIRTUAL_REG_ATTR_NAME);
	if (aval) {
		/* Directly return the "virtual-reg" attribute */
		aval += regset * sizeof(virtual_addr_t);
		*addr = *((virtual_addr_t *)aval);
	} else {
		aval = vmm_devtree_attrval(node, VMM_DEVTREE_REG_ATTR_NAME);
		if (aval) {
			aval += regset * (sizeof(physical_addr_t) + 
					  sizeof(physical_size_t));
			pa = *((physical_addr_t *)aval);
			aval += sizeof(physical_addr_t);
			sz = *((physical_size_t *)aval);
			*addr = vmm_host_iomap(pa, sz);
		} else {
			return VMM_EFAIL;
		}
	}

	return VMM_OK;
}

int vmm_devtree_regsize(struct vmm_devtree_node *node, 
		        physical_size_t *size, int regset)
{
	const char *aval;

	if (!node || !size || regset < 0) {
		return VMM_EFAIL;
	}

	aval = vmm_devtree_attrval(node, VMM_DEVTREE_VIRTUAL_REG_ATTR_NAME);
	if (aval) {
		return VMM_EFAIL;
	} else {
		aval = vmm_devtree_attrval(node, VMM_DEVTREE_REG_ATTR_NAME);
		if (aval) {
			aval += regset * (sizeof(physical_addr_t) + 
					  sizeof(physical_size_t));
			aval += sizeof(physical_addr_t);
			*size = *((physical_size_t *)aval);
		} else {
			return VMM_EFAIL;
		}
	}

	return VMM_OK;
}

int vmm_devtree_regaddr(struct vmm_devtree_node *node, 
		        physical_addr_t *addr, int regset)
{
	const char *aval;

	if (!node || !addr || regset < 0) {
		return VMM_EFAIL;
	}

	aval = vmm_devtree_attrval(node, VMM_DEVTREE_VIRTUAL_REG_ATTR_NAME);
	if (aval) {
		return VMM_EFAIL;
	} else {
		aval = vmm_devtree_attrval(node, VMM_DEVTREE_REG_ATTR_NAME);
		if (aval) {
			aval += regset * (sizeof(physical_addr_t) + 
					  sizeof(physical_size_t));
			*addr = *((physical_addr_t *)aval);
		} else {
			return VMM_EFAIL;
		}
	}

	return VMM_OK;
}

int vmm_devtree_regunmap(struct vmm_devtree_node *node, 
		         virtual_addr_t addr, int regset)
{
	const char *aval;
	physical_addr_t pa;
	physical_size_t sz;

	if (!node || regset < 0) {
		return VMM_EFAIL;
	}

	aval = vmm_devtree_attrval(node, VMM_DEVTREE_VIRTUAL_REG_ATTR_NAME);
	if (aval) {
		return VMM_OK;
	}

	aval = vmm_devtree_attrval(node, VMM_DEVTREE_REG_ATTR_NAME);
	if (aval) {
		aval += regset * (sizeof(pa) + sizeof(sz));
		pa = *((physical_addr_t *)aval);
		aval += sizeof(pa);
		sz = *((physical_size_t *)aval);
		return vmm_host_iounmap(addr, sz);
	}

	return VMM_EFAIL;
}

void *vmm_devtree_attrval(struct vmm_devtree_node *node, 
			  const char *attrib)
{
	struct vmm_devtree_attr *attr;
	struct dlist *l;

	if (!node || !attrib) {
		return NULL;
	}

	list_for_each(l, &node->attr_list) {
		attr = list_entry(l, struct vmm_devtree_attr, head);
		if (strcmp(attr->name, attrib) == 0) {
			return attr->value;
		}
	}

	return NULL;
}

u32 vmm_devtree_attrlen(struct vmm_devtree_node *node, const char *attrib)
{
	struct vmm_devtree_attr *attr;
	struct dlist *l;

	if (!node || !attrib) {
		return 0;
	}

	list_for_each(l, &node->attr_list) {
		attr = list_entry(l, struct vmm_devtree_attr, head);
		if (strcmp(attr->name, attrib) == 0) {
			return attr->len;
		}
	}

	return 0;
}

int vmm_devtree_setattr(struct vmm_devtree_node *node,
			const char *name,
			void *value,
			u32 type,
			u32 len)
{
	bool found;
	struct dlist *l;
	struct vmm_devtree_attr *attr;

	if (!node || !name || !value) {
		return VMM_EFAIL;
	}

	found = FALSE;
	list_for_each(l, &node->attr_list) {
		attr = list_entry(l, struct vmm_devtree_attr, head);
		if (strcmp(attr->name, name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		attr = vmm_malloc(sizeof(struct vmm_devtree_attr));
		INIT_LIST_HEAD(&attr->head);
		attr->len = len;
		attr->type = type;
		len = strlen(name) + 1;
		attr->name = vmm_malloc(len);
		strncpy(attr->name, name, len);
		attr->value = vmm_malloc(attr->len);
		memcpy(attr->value, value, attr->len);
		list_add_tail(&attr->head, &node->attr_list);
	} else {
		if (attr->len != len) {
			attr->len = len;
			vmm_free(attr->value);
			attr->value = vmm_malloc(attr->len);
		}
		attr->type = type;
		memcpy(attr->value, value, attr->len);
	}

	return VMM_OK;
}

struct vmm_devtree_attr *vmm_devtree_getattr(struct vmm_devtree_node *node,
					     const char *name)
{
	struct dlist *l;
	struct vmm_devtree_attr *attr;

	if (!node || !name) {
		return NULL;
	}

	list_for_each(l, &node->attr_list) {
		attr = list_entry(l, struct vmm_devtree_attr, head);
		if (strcmp(attr->name, name) == 0) {
			return attr;
		}
	}

	return NULL;
}

int vmm_devtree_delattr(struct vmm_devtree_node *node, const char *name)
{
	struct vmm_devtree_attr *attr;

	if (!node || !name) {
		return VMM_EFAIL;
	}

	attr = vmm_devtree_getattr(node, name);
	if (!attr) {
		return VMM_EFAIL;
	}

	vmm_free(attr->name);
	vmm_free(attr->value);
	list_del(&attr->head);
	vmm_free(attr);

	return VMM_OK;
}

void recursive_getpath(char **out, struct vmm_devtree_node *node)
{
	if (!node)
		return;

	if (node->parent) {
		recursive_getpath(out, node->parent);
		**out = VMM_DEVTREE_PATH_SEPARATOR;
		(*out) += 1;
		**out = '\0';
	}

	strcat(*out, node->name);
	(*out) += strlen(node->name);
}

int vmm_devtree_getpath(char *out, struct vmm_devtree_node *node)
{
	char *out_ptr = out;

	if (!node)
		return VMM_EFAIL;

	strcpy(out, "");

	recursive_getpath(&out_ptr, node);

	if (strcmp(out, "") == 0) {
		out[0] = VMM_DEVTREE_PATH_SEPARATOR;
		out[1] = '\0';
	}

	return VMM_OK;
}

struct vmm_devtree_node *vmm_devtree_getchild(struct vmm_devtree_node *node,
					      const char *path)
{
	bool found;
	struct dlist *lentry;
	struct vmm_devtree_node *child;

	if (!path || !node)
		return NULL;

	while (*path) {
		found = FALSE;
		list_for_each(lentry, &node->child_list) {
			child = list_entry(lentry, struct vmm_devtree_node, head);
			if (strncmp(child->name, path, strlen(child->name)) == 0) {
				found = TRUE;
				path += strlen(child->name);
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

struct vmm_devtree_node *vmm_devtree_getnode(const char *path)
{
	struct vmm_devtree_node *node = dtree_ctrl.root;

	if (!path || !node)
		return NULL;

	if (strncmp(node->name, path, strlen(node->name)) != 0)
		return NULL;

	path += strlen(node->name);

	if (*path) {
		if (*path != VMM_DEVTREE_PATH_SEPARATOR && *(path + 1) != '\0')
			return NULL;
		if (*path == VMM_DEVTREE_PATH_SEPARATOR)
			path++;
	}

	return vmm_devtree_getchild(node, path);
}

/* NOTE: vmm_devtree_addnode() allows parent == NULL to enable creation of
 * root node using vmm_devtree_addnode().
 */
struct vmm_devtree_node *vmm_devtree_addnode(struct vmm_devtree_node *parent,
					     const char *name)
{
	u32 len;
	struct dlist *l;
	struct vmm_devtree_node *node = NULL;

	if (!name) {
		return NULL;
	}
	if (parent) {
		list_for_each(l, &parent->child_list) {
			node = list_entry(l, struct vmm_devtree_node, head);
			if (strcmp(node->name, name) == 0) {
				return NULL;
			}
		}
	}

	node = vmm_malloc(sizeof(struct vmm_devtree_node));
	INIT_LIST_HEAD(&node->head);
	INIT_LIST_HEAD(&node->attr_list);
	INIT_LIST_HEAD(&node->child_list);
	len = strlen(name) + 1;
	node->name = vmm_malloc(len);
	strncpy(node->name, name, len);
	node->system_data = NULL;
	node->priv = NULL;
	node->parent = parent;
	if (parent) {
		list_add_tail(&node->head, &parent->child_list);
	}

	return node;
}

static int devtree_copynode_recursive(struct vmm_devtree_node *dst,
				      struct vmm_devtree_node *src)
{
	int rc;
	struct dlist *l;
	struct vmm_devtree_attr *sattr = NULL;
	struct vmm_devtree_node *child = NULL;
	struct vmm_devtree_node *schild = NULL;

	list_for_each(l, &src->attr_list) {
		sattr = list_entry(l, struct vmm_devtree_attr, head);
		if ((rc = vmm_devtree_setattr(dst, sattr->name, 
				sattr->value, sattr->type, sattr->len))) {
			return rc;
		}
	}

	list_for_each(l, &src->child_list) {
		schild = list_entry(l, struct vmm_devtree_node, head);
		child = vmm_devtree_addnode(dst, schild->name);
		if (!child) {
			return VMM_EFAIL;
		}
		if ((rc = devtree_copynode_recursive(child, schild))) {
			return rc;
		}
	}
	
	return VMM_OK;
}

int vmm_devtree_copynode(struct vmm_devtree_node *parent,
			 const char *name,
			 struct vmm_devtree_node *src)
{
	struct vmm_devtree_node *node = NULL;

	if (!parent || !name || !src) {
		return VMM_EFAIL;
	}

	node = parent;
	while (node && src != node) {
		node = node->parent;
	}
	if (src == node) {
		return VMM_EFAIL;
	}
	node = NULL;

	node = vmm_devtree_addnode(parent, name);
	if (!node) {
		return VMM_EFAIL;
	}

	return devtree_copynode_recursive(node, src);
}

int vmm_devtree_delnode(struct vmm_devtree_node *node)
{
	int rc;
	struct dlist *l;
	struct vmm_devtree_attr *attr;
	struct vmm_devtree_node *child;

	if (!node || (node == dtree_ctrl.root)) {
		return VMM_EFAIL;
	}

	while(!list_empty(&node->attr_list)) {
		l = list_first(&node->attr_list);
		attr = list_entry(l, struct vmm_devtree_attr, head);
		if ((rc = vmm_devtree_delattr(node, attr->name))) {
			return rc;
		}
	}

	while(!list_empty(&node->child_list)) {
		l = list_first(&node->child_list);
		child = list_entry(l, struct vmm_devtree_node, head);
		if ((rc = vmm_devtree_delnode(child))) {
			return rc;
		}
	}

	list_del(&node->head);

	vmm_free(node);

	return VMM_OK;
}

struct vmm_devtree_node *vmm_devtree_rootnode(void)
{
	return dtree_ctrl.root;
}

int __init vmm_devtree_init(void)
{
	/* Reset the control structure */
	memset(&dtree_ctrl, 0, sizeof(dtree_ctrl));

	/* Populate Board Specific Device Tree */
	return arch_board_devtree_populate(&dtree_ctrl.root);
}
