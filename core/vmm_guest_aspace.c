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
 * @file vmm_guest_aspace.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for guest address space
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_devtree.h>
#include <vmm_devemu.h>
#include <vmm_host_ram.h>
#include <vmm_host_aspace.h>
#include <vmm_guest_aspace.h>
#include <vmm_stdio.h>
#include <libs/stringlib.h>

struct vmm_region *vmm_guest_find_region(struct vmm_guest *guest,
					 physical_addr_t gphys_addr,
					 u32 reg_flags, bool resolve_alias)
{
	bool found = FALSE;
	u32 cmp_flags;
	struct dlist *l;
	struct vmm_region *reg = NULL;

	if (guest == NULL) {
		return NULL;
	}

	/* Determine flags we need to compare */
	cmp_flags = reg_flags & ~VMM_REGION_MANIFEST_MASK;

	/* Try to find region ignoring required manifest flags */
	reg = NULL;
	found = FALSE;
	list_for_each(l, &guest->aspace.reg_list) {
		reg = list_entry(l, struct vmm_region, head);
		if (((reg->flags & cmp_flags) == cmp_flags) &&
		    (reg->gphys_addr <= gphys_addr) &&
		    (gphys_addr < (reg->gphys_addr + reg->phys_size))) {
			found = TRUE;
			break;
		}
	}
	if (!found) {
		return NULL;
	}

	/* Check if we can skip resolve alias */
	if (!resolve_alias) {
		goto done;
	}

	/* Resolve aliased regions */
	while (reg->flags & VMM_REGION_ALIAS) {
		found = FALSE;
		gphys_addr = reg->hphys_addr + (gphys_addr - reg->gphys_addr);
		reg = NULL;
		list_for_each(l, &guest->aspace.reg_list) {
			reg = list_entry(l, struct vmm_region, head);
			if (((reg->flags & cmp_flags) == cmp_flags) &&
			    (reg->gphys_addr <= gphys_addr) &&
			    (gphys_addr < (reg->gphys_addr + reg->phys_size))) {
				found = TRUE;
				break;
			}
		}
		if (!found) {
			return NULL;
		}
	}

done:
	cmp_flags = reg_flags & VMM_REGION_MANIFEST_MASK;
	if ((reg->flags & cmp_flags) != cmp_flags) {
		return NULL;
	}

	return reg;
}

u32 vmm_guest_memory_read(struct vmm_guest *guest, 
			  physical_addr_t gphys_addr, 
			  void *dst, u32 len, bool cacheable)
{
	u32 bytes_read = 0, to_read;
	physical_addr_t hphys_addr;
	struct vmm_region *reg = NULL;

	if (!guest || !dst || !len) {
		return 0;
	}

	while (bytes_read < len) {
		reg = vmm_guest_find_region(guest, gphys_addr, 
				VMM_REGION_REAL | VMM_REGION_MEMORY, TRUE);
		if (!reg) {
			break;
		}

		hphys_addr = reg->hphys_addr + (gphys_addr - reg->gphys_addr);
		to_read = (reg->gphys_addr + reg->phys_size - gphys_addr);
		to_read = ((len - bytes_read) < to_read) ? 
			  (len - bytes_read) : to_read;

		to_read = vmm_host_memory_read(hphys_addr,
					       dst, to_read, cacheable);
		if (!to_read) {
			break;
		}

		gphys_addr += to_read;
		bytes_read += to_read;
		dst += to_read;
	}

	return bytes_read;
}

u32 vmm_guest_memory_write(struct vmm_guest *guest, 
			   physical_addr_t gphys_addr, 
			   void *src, u32 len, bool cacheable)
{
	u32 bytes_written = 0, to_write;
	physical_addr_t hphys_addr;
	struct vmm_region *reg = NULL;

	if (!guest || !src || !len) {
		return 0;
	}

	while (bytes_written < len) {
		reg = vmm_guest_find_region(guest, gphys_addr, 
				VMM_REGION_REAL | VMM_REGION_MEMORY, TRUE);
		if (!reg) {
			break;
		}

		hphys_addr = reg->hphys_addr + (gphys_addr - reg->gphys_addr);
		to_write = (reg->gphys_addr + reg->phys_size - gphys_addr);
		to_write = ((len - bytes_written) < to_write) ? 
			   (len - bytes_written) : to_write;

		to_write = vmm_host_memory_write(hphys_addr,
						 src, to_write, cacheable);
		if (!to_write) {
			break;
		}

		gphys_addr += to_write;
		bytes_written += to_write;
		src += to_write;
	}

	return bytes_written;
}

