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
 * @file vmm_libfdt.c
 * @version 0.1
 * @author Anup Patel. (anup@brainfault.org)
 * @brief Flattend device tree library source
 */

#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_heap.h>
#include <vmm_devtree.h>
#include <vmm_libfdt.h>

#define LIBFDT_DATA32(ptr)	(*((u32*)ptr))

void libfdt_node_parse_recursive(vmm_devtree_node_t * node,
				 char **data_ptr, char *str_buf)
{
	vmm_devtree_node_t *child;
	vmm_devtree_attr_t *attr;

	if (LIBFDT_DATA32(*data_ptr) != FDT_BEGIN_NODE)
		return;

	*data_ptr += sizeof(u32);

	node->name = vmm_malloc(vmm_strlen(*data_ptr) + 1);
	vmm_strcpy(node->name, *data_ptr);
	node->type = VMM_DEVTREE_NODETYPE_UNKNOWN;
	node->priv = NULL;
	*data_ptr += vmm_strlen(*data_ptr) + 1;
	while ((u32) (*data_ptr) % sizeof(u32) != 0)
		(*data_ptr)++;

	while (LIBFDT_DATA32(*data_ptr) != FDT_END_NODE) {
		switch (LIBFDT_DATA32(*data_ptr)) {
		case FDT_PROP:
			*data_ptr += sizeof(u32);
			attr = vmm_malloc(sizeof(vmm_devtree_attr_t));
			INIT_LIST_HEAD(&attr->head);
			attr->len = LIBFDT_DATA32(*data_ptr);
			*data_ptr += sizeof(u32);
			attr->name = &str_buf[LIBFDT_DATA32(*data_ptr)];
			*data_ptr += sizeof(u32);
			attr->value = vmm_malloc(attr->len);
			vmm_memcpy(attr->value, *data_ptr, attr->len);
			*data_ptr += attr->len;
			while ((u32) (*data_ptr) % sizeof(u32) != 0)
				(*data_ptr)++;
			list_add_tail(&node->attr_list, &attr->head);
			break;
		case FDT_NOP:
			*data_ptr += sizeof(u32);
			break;
		case FDT_BEGIN_NODE:
			child = vmm_malloc(sizeof(vmm_devtree_node_t));
			INIT_LIST_HEAD(&child->head);
			INIT_LIST_HEAD(&child->attr_list);
			INIT_LIST_HEAD(&child->child_list);
			child->parent = node;
			libfdt_node_parse_recursive(child, data_ptr, str_buf);
			list_add_tail(&node->child_list, &child->head);
			break;
		default:
			return;
			break;
		};
	}

	*data_ptr += sizeof(u32);

	return;
}

int vmm_libfdt_parse(virtual_addr_t fdt_addr,
		     vmm_devtree_node_t ** root,
		     char **string_buffer, size_t * string_buffer_size)
{
	virtual_addr_t data_addr, str_addr;
	size_t data_size, str_size;
	char *data_ptr;
	struct vmm_fdt_header *header;

	header = (struct vmm_fdt_header *)fdt_addr;

	/* Check magic number of header for sainity */
	if (header->magic != FDT_MAGIC) {
		return VMM_EFAIL;
	}

	/* Compute data location & size */
	data_addr = fdt_addr;
	data_addr += sizeof(vmm_fdt_header_t);
	data_addr += sizeof(vmm_fdt_reserve_entry_t);
	data_size = header->size_dt_struct;

	/* Compute strings location & size */
	str_addr = data_addr + data_size;
	str_size = header->size_dt_strings;

	/* Allocate string buffer */
	*string_buffer = vmm_malloc(str_size);
	vmm_memcpy(*string_buffer, (char *)str_addr, str_size);
	*string_buffer_size = str_size;

	/* Setup root node */
	*root = vmm_malloc(sizeof(vmm_devtree_node_t));
	INIT_LIST_HEAD(&(*root)->head);
	INIT_LIST_HEAD(&(*root)->attr_list);
	INIT_LIST_HEAD(&(*root)->child_list);
	(*root)->name = NULL;
	(*root)->type = VMM_DEVTREE_NODETYPE_UNKNOWN;
	(*root)->priv = NULL;
	(*root)->parent = NULL;

	/* Parse FDT recursively */
	data_ptr = (char *)data_addr;
	libfdt_node_parse_recursive(*root, &data_ptr, *string_buffer);

	return VMM_OK;
}
