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

/* Note: For all ARM boards we support upto 8 RAM banks */
static u32 bank_nr;
static physical_addr_t bank_data[16];

int arch_devtree_ram_bank_setup(void)
{
	int rc = VMM_OK;
	physical_addr_t tmp;
	struct fdt_fileinfo fdt;
	struct fdt_node_header *fdt_root;
	struct fdt_node_header *fdt_node;
	u32 i, j, address_cells, size_cells;

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
				 &i, sizeof(i));
	if (!rc) {
		address_cells = i;
	}

	rc = libfdt_get_property(&fdt, fdt_root, address_cells, size_cells,
				 VMM_DEVTREE_SIZE_CELLS_ATTR_NAME,
				 &i, sizeof(i));
	if (!rc) {
		size_cells = i;
	}

	fdt_node = libfdt_find_node(&fdt,
				    VMM_DEVTREE_PATH_SEPARATOR_STRING
				    VMM_DEVTREE_MEMORY_NODE_NAME);
	if (!fdt_node) {
		return VMM_EFAIL;
	}

	rc = libfdt_get_property(&fdt, fdt_node, address_cells, size_cells,
				 VMM_DEVTREE_ADDR_CELLS_ATTR_NAME,
				 &i, sizeof(i));
	if (!rc) {
		address_cells = i;
	}

	rc = libfdt_get_property(&fdt, fdt_node, address_cells, size_cells,
				 VMM_DEVTREE_SIZE_CELLS_ATTR_NAME,
				 &i, sizeof(i));
	if (!rc) {
		size_cells = i;
	}

	rc = libfdt_get_property(&fdt, fdt_node, address_cells, size_cells,
				 VMM_DEVTREE_REG_ATTR_NAME,
				 bank_data, sizeof(bank_data));
	if (rc) {
		return rc;
	}

	/* Count of RAM banks */
	bank_nr = 0;
	for (i = 0; i < array_size(bank_data); i += 2) {
		if (bank_data[i+1]) {
			bank_nr++;
		} else {
			break;
		}
	}
	if (bank_nr < 2) {
		return VMM_OK;
	}

	/* Sort banks based on start address */
	for (i = 0; i < (bank_nr - 1); i++) {
		for (j = i+1; j < bank_nr; j++) {
			if (bank_data[(2*i)] > bank_data[(2*j)]) {
				tmp = bank_data[(2*i)];
				bank_data[(2*i)] = bank_data[(2*j)];
				bank_data[(2*j)] = tmp;
				tmp = bank_data[(2*i)+1];
				bank_data[(2*i)+1] = bank_data[(2*j)+1];
				bank_data[(2*j)+1] = tmp;
			}
		}
	}

	/* Merge consecutive banks */
	for (i = 1; i < bank_nr; i++) {
		if (bank_data[(2*i)] !=
			(bank_data[(2*(i-1))] + bank_data[(2*(i-1))+1])) {
			continue;
		}
		bank_data[(2*(i-1))+1] += bank_data[(2*i)+1];
		bank_data[(2*i)] = 0;
		bank_data[(2*i)+1] = 0;
		bank_nr--;
		for (j = i; j < bank_nr; j++) {
			bank_data[(2*j)] = bank_data[(2*(j+1))];
			bank_data[(2*j)+1] = bank_data[(2*(j+1))+1];
			bank_data[(2*(j+1))] = 0;
			bank_data[(2*(j+1))+1] = 0;
		}
	}

	return VMM_OK;
}

int arch_devtree_ram_bank_count(u32 *bank_count)
{
	*bank_count = bank_nr;

	return VMM_OK;
}

int arch_devtree_ram_bank_start(u32 bank, physical_addr_t *addr)
{
	if (bank >= bank_nr) {
		return VMM_EINVALID;
	}

	*addr = bank_data[bank*2];

	return VMM_OK;
}

int arch_devtree_ram_bank_size(u32 bank, physical_size_t *size)
{
	if (bank >= bank_nr) {
		return VMM_EINVALID;
	}

	*size = bank_data[bank*2+1];

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