int vmm_guest_physical_map(struct vmm_guest *guest,
			   physical_addr_t gphys_addr,
			   physical_size_t gphys_size,
			   physical_addr_t *hphys_addr,
			   physical_size_t *hphys_size,
			   u32 *reg_flags)
{
	/* FIXME: Need to implement dynamic RAM allocation for RAM region */
	struct vmm_region *reg = NULL;

	if (!guest || !hphys_addr) {
		return VMM_EFAIL;
	}

	reg = vmm_guest_find_region(guest, gphys_addr, 
				    VMM_REGION_MEMORY, FALSE);
	if (!reg) {
		return VMM_EFAIL;
	}
	while (reg->flags & VMM_REGION_ALIAS) {
		gphys_addr = reg->hphys_addr + (gphys_addr - reg->gphys_addr);
		reg = vmm_guest_find_region(guest, gphys_addr, 
					    VMM_REGION_MEMORY, FALSE);
		if (!reg) {
			return VMM_EFAIL;
		}
	}

	*hphys_addr = reg->hphys_addr + (gphys_addr - reg->gphys_addr);

	if (hphys_size) {
		*hphys_size = reg->gphys_addr + reg->phys_size - gphys_addr;
		if (gphys_size < *hphys_size) {
			*hphys_size = gphys_size;
		}
	}

	if (reg_flags) {
		*reg_flags = reg->flags;
	}

	return VMM_OK;
}

int vmm_guest_physical_unmap(struct vmm_guest *guest,
			     physical_addr_t gphys_addr,
			     physical_size_t gphys_size)
{
	/* FIXME: */
	return VMM_OK;
}

int vmm_guest_aspace_reset(struct vmm_guest *guest)
{
	struct dlist *l;
	struct vmm_region *reg = NULL;

	/* Sanity Check */
	if (!guest) {
		return VMM_EFAIL;
	}

	/* Reset device emulation for virtual regions */
	list_for_each(l, &guest->aspace.reg_list) {
		reg = list_entry(l, struct vmm_region, head);
		if (reg->flags & VMM_REGION_VIRTUAL) {
			vmm_devemu_reset_region(guest, reg);
		}
	}

	/* Reset device emulation context */
	return vmm_devemu_reset_context(guest);
}

bool is_region_node_valid(struct vmm_devtree_node *rnode)
{
	u32 order;
	const char *aval;
	bool is_real = FALSE;
	bool is_alias = FALSE;
	physical_addr_t addr;
	physical_size_t size;

	if (vmm_devtree_read_string(rnode, 
			VMM_DEVTREE_MANIFEST_TYPE_ATTR_NAME, &aval)) {
		return FALSE;
	}
	if (strcmp(aval, VMM_DEVTREE_MANIFEST_TYPE_VAL_REAL) != 0 &&
	    strcmp(aval, VMM_DEVTREE_MANIFEST_TYPE_VAL_VIRTUAL) != 0 && 
	    strcmp(aval, VMM_DEVTREE_MANIFEST_TYPE_VAL_ALIAS) != 0) {
		return FALSE;
	}
	if (strcmp(aval, VMM_DEVTREE_MANIFEST_TYPE_VAL_REAL) == 0) {
		is_real = TRUE;
	}
	if (strcmp(aval, VMM_DEVTREE_MANIFEST_TYPE_VAL_ALIAS) == 0) {
		is_alias = TRUE;
	}

	if (vmm_devtree_read_string(rnode,
			VMM_DEVTREE_ADDRESS_TYPE_ATTR_NAME, &aval)) {
		return FALSE;
	}
	if (strcmp(aval, VMM_DEVTREE_ADDRESS_TYPE_VAL_IO) != 0 &&
	    strcmp(aval, VMM_DEVTREE_ADDRESS_TYPE_VAL_MEMORY) != 0) {
		return FALSE;
	}

	if (vmm_devtree_read_physaddr(rnode,
			VMM_DEVTREE_GUEST_PHYS_ATTR_NAME, &addr)) {
		return FALSE;
	}

	if (is_real) {
		if (vmm_devtree_read_physaddr(rnode,
			VMM_DEVTREE_HOST_PHYS_ATTR_NAME, &addr)) {
			return FALSE;
		}
	}

	if (is_alias) {
		if (vmm_devtree_read_physaddr(rnode, 
			VMM_DEVTREE_ALIAS_PHYS_ATTR_NAME, &addr)) {
			return FALSE;
		}
	}

	if (vmm_devtree_read_physsize(rnode,
			VMM_DEVTREE_PHYS_SIZE_ATTR_NAME, &size)) {
		return FALSE;
	}

	if (vmm_devtree_read_u32(rnode,
			VMM_DEVTREE_ALIGN_ORDER_ATTR_NAME, &order)) {
		order = 0;
	}

	if (BITS_PER_LONG <= order) {
		return FALSE;
	}
	if (size & order_mask(order)) {
		return FALSE;
	}

	return TRUE;
}

