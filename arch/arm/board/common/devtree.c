/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file devtree.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief arch device tree functions using libfdt library
 */

#include <vmm_error.h>
#include <libs/libfdt.h>
#include <arch_devtree.h>

/* Note: dt_blob_start is start of flattend device tree
 * that is linked directly with hypervisor binary
 */
extern u32 dt_blob_start;

int arch_devtree_ram_start(physical_addr_t *addr)
{
	int rc = VMM_OK;
	struct fdt_fileinfo fdt;
	struct fdt_node_header *fdt_root;
	struct fdt_node_header *fdt_node;
	u32 tmp, address_cells, size_cells;
	physical_addr_t data[2];

	address_cells = sizeof(physical_addr_t) / sizeof(fdt_cell_t);
	size_cells = sizeof(physical_size_t) / sizeof(fdt_cell_t);

	rc = libfdt_parse_fileinfo((virtual_addr_t)&dt_blob_start, &fdt);
	if (rc) {
		return rc;
	}

	fdt_root = libfdt_find_node(&fdt,
				    VMM_DEVTREE_PATH_SEPARATOR_STRING);
	if (!fdt_root) {
		return VMM_EFAIL;
	}

	rc = libfdt_get_property(&fdt, fdt_root, address_cells, size_cells,
				 VMM_DEVTREE_ADDR_CELLS_ATTR_NAME,
				 &tmp, sizeof(tmp));
	if (!rc) {
		address_cells = tmp;
	}

	rc = libfdt_get_property(&fdt, fdt_root, address_cells, size_cells,
				 VMM_DEVTREE_SIZE_CELLS_ATTR_NAME,
				 &tmp, sizeof(tmp));
	if (!rc) {
		size_cells = tmp;
	}

	fdt_node = libfdt_find_node(&fdt,
				    VMM_DEVTREE_PATH_SEPARATOR_STRING
				    VMM_DEVTREE_MEMORY_NODE_NAME);
	if (!fdt_node) {
		return VMM_EFAIL;
	}

	rc = libfdt_get_property(&fdt, fdt_node, address_cells, size_cells,
				 VMM_DEVTREE_ADDR_CELLS_ATTR_NAME,
				 &tmp, sizeof(tmp));
	if (!rc) {
		address_cells = tmp;
	}

	rc = libfdt_get_property(&fdt, fdt_node, address_cells, size_cells,
				 VMM_DEVTREE_SIZE_CELLS_ATTR_NAME,
				 &tmp, sizeof(tmp));
	if (!rc) {
		size_cells = tmp;
	}

	rc = libfdt_get_property(&fdt, fdt_node, address_cells, size_cells,
				 VMM_DEVTREE_REG_ATTR_NAME,
				 data, sizeof(data));
	if (rc) {
		return rc;
	}

	*addr = data[0];

	return VMM_OK;
}

int arch_devtree_ram_size(physical_size_t *size)
{
	int rc = VMM_OK;
	struct fdt_fileinfo fdt;
	struct fdt_node_header *fdt_root;
	struct fdt_node_header *fdt_node;
	u32 tmp, address_cells, size_cells;
	physical_size_t data[4] = {0, 0, 0, 0};
	
	address_cells = sizeof(physical_addr_t) / sizeof(fdt_cell_t);
	size_cells = sizeof(physical_size_t) / sizeof(fdt_cell_t);

	rc = libfdt_parse_fileinfo((virtual_addr_t)&dt_blob_start, &fdt);
	if (rc) {
		return rc;
	}

	fdt_root = libfdt_find_node(&fdt,
				    VMM_DEVTREE_PATH_SEPARATOR_STRING);
	if (!fdt_root) {
		return VMM_EFAIL;
	}

	rc = libfdt_get_property(&fdt, fdt_root, address_cells, size_cells,
				 VMM_DEVTREE_ADDR_CELLS_ATTR_NAME,
				 &tmp, sizeof(tmp));
	if (!rc) {
		address_cells = tmp;
	}

	rc = libfdt_get_property(&fdt, fdt_root, address_cells, size_cells,
				 VMM_DEVTREE_SIZE_CELLS_ATTR_NAME,
				 &tmp, sizeof(tmp));
	if (!rc) {
		size_cells = tmp;
	}

	fdt_node = libfdt_find_node(&fdt,
				    VMM_DEVTREE_PATH_SEPARATOR_STRING
				    VMM_DEVTREE_MEMORY_NODE_NAME);
	if (!fdt_node) {
		return VMM_EFAIL;
	}

	rc = libfdt_get_property(&fdt, fdt_node, address_cells, size_cells,
				 VMM_DEVTREE_ADDR_CELLS_ATTR_NAME,
				 &tmp, sizeof(tmp));
	if (!rc) {
		address_cells = tmp;
	}

	rc = libfdt_get_property(&fdt, fdt_node, address_cells, size_cells,
				 VMM_DEVTREE_SIZE_CELLS_ATTR_NAME,
				 &tmp, sizeof(tmp));
	if (!rc) {
		size_cells = tmp;
	}

	rc = libfdt_get_property(&fdt, fdt_node, address_cells, size_cells,
				 VMM_DEVTREE_REG_ATTR_NAME,
				 data, sizeof(data));
	if (rc) {
		return rc;
	}

	*size = data[1];

	if (data[2] == (data[0] + data[1])) {
		*size += data[3];
	}

	return VMM_OK;
}

int arch_devtree_reserve_count(u32 *count)
{
	int rc = VMM_OK;
	struct fdt_fileinfo fdt;

	rc = libfdt_parse_fileinfo((virtual_addr_t)&dt_blob_start, &fdt);
	if (rc) {
		return rc;
	}

	*count = libfdt_reserve_count(&fdt);

	return VMM_OK;
}

int arch_devtree_reserve_addr(u32 index, physical_addr_t *addr)
{
	u64 tmp;
	int rc = VMM_OK;
	struct fdt_fileinfo fdt;

	rc = libfdt_parse_fileinfo((virtual_addr_t)&dt_blob_start, &fdt);
	if (rc) {
		return rc;
	}

	rc = libfdt_reserve_address(&fdt, index, &tmp);
	if (rc) {
		return rc;
	}

	*addr = (physical_addr_t)tmp;

	return VMM_OK;
}

int arch_devtree_reserve_size(u32 index, physical_size_t *size)
{
	u64 tmp;
	int rc = VMM_OK;
	struct fdt_fileinfo fdt;

	rc = libfdt_parse_fileinfo((virtual_addr_t)&dt_blob_start, &fdt);
	if (rc) {
		return rc;
	}

	rc = libfdt_reserve_size(&fdt, index, &tmp);
	if (rc) {
		return rc;
	}

	*size = (physical_size_t)tmp;

	return VMM_OK;
}

int arch_devtree_populate(struct vmm_devtree_node **root)
{
	int rc = VMM_OK;
	struct fdt_fileinfo fdt;
	
	rc = libfdt_parse_fileinfo((virtual_addr_t)&dt_blob_start, &fdt);
	if (rc) {
		return rc;
	}

	return libfdt_parse_devtree(&fdt, root, "\0", NULL);
}

