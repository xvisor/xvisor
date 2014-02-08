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
#include <vmm_heap.h>
#include <vmm_host_io.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <libs/libfdt.h>

#define LIBFDT_DATA32(ptr)	vmm_be32_to_cpu(*((u32*)ptr))
#define LIBFDT_DATA64(ptr)	vmm_be64_to_cpu(*((u64*)ptr))

static u32 libfdt_property_len(const char *prop, 
				u32 address_cells, u32 size_cells, u32 len)
{
	u32 lsz, type, reg_cells, reg_count;

	/* Special way of handling 'reg' property */
	if (strcmp(prop, "reg") == 0) {
		reg_cells = len / sizeof(fdt_cell_t);
		reg_count = udiv32(reg_cells, address_cells + size_cells);
		if (umod32(reg_cells, address_cells + size_cells)) {
			reg_count++;
		}
		reg_cells = sizeof(physical_addr_t) / sizeof(fdt_cell_t);
		reg_cells += sizeof(physical_size_t) / sizeof(fdt_cell_t);
		reg_count = reg_count * (reg_cells * sizeof(fdt_cell_t));
		return reg_count;
	}

	type = vmm_devtree_estimate_attrtype(prop);

	/* Special way of handling non-literal property */
	if (!vmm_devtree_isliteral(type)) {
		return len;
	}

	/* Special way of handling literal property */
	lsz = vmm_devtree_literal_size(type);
	if (umod32(len, lsz)) {
		return udiv32(len, lsz) * lsz + lsz;
	}

	return len;
}

