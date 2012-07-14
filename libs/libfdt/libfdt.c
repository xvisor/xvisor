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
 * @author Anup Patel. (anup@brainfault.org)
 * @brief Flattend device tree library source
 */

#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_devtree.h>
#include <vmm_host_io.h>
#include <mathlib.h>
#include <libfdt.h>

#define LIBFDT_DATA32(ptr)	vmm_be32_to_cpu(*((u32*)ptr))
#define LIBFDT_DATA64(ptr)	vmm_be64_to_cpu(*((u64*)ptr))

static void libfdt_fix_literal_val(u32 prop_type,
				   void *prop_val, 
				   u32 prop_len)
{
	int len;
	u32 pos, lsz, val32;
	u64 val64;

	if (!vmm_devtree_isliteral(prop_type)) {
		return;
	}

	lsz = vmm_devtree_literal_size(prop_type);
	if (lsz == 4) {
		pos = 0;
		len = prop_len;
		while (len > 0) {
			if (len < 4) {
				break;
			}
			val32 = ((u32 *)prop_val)[pos];
			((u32 *)prop_val)[pos] = LIBFDT_DATA32(&val32);
			pos++;
			len -= 4;
		}
	} else if (lsz == 8) {
		pos = 0;
		len = prop_len;
		while (len > 0) {
			if (len < 4) {
				break;
			}
			if (len == 4) {
				((u32 *)prop_val)[pos*2] =
				LIBFDT_DATA32(&((u32 *)prop_val)[pos*2]);
				break;
			}
			val64 = ((u64 *)prop_val)[pos];
			((u64 *)prop_val)[pos] = LIBFDT_DATA64(&val64);
			pos++;
			len -= 8;
		}
	}
}

int libfdt_parse_fileinfo(virtual_addr_t fdt_addr, 
			  struct fdt_fileinfo *fdt)
{
	/* Sanity check */
	if (!fdt) {
		return VMM_EFAIL;
	}

	/* Retrive header */
	vmm_memcpy(&fdt->header, (void *)fdt_addr, sizeof(fdt->header));
	fdt->header.magic = LIBFDT_DATA32(&fdt->header.magic);
	fdt->header.totalsize = LIBFDT_DATA32(&fdt->header.totalsize);
	fdt->header.off_dt_struct = LIBFDT_DATA32(&fdt->header.off_dt_struct);
	fdt->header.off_dt_strings = LIBFDT_DATA32(&fdt->header.off_dt_strings);
	fdt->header.off_mem_rsvmap = LIBFDT_DATA32(&fdt->header.off_mem_rsvmap);
	fdt->header.version = LIBFDT_DATA32(&fdt->header.version);
	fdt->header.last_comp_version = LIBFDT_DATA32(&fdt->header.last_comp_version);
	fdt->header.boot_cpuid_phys = LIBFDT_DATA32(&fdt->header.boot_cpuid_phys);
	fdt->header.size_dt_strings = LIBFDT_DATA32(&fdt->header.size_dt_strings);
	fdt->header.size_dt_struct = LIBFDT_DATA32(&fdt->header.size_dt_struct);

	/* Check magic number of header for sainity */
	if (fdt->header.magic != FDT_MAGIC) {
		return VMM_EFAIL;
	}

	/* Compute data location & size */
	fdt->data = (char *)fdt_addr;
	fdt->data += sizeof(struct fdt_header);
	fdt->data += sizeof(struct fdt_reserve_entry);
	fdt->data_size = fdt->header.size_dt_struct;

	/* Compute strings location & size */
	fdt->str = fdt->data + fdt->data_size;
	fdt->str_size = fdt->header.size_dt_strings;

	return VMM_OK;
}

static void libfdt_parse_devtree_recursive(struct fdt_fileinfo *fdt,
					   struct vmm_devtree_node *node,
					   char **data)
{
	u32 type, len;
	const char * name;
	struct vmm_devtree_node *child;
	struct vmm_devtree_attr *attr;

	if (!fdt || !node) {
		return;
	}

	while (LIBFDT_DATA32(*data) != FDT_END_NODE) {
		switch (LIBFDT_DATA32(*data)) {
		case FDT_PROP:
			*data += sizeof(fdt_cell_t);
			len = LIBFDT_DATA32(*data);
			*data += sizeof(fdt_cell_t);
			name = &fdt->str[LIBFDT_DATA32(*data)];
			*data += sizeof(fdt_cell_t);
			type = vmm_devtree_estimate_attrtype(name);
			vmm_devtree_setattr(node, name, *data, type, len);
			*data += len;
			while ((virtual_addr_t) (*data) % sizeof(fdt_cell_t) != 0)
				(*data)++;
			attr = vmm_devtree_getattr(node, name);
			libfdt_fix_literal_val(type, attr->value, len);
			break;
		case FDT_NOP:
			*data += sizeof(fdt_cell_t);
			break;
		case FDT_BEGIN_NODE:
			*data += sizeof(fdt_cell_t);
			type = VMM_DEVTREE_NODETYPE_UNKNOWN;
			child = vmm_devtree_addnode(node, *data, type, NULL);
			*data += vmm_strlen(*data) + 1;
			while ((virtual_addr_t) (*data) % sizeof(fdt_cell_t) != 0) {
				(*data)++;
			}
			libfdt_parse_devtree_recursive(fdt, child, data);
			break;
		default:
			return;
			break;
		};
	}

	*data += sizeof(fdt_cell_t);

	return;
}

