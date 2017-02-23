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
#include <vmm_resource.h>
#include <vmm_host_vapool.h>
#include <vmm_host_aspace.h>
#include <vmm_devtree.h>
#include <libs/stringlib.h>

int vmm_devtree_regsize(struct vmm_devtree_node *node,
		        physical_size_t *size, int regset)
{
	int rc;
	u32 start, addr_cells, size_cells, cells[2] = { 0, 0 };
	struct vmm_devtree_node *np;

	if (!node || !size || regset < 0) {
		return VMM_EFAIL;
	}

	if (vmm_devtree_getattr(node, VMM_DEVTREE_VIRTUAL_REG_ATTR_NAME)) {
		return VMM_ENOTAVAIL;
	}

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

	if ((addr_cells > 2) || (size_cells < 1) || (2 < size_cells)) {
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
	struct vmm_devtree_node *np;

	if (!node || !addr || regset < 0) {
		return VMM_EFAIL;
	}

	if (vmm_devtree_getattr(node, VMM_DEVTREE_VIRTUAL_REG_ATTR_NAME)) {
		return VMM_ENOTAVAIL;
	}

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

	if ((size_cells > 2) || (addr_cells < 1) || (2 < addr_cells)) {
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