static void libfdt_property_read(const char *prop, void *dst, void *src,
				 u32 address_cells, u32 size_cells, u32 len)
{
	int tlen;
	u32 pos, type, lsz, val32, reg, reg_cells, reg_count;
	u64 val64;

	/* Special way of handling 'reg' property */
	if (strcmp(prop, "reg") == 0) {
		reg_cells = len / sizeof(fdt_cell_t);
		reg_count = udiv32(reg_cells, address_cells + size_cells);
		if (umod32(reg_cells, address_cells + size_cells)) {
			reg_count++;
		}
		for (reg = 0; reg < reg_count; reg++) {
			if (address_cells == 2) {
				*((physical_addr_t *)dst) = 
					(physical_addr_t)LIBFDT_DATA64(src);
			} else {
				*((physical_addr_t *)dst) = 
					(physical_addr_t)LIBFDT_DATA32(src);
			}
			dst += sizeof(physical_addr_t);
			src += address_cells * sizeof(fdt_cell_t);
			if (size_cells == 2) {
				*((physical_size_t *)dst) = 
					(physical_size_t)LIBFDT_DATA64(src);
			} else {
				*((physical_size_t *)dst) = 
					(physical_size_t)LIBFDT_DATA32(src);
			}
			dst += sizeof(physical_size_t);
			src += size_cells * sizeof(fdt_cell_t);
		}
		return;
	}

	type = vmm_devtree_estimate_attrtype(prop);

	/* Special way of handling non-literal property */
	if (!vmm_devtree_isliteral(type)) {
		memcpy(dst, src, len);
		return;
	}

	/* Special way of handling literal property */
	lsz = vmm_devtree_literal_size(type);
	if (lsz == 4) {
		pos = 0;
		tlen = len;
		while (tlen > 0) {
			if (tlen < 4) {
				break;
			}
			val32 = ((u32 *)src)[pos];
			((u32 *)dst)[pos] = LIBFDT_DATA32(&val32);
			pos++;
			tlen -= 4;
		}
	} else if (lsz == 8) {
		pos = 0;
		tlen = len;
		while (tlen > 0) {
			if (tlen < 4) {
				break;
			} else if (tlen == 4) {
				((u64 *)dst)[pos] =
				LIBFDT_DATA32(&((u32 *)src)[pos*2]);
				break;
			}
			val64 = ((u64 *)src)[pos];
			((u64 *)dst)[pos] = LIBFDT_DATA64(&val64);
			pos++;
			tlen -= 8;
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
	memcpy(&fdt->header, (void *)fdt_addr, sizeof(fdt->header));
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
	void *val;
	const void *aval;
	const char *name;
	u32 type, len, alen, addr_cells, size_cells;
	struct vmm_devtree_node *child;

	if (!fdt || !node) {
		return;
	}

	aval = vmm_devtree_attrval(node->parent, "#address-cells");
	if (aval) {
		addr_cells = *((const u32 *)aval);
	} else {
		addr_cells = sizeof(physical_addr_t) / sizeof(fdt_cell_t);
	}
	aval = vmm_devtree_attrval(node->parent, "#size-cells");
	if (aval) {
		size_cells = *((const u32 *)aval);
	} else {
		size_cells = sizeof(physical_size_t) / sizeof(fdt_cell_t);
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
			alen = libfdt_property_len(name, addr_cells, 
						   size_cells, len);
			val = vmm_zalloc(alen);
			libfdt_property_read(name, val, *data,
					     addr_cells, size_cells, len);
			vmm_devtree_setattr(node, name, val, type, alen);
			vmm_free(val);
			*data += len;
			while ((virtual_addr_t) (*data) % sizeof(fdt_cell_t) != 0)
				(*data)++;
			break;
		case FDT_NOP:
			*data += sizeof(fdt_cell_t);
			break;
		case FDT_BEGIN_NODE:
			*data += sizeof(fdt_cell_t);
			child = vmm_devtree_addnode(node, *data);
			*data += strlen(*data) + 1;
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

int libfdt_parse_devtree(struct fdt_fileinfo *fdt,
			 struct vmm_devtree_node **root)
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
	*root = vmm_devtree_addnode(NULL, data);

	/* Skip root node name */
	data += strlen(data) + 1;
	while ((virtual_addr_t) (data) % sizeof(fdt_cell_t) != 0) {
		(data)++;
	}

	/* Parse FDT recursively */
	libfdt_parse_devtree_recursive(fdt, *root, &data);

	return VMM_OK;
}

static struct fdt_node_header *libfdt_find_node_recursive(char **data, 
							  char *str, 
							  const char *node_path)
{
	u32 i, valid, len = 0x0;
	struct fdt_node_header *ret = NULL;

	while ((*node_path == ' ') || 
	       (*node_path == '\t') ||
	       (*node_path == '\r') ||
	       (*node_path == '\n')) {
		node_path++;
	}

	if (LIBFDT_DATA32(*data) != FDT_BEGIN_NODE)
		return NULL;

	*data += sizeof(fdt_cell_t);

	len = strlen(*data);
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
	} else {
		/* Skip the entire node by looking for matching FDT_END_NODE */
		*data += len + 1;
		while ((virtual_addr_t) (*data) % sizeof(fdt_cell_t) != 0) {
			(*data)++;
		}

		valid = 1;
		while (valid) {
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
				*data += sizeof(fdt_cell_t);
				len = strlen(*data);
				*data += len + 1;
				while ((virtual_addr_t) (*data) % sizeof(fdt_cell_t) != 0) {
					(*data)++;
				}
				valid++;
				break;
			case FDT_END_NODE:
				*data += sizeof(fdt_cell_t);
				valid--;
				break;
			default:
				return NULL;
				break;
			};
		}
	}

	return NULL;
}

struct fdt_node_header *libfdt_find_node(struct fdt_fileinfo *fdt, 
					 const char *node_path)
{
	char *data = NULL;

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
			const char *property,
			void *property_value)
{
	u32 len = 0x0;
	struct fdt_property *ret = NULL;
	char *data = NULL;

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
	len = strlen(data);
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
		if (!strcmp(&fdt->str[LIBFDT_DATA32(data)], 
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

	libfdt_property_read(property, property_value, &ret->data[0],
		sizeof(physical_addr_t) / 4, sizeof(physical_size_t) / 4, len);

	return VMM_OK;
}

