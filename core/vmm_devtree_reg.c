/**
 * Copyright (c) 2015 Anup Patel.
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
 * @file vmm_devtree_reg.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Host registers related device tree functions
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_resource.h>
#include <vmm_host_ram.h>
#include <vmm_host_vapool.h>
#include <vmm_host_aspace.h>
#include <vmm_devtree.h>
#include <libs/stringlib.h>

static int devtree_get_regcells(struct vmm_devtree_node *node,
				u32 *addr_cells_p, u32 *size_cells_p)
{
	u32 addr_cells, size_cells;
	struct vmm_devtree_node *np;

	addr_cells = sizeof(physical_addr_t) / sizeof(u32);
	size_cells = sizeof(physical_size_t) / sizeof(u32);

	np = node->parent;
	while (np && vmm_devtree_read_u32(np,
			VMM_DEVTREE_ADDR_CELLS_ATTR_NAME, &addr_cells)) {
		np = node->parent;
	}

	np = node->parent;
	while (np && vmm_devtree_read_u32(np,
			VMM_DEVTREE_SIZE_CELLS_ATTR_NAME, &size_cells)) {
		np = node->parent;
	}

	if ((2 < addr_cells) || (2 < size_cells)) {
		return VMM_EINVALID;
	}

	if (addr_cells_p) {
		*addr_cells_p = addr_cells;
	}

	if (size_cells_p) {
		*size_cells_p = size_cells;
	}

	return VMM_OK;
}

static void devtree_map_regaddr(struct vmm_devtree_node *node,
				physical_addr_t addr,
				physical_addr_t *map_addr)
{
	int rc;
	u32 start, end, c[2] = { 0, 0 };
	u32 addr_cells, size_cells;
	u32 n_addr_cells, n_size_cells;
	physical_addr_t in_addr, out_addr;
	physical_size_t in_size;
	struct vmm_devtree_node *np;

	if (!node) {
		goto done;
	}

	np = node->parent;
	while (np) {
		if (!vmm_devtree_getattr(np,
					 VMM_DEVTREE_RANGES_ATTR_NAME)) {
			goto skip;
		}

		rc = vmm_devtree_read_u32(np,
			VMM_DEVTREE_ADDR_CELLS_ATTR_NAME, &addr_cells);
		if (rc) {
			goto skip;
		}

		rc = vmm_devtree_read_u32(np,
			VMM_DEVTREE_SIZE_CELLS_ATTR_NAME, &size_cells);
		if (rc) {
			goto skip;
		}

		if ((addr_cells < 1) || (size_cells < 1)) {
			goto done;
		}

		rc = devtree_get_regcells(np, &n_addr_cells, &n_size_cells);
		if (rc) {
			goto skip;
		}

		if ((n_addr_cells < 1) || (n_size_cells < 1)) {
			goto done;
		}

		start = 0;
		end = vmm_devtree_attrlen(np, VMM_DEVTREE_RANGES_ATTR_NAME);
		end = end / sizeof(u32);
		while (start < end) {
			rc = vmm_devtree_read_u32_atindex(np,
						VMM_DEVTREE_RANGES_ATTR_NAME,
						&c[0], start);
			start++;
			if (rc) {
				continue;
			}

			if (addr_cells == 2) {
				rc = vmm_devtree_read_u32_atindex(np,
						VMM_DEVTREE_RANGES_ATTR_NAME,
						&c[1], start);
				start++;
				if (rc) {
					continue;
				}
				in_addr = ((u64)c[0] << 32) | (u64)c[1];
			} else {
				in_addr = c[0];
			}

			rc = vmm_devtree_read_u32_atindex(np,
						VMM_DEVTREE_RANGES_ATTR_NAME,
						&c[0], start);
			start++;
			if (rc) {
				continue;
			}

			if (n_addr_cells == 2) {
				rc = vmm_devtree_read_u32_atindex(np,
						VMM_DEVTREE_RANGES_ATTR_NAME,
						&c[1], start);
				start++;
				if (rc) {
					continue;
				}
				out_addr = ((u64)c[0] << 32) | (u64)c[1];
			} else {
				out_addr = c[0];
			}

			rc = vmm_devtree_read_u32_atindex(np,
						VMM_DEVTREE_RANGES_ATTR_NAME,
						&c[0], start);
			start++;
			if (rc) {
				continue;
			}

			if (size_cells == 2) {
				rc = vmm_devtree_read_u32_atindex(np,
						VMM_DEVTREE_RANGES_ATTR_NAME,
						&c[1], start);
				start++;
				if (rc) {
					continue;
				}
				in_size = ((u64)c[0] << 32) | (u64)c[1];
			} else {
				in_size = c[0];
			}

			if (in_addr <= addr && addr < (in_addr + in_size)) {
				addr = out_addr + (addr - in_addr);
			}
		}

skip:
		np = np->parent;
	}

done:
	if (map_addr) {
		*map_addr = addr;
	}
}

int vmm_devtree_regsize(struct vmm_devtree_node *node,
		        physical_size_t *size, int regset)
{
	int rc;
	u32 start, addr_cells, size_cells, cells[2] = { 0, 0 };

	if (!node || !size || regset < 0) {
		return VMM_EFAIL;
	}

	if (vmm_devtree_getattr(node, VMM_DEVTREE_VIRTUAL_REG_ATTR_NAME)) {
		return VMM_ENOTAVAIL;
	}

	rc = devtree_get_regcells(node, &addr_cells, &size_cells);
	if (rc) {
		return rc;
	}

	if (size_cells < 1) {
		return VMM_EINVALID;
	}

	start = regset * (addr_cells + size_cells) + addr_cells;

	rc = vmm_devtree_read_u32_atindex(node,
			VMM_DEVTREE_REG_ATTR_NAME, &cells[0], start);
	if (rc) {
		return rc;
	}

	if (size_cells == 2) {
		rc = vmm_devtree_read_u32_atindex(node,
			VMM_DEVTREE_REG_ATTR_NAME, &cells[1], start+1);
		if (rc) {
			return rc;
		}
	}

	if (size_cells == 2) {
		*size = ((u64)cells[0] << 32) | (u64)cells[1];
	} else {
		*size = cells[0];
	}

	return VMM_OK;
}

int vmm_devtree_regaddr(struct vmm_devtree_node *node,
		        physical_addr_t *addr, int regset)
{
	int rc;
	u32 start, addr_cells, size_cells, cells[2] = { 0, 0 };

	if (!node || !addr || regset < 0) {
		return VMM_EFAIL;
	}

	if (vmm_devtree_getattr(node, VMM_DEVTREE_VIRTUAL_REG_ATTR_NAME)) {
		return VMM_ENOTAVAIL;
	}

	rc = devtree_get_regcells(node, &addr_cells, &size_cells);
	if (rc) {
		return rc;
	}

	if (addr_cells < 1) {
		return VMM_EINVALID;
	}

	start = regset * (addr_cells + size_cells);

	rc = vmm_devtree_read_u32_atindex(node,
			VMM_DEVTREE_REG_ATTR_NAME, &cells[0], start);
	if (rc) {
		return rc;
	}

	if (addr_cells == 2) {
		rc = vmm_devtree_read_u32_atindex(node,
			VMM_DEVTREE_REG_ATTR_NAME, &cells[1], start+1);
		if (rc) {
			return rc;
		}
	}

	if (addr_cells == 2) {
		*addr = ((u64)cells[0] << 32) | (u64)cells[1];
	} else {
		*addr = cells[0];
	}

	devtree_map_regaddr(node, *addr, addr);

	return VMM_OK;
}

int vmm_devtree_regmap(struct vmm_devtree_node *node,
		       virtual_addr_t *addr, int regset)
{
	int rc;
	physical_addr_t pa;
	physical_size_t sz;

	if (!node || !addr || regset < 0) {
		return VMM_EFAIL;
	}

	rc = vmm_devtree_read_virtaddr_atindex(node,
					VMM_DEVTREE_VIRTUAL_REG_ATTR_NAME,
					addr, regset);
	if (!rc) {
		return VMM_OK;
	}

	rc = vmm_devtree_regsize(node, &sz, regset);
	if (rc) {
		return rc;
	}

	rc = vmm_devtree_regaddr(node, &pa, regset);
	if (rc) {
		return rc;
	}

	if (!sz) {
		return VMM_EINVALID;
	}

	*addr = vmm_host_iomap(pa, sz);

	return VMM_OK;
}

int vmm_devtree_regunmap(struct vmm_devtree_node *node,
		         virtual_addr_t addr, int regset)
{
	int rc;
	physical_size_t sz;
	virtual_addr_t vva;
	virtual_size_t vsz;

	if (!node || regset < 0) {
		return VMM_EFAIL;
	}

	if (vmm_devtree_getattr(node, VMM_DEVTREE_VIRTUAL_REG_ATTR_NAME)) {
		return VMM_OK;
	}

	rc = vmm_devtree_regsize(node, &sz, regset);
	if (rc) {
		return rc;
	}

	rc = vmm_host_vapool_find(addr, &vva, &vsz);
	if (rc) {
		return rc;
	}

	if (sz != vsz) {
		return VMM_EINVALID;
	}

	return vmm_host_iounmap(addr);
}

int vmm_devtree_regname_to_regset(struct vmm_devtree_node *node,
				  const char *regname)
{
	if (!node || !regname) {
		return VMM_EFAIL;
	}

	return vmm_devtree_match_string(node,
			VMM_DEVTREE_REG_NAMES_ATTR_NAME, regname);
}

int vmm_devtree_regmap_byname(struct vmm_devtree_node *node,
			      virtual_addr_t *addr, const char *regname)
{
	int regset;

	if (!node || !addr || !regname) {
		return VMM_EFAIL;
	}

	regset = vmm_devtree_regname_to_regset(node, regname);
	if (regset < 0)
		return regset;

	return vmm_devtree_regmap(node, addr, regset);
}

int vmm_devtree_regunmap_byname(struct vmm_devtree_node *node,
				virtual_addr_t addr, const char *regname)
{
	int regset;

	if (!node || !regname) {
		return VMM_EFAIL;
	}

	regset = vmm_devtree_regname_to_regset(node, regname);
	if (regset < 0)
		return regset;

	return vmm_devtree_regunmap(node, addr, regset);
}

bool vmm_devtree_is_reg_big_endian(struct vmm_devtree_node *node)
{
	if (!node) {
		return FALSE;
	}

	if (vmm_devtree_getattr(node, VMM_DEVTREE_BIG_ENDIAN_ATTR_NAME))
		return TRUE;
	if (IS_ENABLED(CONFIG_CPU_BE) &&
	    vmm_devtree_getattr(node, VMM_DEVTREE_NATIVE_ENDIAN_ATTR_NAME))
		return TRUE;

	return FALSE;
}

bool vmm_devtree_is_dma_coherent(struct vmm_devtree_node *node)
{
	if (node &&
	    vmm_devtree_getattr(node, VMM_DEVTREE_DMA_COHERENT_ATTR_NAME))
		return TRUE;
	return FALSE;
}

int vmm_devtree_request_regmap(struct vmm_devtree_node *node,
			       virtual_addr_t *addr, int regset,
			       const char *resname)
{
	int rc;
	physical_addr_t pa;
	physical_size_t sz;

	if (!node || !addr || (regset < 0) || !resname) {
		return VMM_EFAIL;
	}

	rc = vmm_devtree_read_virtaddr_atindex(node,
					VMM_DEVTREE_VIRTUAL_REG_ATTR_NAME,
					addr, regset);
	if (!rc) {
		return VMM_EINVALID;
	}

	rc = vmm_devtree_regsize(node, &sz, regset);
	if (rc) {
		return rc;
	}

	rc = vmm_devtree_regaddr(node, &pa, regset);
	if (rc) {
		return rc;
	}

	if (!sz) {
		return VMM_EINVALID;
	}

	vmm_request_mem_region(pa, sz, resname);

	*addr = vmm_host_iomap(pa, sz);

	return VMM_OK;
}

int vmm_devtree_regunmap_release(struct vmm_devtree_node *node,
				 virtual_addr_t addr, int regset)
{
	int rc;
	physical_addr_t pa;
	physical_size_t sz;
	virtual_addr_t vva;
	virtual_size_t vsz;

	if (!node || regset < 0) {
		return VMM_EFAIL;
	}

	if (vmm_devtree_getattr(node, VMM_DEVTREE_VIRTUAL_REG_ATTR_NAME)) {
		return VMM_EINVALID;
	}

	rc = vmm_devtree_regsize(node, &sz, regset);
	if (rc) {
		return rc;
	}

	rc = vmm_devtree_regaddr(node, &pa, regset);
	if (rc) {
		return rc;
	}

	rc = vmm_host_vapool_find(addr, &vva, &vsz);
	if (rc) {
		return rc;
	}

	if (sz != vsz) {
		return VMM_EINVALID;
	}

	rc = vmm_host_iounmap(addr);
	if (rc) {
		return rc;
	}

	vmm_release_mem_region(pa, sz);

	return VMM_OK;
}

int __init vmm_devtree_reserved_memory_init(void)
{
	int pos,ret;
	physical_addr_t pa;
	physical_size_t sz;
	struct vmm_devtree_node *child, *node;

	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_RESERVED_MEMORY_NODE_NAME);
	if (!node)
		return VMM_OK;

	vmm_devtree_for_each_child(child, node) {
		pos = 0;
		while (1) {
			if (vmm_devtree_regaddr(child, &pa, pos) != VMM_OK)
				break;
			if (vmm_devtree_regsize(child, &sz, pos) != VMM_OK)
				break;
			pos++;
			vmm_init_printf("ram_reserve: phys=0x%"PRIPADDR
					" size=%"PRIPSIZE"\n", pa, sz);
			ret = vmm_host_ram_reserve(pa, sz);
			if (ret) {
				vmm_devtree_dref_node(child);
				vmm_devtree_dref_node(node);
				return ret;
			}
		}
	}

	vmm_devtree_dref_node(node);

	return VMM_OK;
}
