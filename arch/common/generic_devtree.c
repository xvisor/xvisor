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
 * @file generic_devtree.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief generic arch device tree functions using libfdt library
 */

#include <vmm_error.h>
#include <vmm_host_aspace.h>
#include <vmm_host_ram.h>
#include <libs/stringlib.h>
#include <libs/libfdt.h>
#include <arch_cpu_aspace.h>
#include <arch_devtree.h>
#include <arch_sections.h>
#include <generic_devtree.h>

/*
 * Note: All devtree_<xyz> global variables are initialized by
 * initial page table setup
 */
virtual_addr_t devtree_virt;
virtual_addr_t devtree_virt_base;
physical_addr_t devtree_phys_base;
virtual_size_t devtree_virt_size;

static u32 bank_nr;
static physical_addr_t bank_data[CONFIG_MAX_RAM_BANK_COUNT*2];
static physical_addr_t dt_bank_data[CONFIG_MAX_RAM_BANK_COUNT*2];

struct match_info {
	struct fdt_fileinfo *fdt;
	u32 address_cells;
	u32 size_cells;
	u32 visited_count;
	struct fdt_node_header *visited[CONFIG_MAX_RAM_BANK_COUNT];
};

static int __init match_memory_node(struct fdt_node_header *fdt_node,
				    int level, void *priv)
{
	int rc, i;
	char dev_type[16];
	struct match_info *info = priv;

	if (level == 1) {
		rc = libfdt_get_property(info->fdt, fdt_node,
				info->address_cells, info->size_cells,
				VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME,
				dev_type, sizeof(dev_type));
		if (rc) {
			return 0;
		}

		if (!strncmp(dev_type, "memory", sizeof(dev_type))) {
			for (i = 0; i < info->visited_count; i++) {
				if (info->visited[i] == fdt_node) {
					return 0;
				}
			}
			return 1;
		}
	}

	return 0;
}

int __init arch_devtree_ram_bank_setup(void)
{
	int rc = VMM_OK;
	physical_addr_t tmp;
	struct match_info info;
	struct fdt_fileinfo fdt;
	struct fdt_node_header *fdt_root;
	struct fdt_node_header *fdt_node;
	u32 i, j, address_cells, size_cells;

	if (!devtree_virt_size) {
		return VMM_ENOTAVAIL;
	}

	address_cells = sizeof(physical_addr_t) / sizeof(fdt_cell_t);
	size_cells = sizeof(physical_size_t) / sizeof(fdt_cell_t);

	rc = libfdt_parse_fileinfo(devtree_virt, &fdt);
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

	memset(&info, 0, sizeof(info));
	info.fdt = &fdt;
	info.address_cells = address_cells;
	info.size_cells = size_cells;
	memset(bank_data, 0, sizeof(bank_data));
	j = 0;

	while ((info.visited_count < CONFIG_MAX_RAM_BANK_COUNT) &&
	       (j < array_size(bank_data))) {
		fdt_node = libfdt_find_matching_node(&fdt, match_memory_node,
						     &info);
		if (!fdt_node) {
			break;
		}

		rc = libfdt_get_property(&fdt, fdt_node,
					 address_cells, size_cells,
					 VMM_DEVTREE_ADDR_CELLS_ATTR_NAME,
					 &i, sizeof(i));
		if (!rc) {
			address_cells = i;
		}

		rc = libfdt_get_property(&fdt, fdt_node,
					 address_cells, size_cells,
					 VMM_DEVTREE_SIZE_CELLS_ATTR_NAME,
					 &i, sizeof(i));
		if (!rc) {
			size_cells = i;
		}

		memset(dt_bank_data, 0, sizeof(dt_bank_data));
		rc = libfdt_get_property(&fdt, fdt_node,
					 address_cells, size_cells,
					 VMM_DEVTREE_REG_ATTR_NAME,
					 dt_bank_data, sizeof(dt_bank_data));
		if (rc) {
			continue;
		}

		/* Copy over DT RAM data excluding zero sized RAM banks */
		for (i = 0; i < array_size(dt_bank_data); i += 2) {
			if (dt_bank_data[i + 1] &&
			    (j < array_size(bank_data))) {
				bank_data[j] = dt_bank_data[i];
				bank_data[j + 1] = dt_bank_data[i + 1];
				j += 2;
			}
		}

		info.visited[info.visited_count++] = fdt_node;
	}

	/* Count of RAM banks */
	bank_nr = 0;
	for (i = 0; i < array_size(bank_data); i += 2) {
		if (bank_data[i + 1]) {
			bank_nr++;
		} else {
			break;
		}
	}
	if (!bank_nr) {
		return VMM_OK;
	}

	/* Sort banks based on start address */
	for (i = 0; i < (bank_nr - 1); i++) {
		for (j = i + 1; j < bank_nr; j++) {
			if (bank_data[(2 * i)] > bank_data[(2 * j)]) {
				tmp = bank_data[(2 * i)];
				bank_data[(2 * i)] = bank_data[(2 * j)];
				bank_data[(2 * j)] = tmp;
				tmp = bank_data[(2 * i) + 1];
				bank_data[(2 * i) + 1] =
						bank_data[(2 * j) + 1];
				bank_data[(2 * j) + 1] = tmp;
			}
		}
	}

	return VMM_OK;
}