int libfdt_parse_devtree(struct fdt_fileinfo * fdt,
			 struct vmm_devtree_node ** root)
{
	char *data;

	/* Sanity check */
	if (!fdt) {
		return VMM_EFAIL;
	}

	/* Get data pointer */
	data = fdt->data;

	/* Sanity check */
	if (LIBFDT_DATA32(data) != FDT_BEGIN_NODE)
		return VMM_EFAIL;

	/* Point to root node name */
	data += sizeof(fdt_cell_t);

	/* Create root node */
	*root = vmm_devtree_addnode(NULL, data, 
				    VMM_DEVTREE_NODETYPE_UNKNOWN, NULL);

	/* Skip root node name */
	data += vmm_strlen(data) + 1;
	while ((virtual_addr_t) (data) % sizeof(fdt_cell_t) != 0) {
		(data)++;
	}

	/* Parse FDT recursively */
	libfdt_parse_devtree_recursive(fdt, *root, &data);

	return VMM_OK;
}

static struct fdt_node_header * libfdt_find_node_recursive(char **data, 
							   char *str, 
							   const char * node_path)
{
	struct fdt_node_header * ret = NULL;
	u32 i, valid, len = 0x0;

	while ((*node_path == ' ') || 
	       (*node_path == '\t') ||
	       (*node_path == '\r') ||
	       (*node_path == '\n')) {
		node_path++;
	}

	if (LIBFDT_DATA32(*data) != FDT_BEGIN_NODE)
		return NULL;

	*data += sizeof(fdt_cell_t);

	len = vmm_strlen(*data);
	valid = 1;
	for (i = 0; i < len; i++) {
		if (!node_path[i]) {
			valid = 0;
			break;
		}
		if ((*data)[i] != node_path[i]) {
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
			*data -= sizeof(fdt_cell_t);
			return (struct fdt_node_header *)(*data);
		}
	}
	*data += len + 1;
	while ((virtual_addr_t) (*data) % sizeof(fdt_cell_t) != 0) {
		(*data)++;
	}

	while (LIBFDT_DATA32(*data) != FDT_END_NODE) {
		switch (LIBFDT_DATA32(*data)) {
		case FDT_PROP:
			*data += sizeof(fdt_cell_t);
			len = LIBFDT_DATA32(*data);
			*data += sizeof(fdt_cell_t);
			*data += sizeof(fdt_cell_t);
			*data += len;
			while ((virtual_addr_t) (*data) % sizeof(fdt_cell_t) != 0) {
				(*data)++;
			}
			break;
		case FDT_NOP:
			*data += sizeof(fdt_cell_t);
			break;
		case FDT_BEGIN_NODE:
			ret = libfdt_find_node_recursive(data, str, node_path);
			if (ret) {
				return ret;
			}
			break;
		default:
			return NULL;
			break;
		};
	}

	*data += sizeof(fdt_cell_t);

	return NULL;
}

struct fdt_node_header * libfdt_find_node(struct fdt_fileinfo * fdt, 
					  const char * node_path)
{
	char * data = NULL;

	/* Sanity checks */
	if (!fdt || !node_path) {
		return NULL;
	}

	/* Find the FDT node recursively */
	data = fdt->data;
	return libfdt_find_node_recursive(&data, fdt->str, node_path);
}

int libfdt_get_property(struct fdt_fileinfo *fdt, 
			struct fdt_node_header *fdt_node, 
			const char * property,
			void *property_value)
{
	u32 type, len = 0x0;
	struct fdt_property * ret = NULL;
	char * data = NULL;

	/* Sanity checks */
	if (!fdt || !fdt_node || !property || !property_value) {
		return VMM_EFAIL;
	}

	/* Sanity checks */
	if (LIBFDT_DATA32(&fdt_node->tag) != FDT_BEGIN_NODE) {
		return VMM_EFAIL;
	}

	/* Convert node to character stream */
	data = (char *)fdt_node;
	data += sizeof(fdt_cell_t);

	/* Skip node name */
	len = vmm_strlen(data);
	data += len + 1;
	while ((virtual_addr_t) (data) % sizeof(fdt_cell_t) != 0) {
		data++;
	}

	/* Find node property and its value */
	ret = NULL;
	while (LIBFDT_DATA32(data) == FDT_PROP) {
		data += sizeof(fdt_cell_t);
		len = LIBFDT_DATA32(data);
		data += sizeof(fdt_cell_t);
		if (!vmm_strcmp(&fdt->str[LIBFDT_DATA32(data)], 
				property)) {
			data -= sizeof(fdt_cell_t) * 2;
			ret = (struct fdt_property *)data;
			break;
		}
		data += sizeof(fdt_cell_t);
		data += len;
		while ((virtual_addr_t) (data) % sizeof(fdt_cell_t) != 0) {
			(data)++;
		}
	}

	if (!ret) {
		return VMM_EFAIL;
	}

	vmm_memcpy(property_value, &ret->data[0], len);

	type = vmm_devtree_estimate_attrtype(property);
	libfdt_fix_literal_val(type, property_value, len);

	return VMM_OK;
}

