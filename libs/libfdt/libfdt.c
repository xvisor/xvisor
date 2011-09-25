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
 * @file libfdt.c
 * @version 0.1
 * @author Anup Patel. (anup@brainfault.org)
 * @brief Flattend device tree library source
 */

#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_heap.h>
#include <vmm_devtree.h>
#include <libfdt.h>

#define LIBFDT_DATA32(ptr)	(*((u32*)ptr))

int libfdt_parse_fileinfo(virtual_addr_t fdt_addr, 
			  fdt_fileinfo_t * fdt)
{
	struct fdt_header *header;

	/* Sanity check */
	if (!fdt) {
		return VMM_EFAIL;
	}

	/* Retrive header */
	header = (struct fdt_header *)fdt_addr;

	/* Check magic number of header for sainity */
	if (header->magic != FDT_MAGIC) {
		return VMM_EFAIL;
	}
	fdt->header = header;

	/* Compute data location & size */
	fdt->data_ptr = (char *)fdt_addr;
	fdt->data_ptr += sizeof(fdt_header_t);
	fdt->data_ptr += sizeof(fdt_reserve_entry_t);
	fdt->data_size = header->size_dt_struct;

	/* Compute strings location & size */
	fdt->str_ptr = fdt->data_ptr + fdt->data_size;
	fdt->str_size = header->size_dt_strings;

	return VMM_OK;
}

static void libfdt_parse_devtree_recursive(vmm_devtree_node_t * node,
					   char **data_ptr, 
					   char *str_buf)
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
	while ((u32) (*data_ptr) % sizeof(u32) != 0) {
		(*data_ptr)++;
	}

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
			libfdt_parse_devtree_recursive(child, data_ptr, str_buf);
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

int libfdt_parse_devtree(fdt_fileinfo_t * fdt,
			 vmm_devtree_node_t ** root,
			 char **string_buffer, 
			 size_t * string_buffer_size)
{
	char *data_ptr;

	/* Sanity check */
	if (!fdt) {
		return VMM_EFAIL;
	}

	/* Allocate string buffer */
	*string_buffer = vmm_malloc(fdt->str_size);
	vmm_memcpy(*string_buffer, fdt->str_ptr, fdt->str_size);
	*string_buffer_size = fdt->str_size;

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
	data_ptr = fdt->data_ptr;
	libfdt_parse_devtree_recursive(*root, &data_ptr, *string_buffer);

	return VMM_OK;
}

static fdt_node_header_t * libfdt_find_node_recursive(char **data_ptr, 
						      char *str_buf, 
						      const char * node_path)
{
	fdt_node_header_t * ret = NULL;
	u32 i, valid, len = 0x0;

	while ((*node_path == ' ') || 
	       (*node_path == '\t') ||
	       (*node_path == '\r') ||
	       (*node_path == '\n')) {
		node_path++;
	}

	if (LIBFDT_DATA32(*data_ptr) != FDT_BEGIN_NODE)
		return NULL;

	*data_ptr += sizeof(u32);

	len = vmm_strlen(*data_ptr);
	valid = 1;
	for (i = 0; i < len; i++) {
		if (!node_path[i]) {
			valid = 0;
			break;
		}
		if ((*data_ptr)[i] != node_path[i]) {
			valid = 0;
			break;
		}
	}
	if (valid) {
		node_path += len;

		if (*node_path == '/') {
			node_path++;
		}

		while ((*node_path == ' ') || 
		       (*node_path == '\t') ||
		       (*node_path == '\r') ||
		       (*node_path == '\n')) {
			node_path++;
		}

		if (*node_path == '\0') {
			*data_ptr -= sizeof(u32);
			return (fdt_node_header_t *)(*data_ptr);
		}
	}
	*data_ptr += len + 1;
	while ((u32) (*data_ptr) % sizeof(u32) != 0) {
		(*data_ptr)++;
	}

	while (LIBFDT_DATA32(*data_ptr) != FDT_END_NODE) {
		switch (LIBFDT_DATA32(*data_ptr)) {
		case FDT_PROP:
			*data_ptr += sizeof(u32);
			len = LIBFDT_DATA32(*data_ptr);
			*data_ptr += sizeof(u32);
			*data_ptr += sizeof(u32);
			*data_ptr += len;
			while ((u32) (*data_ptr) % sizeof(u32) != 0) {
				(*data_ptr)++;
			}
			break;
		case FDT_NOP:
			*data_ptr += sizeof(u32);
			break;
		case FDT_BEGIN_NODE:
			ret = libfdt_find_node_recursive(data_ptr, 
							 str_buf, 
							 node_path);
			if (ret) {
				return ret;
			}
			break;
		default:
			return NULL;
			break;
		};
	}

	*data_ptr += sizeof(u32);

	return NULL;
}

fdt_node_header_t * libfdt_find_node(fdt_fileinfo_t * fdt, 
				     const char * node_path)
{
	char * data_ptr = NULL;

	/* Sanity checks */
	if (!fdt || !node_path) {
		return NULL;
	}

	/* Find the FDT node recursively */
	data_ptr = fdt->data_ptr;
	return libfdt_find_node_recursive(&data_ptr, fdt->str_ptr, node_path);
}

fdt_property_t * libfdt_get_property(fdt_fileinfo_t * fdt, 
				     fdt_node_header_t * fdt_node, 
				     const char * property)
{
	u32 len = 0x0;
	fdt_property_t * ret = NULL;
	char * data_ptr = NULL;

	/* Sanity checks */
	if (!fdt || !fdt_node || !property) {
		return NULL;
	}

	/* Sanity checks */
	if (fdt_node->tag != FDT_BEGIN_NODE)
		return NULL;

	/* Convert node to character stream */
	data_ptr = (char *)fdt_node;
	data_ptr += sizeof(u32);

	/* Skip node name */
	len = vmm_strlen(data_ptr);
	data_ptr += len + 1;
	while ((u32) (data_ptr) % sizeof(u32) != 0) {
		data_ptr++;
	}

	/* Find node property and its value */
	ret = NULL;
	while (LIBFDT_DATA32(data_ptr) == FDT_PROP) {
		data_ptr += sizeof(u32);
		len = LIBFDT_DATA32(data_ptr);
		data_ptr += sizeof(u32);
		if (!vmm_strcmp(&fdt->str_ptr[LIBFDT_DATA32(data_ptr)], 
				property)) {
			data_ptr -= sizeof(u32) * 2;
			ret = (fdt_property_t *)data_ptr;
			break;
		}
		data_ptr += sizeof(u32);
		data_ptr += len;
		while ((u32) (data_ptr) % sizeof(u32) != 0) {
			(data_ptr)++;
		}
	}

	return ret;
}