int __init arch_devtree_ram_bank_count(u32 *bank_count)
{
	*bank_count = bank_nr;

	return VMM_OK;
}

int __init arch_devtree_ram_bank_start(u32 bank, physical_addr_t *addr)
{
	if (bank >= bank_nr) {
		return VMM_EINVALID;
	}

	*addr = bank_data[bank*2];

	return VMM_OK;
}

int __init arch_devtree_ram_bank_size(u32 bank, physical_size_t *size)
{
	if (bank >= bank_nr) {
		return VMM_EINVALID;
	}

	*size = bank_data[bank*2+1];

	return VMM_OK;
}

static bool devtree_reserve_has_fdt(struct fdt_fileinfo *fdt, u32 resv_count)
{
	u32 i;
	int rc;
	u64 adr, sz;

	for (i = 0; i < resv_count; i++) {
		rc = libfdt_reserve_address(fdt, i, &adr);
		if (rc) {
			continue;
		}

		rc = libfdt_reserve_size(fdt, i, &sz);
		if (rc) {
			continue;
		}

		if (adr == devtree_phys_base) {
			return TRUE;
		}
	}

	return FALSE;
}

int __init arch_devtree_reserve_count(u32 *count)
{
	int rc = VMM_OK;
	struct fdt_fileinfo fdt;

	if (!devtree_virt_size) {
		return VMM_ENOTAVAIL;
	}

	rc = libfdt_parse_fileinfo(devtree_virt, &fdt);
	if (rc) {
		return rc;
	}

	*count = libfdt_reserve_count(&fdt);
	if (!devtree_reserve_has_fdt(&fdt, *count)) {
		*count += 1;
	}

	return VMM_OK;
}

int __init arch_devtree_reserve_addr(u32 index, physical_addr_t *addr)
{
	u64 adr;
	u32 count;
	int rc = VMM_OK;
	struct fdt_fileinfo fdt;

	if (!devtree_virt_size) {
		return VMM_ENOTAVAIL;
	}

	rc = libfdt_parse_fileinfo(devtree_virt, &fdt);
	if (rc) {
		return rc;
	}
	count = libfdt_reserve_count(&fdt);

	if (index < count) {
		rc = libfdt_reserve_address(&fdt, index, &adr);
		if (rc) {
			return rc;
		}

		*addr = (physical_addr_t)adr;
	} else if (index == count &&
		   !devtree_reserve_has_fdt(&fdt, count)) {
		*addr = devtree_phys_base;
	} else {
		return VMM_EINVALID;
	}

	return VMM_OK;
}

int __init arch_devtree_reserve_size(u32 index, physical_size_t *size)
{
	u32 count;
	u64 adr, sz;
	int rc = VMM_OK;
	struct fdt_fileinfo fdt;

	if (!devtree_virt_size) {
		return VMM_ENOTAVAIL;
	}

	rc = libfdt_parse_fileinfo(devtree_virt, &fdt);
	if (rc) {
		return rc;
	}
	count = libfdt_reserve_count(&fdt);

	if (index < count) {
		rc = libfdt_reserve_address(&fdt, index, &adr);
		if (rc) {
			return rc;
		}

		rc = libfdt_reserve_size(&fdt, index, &sz);
		if (rc) {
			return rc;
		}

		if (adr == devtree_phys_base) {
			*size = devtree_virt_size;
		} else {
			*size = (physical_size_t)sz;
		}
	} else if (index == count &&
		   !devtree_reserve_has_fdt(&fdt, count)) {
		*size = devtree_virt_size;
	} else {
		return VMM_EINVALID;
	}

	return VMM_OK;
}

int __init arch_devtree_populate(struct vmm_devtree_node **root)
{
	int rc = VMM_OK;
	virtual_addr_t off;
	struct fdt_fileinfo fdt;

	if (!devtree_virt_size) {
		return VMM_ENOTAVAIL;
	}

	rc = libfdt_parse_fileinfo(devtree_virt, &fdt);
	if (rc) {
		return rc;
	}

	rc = libfdt_parse_devtree(&fdt, root, "\0", NULL);
	if (rc) {
		return rc;
	}

	for (off = 0; off < devtree_virt_size; off += VMM_PAGE_SIZE) {
		arch_cpu_aspace_unmap(devtree_virt_base + off);
	}

	return vmm_host_ram_free(devtree_phys_base, devtree_virt_size);
}