static bool is_region_overlapping(struct vmm_guest *guest,
				  struct vmm_region *reg)
{
	struct dlist *l;
	struct vmm_region *treg = NULL;
	physical_addr_t reg_start, reg_end;
	physical_addr_t treg_start, treg_end;

	reg_start = reg->gphys_addr;
	reg_end = reg->gphys_addr + reg->phys_size;

	list_for_each(l, &guest->aspace.reg_list) {
		treg = list_entry(l, struct vmm_region, head);
		if ((treg->flags & VMM_REGION_MEMORY) && 
		    !(reg->flags & VMM_REGION_MEMORY)) {
			continue;
		}
		if ((treg->flags & VMM_REGION_IO) && 
		    !(reg->flags & VMM_REGION_IO)) {
			continue;
		}
		treg_start = treg->gphys_addr;
		treg_end = treg->gphys_addr + treg->phys_size;
		if ((treg_start <= reg_start) && (reg_start < treg_end)) {
			return TRUE;
		}
		if ((treg_start < reg_end) && (reg_end < treg_end)) {
			return TRUE;
		}
	}

	return FALSE;
}

int vmm_guest_aspace_init(struct vmm_guest *guest)
{
	int rc;
	const char *aval;
	struct dlist *l;
	struct vmm_devtree_node *gnode;
	struct vmm_devtree_node *rnode = NULL;
	struct vmm_region *reg = NULL;

	/* Sanity Check */
	if (!guest) {
		return VMM_EFAIL;
	}

        gnode = guest->node;

	/* Reset the address space for guest */
	memset(&guest->aspace, 0, sizeof(struct vmm_guest_aspace));

	/* Get address space node under guest node */
	guest->aspace.node = vmm_devtree_getchild(gnode, 
					VMM_DEVTREE_ADDRSPACE_NODE_NAME);
	if (!guest->aspace.node) {
		vmm_printf("%s: %s/aspace node not found\n",
			   __func__, gnode->name);
		return VMM_EFAIL;
	}

	/* Point to parent guest */
	guest->aspace.guest = guest;

	/* Initialize region list */
	INIT_LIST_HEAD(&guest->aspace.reg_list);

	/* Initialize devemu_priv pointer of aspace */
	guest->aspace.devemu_priv = NULL;

	/* Initialize device emulation context */
	if ((rc = vmm_devemu_init_context(guest))) {
		return rc;
	}

	/* Populate valid regions */
	list_for_each(l, &(guest->aspace.node->child_list)) {
		rnode = list_entry(l, struct vmm_devtree_node, head);
		if (!rnode) {
			continue;
		}

		if (!is_region_node_valid(rnode)) {
			continue;
		}

		reg = vmm_zalloc(sizeof(struct vmm_region));

		INIT_LIST_HEAD(&reg->head);

		reg->node = rnode;
		reg->aspace = &guest->aspace;
		reg->flags = 0x0;

		rc = vmm_devtree_read_string(rnode,
				VMM_DEVTREE_MANIFEST_TYPE_ATTR_NAME, &aval);
		if (rc) {
			return rc;
		}

		if (!strcmp(aval, VMM_DEVTREE_MANIFEST_TYPE_VAL_REAL)) {
			reg->flags |= VMM_REGION_REAL;
		} else if (!strcmp(aval,
				VMM_DEVTREE_MANIFEST_TYPE_VAL_ALIAS)) {
			reg->flags |= VMM_REGION_ALIAS;
		} else {
			reg->flags |= VMM_REGION_VIRTUAL;
		}

		rc = vmm_devtree_read_string(rnode,
				VMM_DEVTREE_ADDRESS_TYPE_ATTR_NAME, &aval);
		if (rc) {
			return rc;
		}

		if (!strcmp(aval, VMM_DEVTREE_ADDRESS_TYPE_VAL_IO)) {
			reg->flags |= VMM_REGION_IO;
		} else {
			reg->flags |= VMM_REGION_MEMORY;
		}

		rc = vmm_devtree_read_string(rnode,
				VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME, &aval);
		if (rc) {
			return rc;
		}

		if (!strcmp(aval, VMM_DEVTREE_DEVICE_TYPE_VAL_RAM) ||
		    !strcmp(aval, VMM_DEVTREE_DEVICE_TYPE_VAL_ALLOCED_RAM)) {
			reg->flags |= VMM_REGION_ISRAM;
		} else if (!strcmp(aval, VMM_DEVTREE_DEVICE_TYPE_VAL_ROM) ||
		      !strcmp(aval, VMM_DEVTREE_DEVICE_TYPE_VAL_ALLOCED_ROM)) {
			reg->flags |= VMM_REGION_READONLY;
			reg->flags |= VMM_REGION_ISROM;
		} else {
			reg->flags |= VMM_REGION_ISDEVICE;
		}

		if (!strcmp(aval, VMM_DEVTREE_DEVICE_TYPE_VAL_RAM) ||
		    !strcmp(aval, VMM_DEVTREE_DEVICE_TYPE_VAL_ROM)) {
			reg->flags |= VMM_REGION_ISRESERVED;
		}

		if (!strcmp(aval, VMM_DEVTREE_DEVICE_TYPE_VAL_ALLOCED_RAM) ||
		    !strcmp(aval, VMM_DEVTREE_DEVICE_TYPE_VAL_ALLOCED_ROM)) {
			reg->flags |= VMM_REGION_ISALLOCED;
		}

		if ((reg->flags & VMM_REGION_REAL) &&
		    (reg->flags & VMM_REGION_MEMORY) &&
		    (reg->flags & VMM_REGION_ISRAM)) {
			reg->flags |= VMM_REGION_CACHEABLE;
			reg->flags |= VMM_REGION_BUFFERABLE;
		}

		rc = vmm_devtree_read_physaddr(rnode,
				VMM_DEVTREE_GUEST_PHYS_ATTR_NAME,
				&reg->gphys_addr);
		if (rc) {
			return rc;
		}

		if (reg->flags & VMM_REGION_REAL) {
			rc = vmm_devtree_read_physaddr(rnode,
					VMM_DEVTREE_HOST_PHYS_ATTR_NAME,
					&reg->hphys_addr);
			if (rc) {
				return rc;
			}
		} else if (reg->flags & VMM_REGION_ALIAS){
			rc = vmm_devtree_read_physaddr(rnode,
					VMM_DEVTREE_ALIAS_PHYS_ATTR_NAME,
					&reg->hphys_addr);
			if (rc) {
				return rc;
			}
		} else {
			reg->hphys_addr = reg->gphys_addr;
		}

		rc = vmm_devtree_read_physsize(rnode,
				VMM_DEVTREE_PHYS_SIZE_ATTR_NAME,
				&reg->phys_size);
		if (rc) {
			return rc;
		}

		rc = vmm_devtree_read_u32(rnode,
				VMM_DEVTREE_ALIGN_ORDER_ATTR_NAME,
				&reg->align_order);
		if (rc) {
			reg->align_order = 0;
		}

		reg->devemu_priv = NULL;

		if (is_region_overlapping(guest, reg)) {
			vmm_free(reg);
			vmm_printf("%s: Region for %s/%s overlapping with "
				   "a previous node\n", __func__, 
				   gnode->name, rnode->name);
			return VMM_EINVALID;
		}

		if (!(reg->flags & (VMM_REGION_ALIAS | VMM_REGION_VIRTUAL)) &&
		    (reg->flags & (VMM_REGION_ISRAM | VMM_REGION_ISROM)) &&
		    (reg->flags & VMM_REGION_ISRESERVED)) {
			rc = vmm_host_ram_reserve(reg->hphys_addr,
						  reg->phys_size);
			if (rc) {
				vmm_printf("%s: Failed to reserve "
					   "host RAM for %s/%s\n",
					   __func__, gnode->name,
					   rnode->name);
				vmm_free(reg);
				return rc;
			} else {
				reg->flags |= VMM_REGION_ISHOSTRAM;
			}
		}

		list_add_tail(&reg->head, &guest->aspace.reg_list);
	}

	/* Allocate host RAM for alloced RAM/ROM regions */
	list_for_each(l, &guest->aspace.reg_list) {
		reg = list_entry(l, struct vmm_region, head);
		if (!(reg->flags & (VMM_REGION_ALIAS | VMM_REGION_VIRTUAL)) &&
		    (reg->flags & (VMM_REGION_ISRAM | VMM_REGION_ISROM)) &&
		    (reg->flags & VMM_REGION_ISALLOCED)) {
			if (!vmm_host_ram_alloc(&reg->hphys_addr,
						reg->phys_size,
						reg->align_order)) {
				vmm_printf("%s: Failed to alloc "
					   "host RAM for %s/%s\n",
					   __func__, gnode->name,
					   rnode->name);
				return VMM_ENOMEM;
			} else {
				reg->flags |= VMM_REGION_ISHOSTRAM;
			}
		}
	}

	/* Probe device emulation for virtual regions */
	list_for_each(l, &guest->aspace.reg_list) {
		reg = list_entry(l, struct vmm_region, head);
		if (reg->flags & VMM_REGION_VIRTUAL) {
			if ((rc = vmm_devemu_probe_region(guest, reg))) {
				return rc;
			}
		}
	}

	return VMM_OK;
}

int vmm_guest_aspace_deinit(struct vmm_guest *guest)
{
	int rc;
	struct dlist *l;
	struct vmm_region *reg = NULL;

	/* Sanity Check */
	if (!guest) {
		return VMM_EFAIL;
	}

	/* Remove emulator for each virtual regions */
	list_for_each(l, &guest->aspace.reg_list) {
		reg = list_entry(l, struct vmm_region, head);
		if (reg->flags & VMM_REGION_VIRTUAL) {
			vmm_devemu_remove_region(guest, reg);
		}
	}

	/* DeInitialize device emulation context */
	if ((rc = vmm_devemu_deinit_context(guest))) {
		return rc;
	}

	/* Free the regions */
	while (!list_empty(&guest->aspace.reg_list)) {
		l = list_pop(&guest->aspace.reg_list);
		reg = list_entry(l, struct vmm_region, head);

		if (!(reg->flags & (VMM_REGION_ALIAS | VMM_REGION_VIRTUAL)) &&
		    (reg->flags & (VMM_REGION_ISRAM | VMM_REGION_ISROM)) &&
		    (reg->flags & VMM_REGION_ISHOSTRAM)) {
			rc = vmm_host_ram_free(reg->hphys_addr, 
						reg->phys_size);
			if (rc) {
				return rc;
			}
		}

		vmm_free(reg);
	}

	/* Reset region list */
	INIT_LIST_HEAD(&guest->aspace.reg_list);

	/* Reset devemu_priv pointer of aspace */
	guest->aspace.devemu_priv = NULL;

	return VMM_OK;
}

