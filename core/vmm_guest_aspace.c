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
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for guest address space
 */

#include <vmm_error.h>
#include <vmm_list.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_devtree.h>
#include <vmm_devemu.h>
#include <vmm_host_aspace.h>
#include <vmm_guest_aspace.h>

struct vmm_region *vmm_guest_find_region(struct vmm_guest *guest,
					 physical_addr_t gphys_addr,
					 bool resolve_alias)
{
	bool found = FALSE;
	struct dlist *l;
	struct vmm_region *reg = NULL;

	if (guest == NULL) {
		return NULL;
	}

	reg = NULL;
	found = FALSE;
	list_for_each(l, &guest->aspace.reg_list) {
		reg = list_entry(l, struct vmm_region, head);
		if (reg->gphys_addr <= gphys_addr &&
		    gphys_addr < (reg->gphys_addr + reg->phys_size)) {
			found = TRUE;
			break;
		}
	}
	if (!found) {
		return NULL;
	}

	while (resolve_alias && (reg->flags & VMM_REGION_ALIAS)) {
		gphys_addr = reg->hphys_addr + (gphys_addr - reg->gphys_addr);
		reg = NULL;
		found = FALSE;
		list_for_each(l, &guest->aspace.reg_list) {
			reg = list_entry(l, struct vmm_region, head);
			if (reg->gphys_addr <= gphys_addr &&
			    gphys_addr < (reg->gphys_addr + reg->phys_size)) {
				found = TRUE;
				break;
			}
		}
		if (!found) {
			return NULL;
		}
	}

	return reg;
}

u32 vmm_guest_physical_read(struct vmm_guest * guest, 
			    physical_addr_t gphys_addr, 
			    void * dst, u32 len)
{
	u32 bytes_read = 0, to_read;
	physical_addr_t hphys_addr;
	struct vmm_region * reg = NULL;

	if (!guest || !dst || !len) {
		return 0;
	}

	while (bytes_read < len) {
		reg = vmm_guest_find_region(guest, gphys_addr, TRUE);
		if (!reg) {
			break;
		}

		if (reg->flags & (VMM_REGION_VIRTUAL | VMM_REGION_IO)) {
			break;
		}

		hphys_addr = reg->hphys_addr + (gphys_addr - reg->gphys_addr);
		to_read = (reg->gphys_addr + reg->phys_size - gphys_addr);
		to_read = ((len - bytes_read) < to_read) ? 
			  (len - bytes_read) : to_read;

		to_read = vmm_host_physical_read(hphys_addr, dst, to_read);
		if (!to_read) {
			break;
		}

		gphys_addr += to_read;
		bytes_read += to_read;
		dst += to_read;
	}

	return bytes_read;
}

u32 vmm_guest_physical_write(struct vmm_guest * guest, 
			     physical_addr_t gphys_addr, 
			     void * src, u32 len)
{
	u32 bytes_written = 0, to_write;
	physical_addr_t hphys_addr;
	struct vmm_region * reg = NULL;

	if (!guest || !src || !len) {
		return 0;
	}

	while (bytes_written < len) {
		reg = vmm_guest_find_region(guest, gphys_addr, TRUE);
		if (!reg) {
			break;
		}

		if (reg->flags & (VMM_REGION_VIRTUAL | VMM_REGION_IO)) {
			break;
		}

		hphys_addr = reg->hphys_addr + (gphys_addr - reg->gphys_addr);
		to_write = (reg->gphys_addr + reg->phys_size - gphys_addr);
		to_write = ((len - bytes_written) < to_write) ? 
			   (len - bytes_written) : to_write;

		to_write = vmm_host_physical_write(hphys_addr, src, to_write);
		if (!to_write) {
			break;
		}

		gphys_addr += to_write;
		bytes_written += to_write;
		src += to_write;
	}

	return bytes_written;
}

int vmm_guest_physical_map(struct vmm_guest * guest,
			   physical_addr_t gphys_addr,
			   physical_size_t gphys_size,
			   physical_addr_t * hphys_addr,
			   physical_size_t * hphys_size,
			   u32 * reg_flags)
{
	/* FIXME: Need to implement dynamic RAM allocation for RAM region */
	struct vmm_region * reg = NULL;

	if (!guest || !hphys_addr) {
		return VMM_EFAIL;
	}

	reg = vmm_guest_find_region(guest, gphys_addr, FALSE);
	if (!reg) {
		return VMM_EFAIL;
	}
	while (reg->flags & VMM_REGION_ALIAS) {
		gphys_addr = reg->hphys_addr + (gphys_addr - reg->gphys_addr);
		reg = vmm_guest_find_region(guest, gphys_addr, FALSE);
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

int vmm_guest_physical_unmap(struct vmm_guest * guest,
			     physical_addr_t gphys_addr,
			     physical_size_t gphys_size)
{
	/* FIXME: */
	return VMM_OK;
}

bool is_address_node_valid(struct vmm_devtree_node * anode)
{
	const char *attrval;
	bool is_real = FALSE;
	bool is_alias = FALSE;

	attrval = vmm_devtree_attrval(anode, VMM_DEVTREE_MANIFEST_TYPE_ATTR_NAME);
	if (!attrval) {
		return FALSE;
	}
	if (vmm_strcmp(attrval, VMM_DEVTREE_MANIFEST_TYPE_VAL_REAL) != 0 &&
	    vmm_strcmp(attrval, VMM_DEVTREE_MANIFEST_TYPE_VAL_VIRTUAL) != 0 && 
	    vmm_strcmp(attrval, VMM_DEVTREE_MANIFEST_TYPE_VAL_ALIAS) != 0) {
		return FALSE;
	}
	if (vmm_strcmp(attrval, VMM_DEVTREE_MANIFEST_TYPE_VAL_REAL) == 0) {
		is_real = TRUE;
	}
	if (vmm_strcmp(attrval, VMM_DEVTREE_MANIFEST_TYPE_VAL_ALIAS) == 0) {
		is_alias = TRUE;
	}

	attrval = vmm_devtree_attrval(anode,
				      VMM_DEVTREE_ADDRESS_TYPE_ATTR_NAME);
	if (!attrval) {
		return FALSE;
	}
	if (vmm_strcmp(attrval, VMM_DEVTREE_ADDRESS_TYPE_VAL_IO) != 0 &&
	    vmm_strcmp(attrval, VMM_DEVTREE_ADDRESS_TYPE_VAL_MEMORY) != 0) {
		return FALSE;
	}

	attrval = vmm_devtree_attrval(anode, VMM_DEVTREE_GUEST_PHYS_ATTR_NAME);
	if (!attrval) {
		return FALSE;
	}

	if (is_real) {
		attrval = vmm_devtree_attrval(anode, 
					VMM_DEVTREE_HOST_PHYS_ATTR_NAME);
		if (!attrval) {
			return FALSE;
		}
	}

	if (is_alias) {
		attrval = vmm_devtree_attrval(anode, 
					VMM_DEVTREE_ALIAS_PHYS_ATTR_NAME);
		if (!attrval) {
			return FALSE;
		}
	}

	attrval = vmm_devtree_attrval(anode, VMM_DEVTREE_PHYS_SIZE_ATTR_NAME);
	if (!attrval) {
		return FALSE;
	}

	return TRUE;
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

int vmm_guest_aspace_init(struct vmm_guest *guest)
{
	int rc;
	const char *attrval;
	struct dlist *l;
	struct vmm_devtree_node *gnode = guest->node;
	struct vmm_devtree_node *anode = NULL;
	struct vmm_region *reg = NULL;

	/* Sanity Check */
	if (!guest) {
		return VMM_EFAIL;
	}

	/* Reset the address space for guest */
	vmm_memset(&guest->aspace, 0, sizeof(struct vmm_guest_aspace));

	/* Get address space node under guest node */
	guest->aspace.node = vmm_devtree_getchild(gnode, 
					VMM_DEVTREE_ADDRSPACE_NODE_NAME);
	if (!guest->aspace.node) {
		return VMM_EFAIL;
	}

	/* Point to parent guest */
	guest->aspace.guest = guest;

	/* Initialize region list */
	INIT_LIST_HEAD(&guest->aspace.reg_list);

	/* Initialize devemu_priv pointer of aspace */
	guest->aspace.devemu_priv = NULL;

	/* Populate valid regions */
	list_for_each(l, &(guest->aspace.node->child_list)) {
		anode = list_entry(l, struct vmm_devtree_node, head);
		if (!anode) {
			continue;
		}

		if (!is_address_node_valid(anode)) {
			continue;
		}

		reg = vmm_malloc(sizeof(struct vmm_region));

		vmm_memset(reg, 0, sizeof(struct vmm_region));

		INIT_LIST_HEAD(&reg->head);

		reg->node = anode;
		reg->aspace = &guest->aspace;
		reg->flags = 0x0;

		attrval = vmm_devtree_attrval(anode,
					      VMM_DEVTREE_MANIFEST_TYPE_ATTR_NAME);
		if (vmm_strcmp(attrval, VMM_DEVTREE_MANIFEST_TYPE_VAL_REAL) == 0) {
			reg->flags |= VMM_REGION_REAL;
		} else if (vmm_strcmp(attrval, VMM_DEVTREE_MANIFEST_TYPE_VAL_ALIAS) == 0) {
			reg->flags |= VMM_REGION_ALIAS;
		} else {
			reg->flags |= VMM_REGION_VIRTUAL;
		}

		attrval = vmm_devtree_attrval(anode,
					      VMM_DEVTREE_ADDRESS_TYPE_ATTR_NAME);
		if (vmm_strcmp(attrval, VMM_DEVTREE_ADDRESS_TYPE_VAL_IO) == 0) {
			reg->flags |= VMM_REGION_IO;
		} else {
			reg->flags |= VMM_REGION_MEMORY;
		}

		attrval = vmm_devtree_attrval(anode,
					      VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME);
		if (vmm_strcmp(attrval, VMM_DEVTREE_DEVICE_TYPE_VAL_RAM) == 0) {
			reg->flags |= VMM_REGION_ISRAM;
		} else if (vmm_strcmp(attrval, VMM_DEVTREE_DEVICE_TYPE_VAL_ROM) == 0) {
			reg->flags |= VMM_REGION_READONLY;
			reg->flags |= VMM_REGION_ISROM;
		} else {
			reg->flags |= VMM_REGION_ISDEVICE;
		}

		if ((reg->flags & VMM_REGION_REAL) &&
		    (reg->flags & VMM_REGION_MEMORY) &&
		    (reg->flags & VMM_REGION_ISRAM)) {
			reg->flags |= VMM_REGION_CACHEABLE;
		}

		attrval = vmm_devtree_attrval(anode,
					      VMM_DEVTREE_GUEST_PHYS_ATTR_NAME);
		reg->gphys_addr = *((physical_addr_t *) attrval);

		if (reg->flags & VMM_REGION_REAL) {
			attrval = vmm_devtree_attrval(anode,
					      VMM_DEVTREE_HOST_PHYS_ATTR_NAME);
			reg->hphys_addr = *((physical_addr_t *) attrval);
		} else if (reg->flags & VMM_REGION_ALIAS){
			attrval = vmm_devtree_attrval(anode,
					      VMM_DEVTREE_ALIAS_PHYS_ATTR_NAME);
			reg->hphys_addr = *((physical_addr_t *) attrval);
		} else {
			reg->hphys_addr = reg->gphys_addr;
		}

		attrval = vmm_devtree_attrval(anode,
					      VMM_DEVTREE_PHYS_SIZE_ATTR_NAME);
		reg->phys_size = *((physical_size_t *) attrval);

		reg->devemu_priv = NULL;

		if (!(reg->flags & (VMM_REGION_ALIAS | VMM_REGION_VIRTUAL)) && 
		    (reg->flags & (VMM_REGION_ISRAM | VMM_REGION_ISROM))) {
			rc = vmm_host_ram_reserve(reg->hphys_addr, 
						  reg->phys_size);
			if (rc) {
				return rc;
			}
		}

		list_add_tail(&guest->aspace.reg_list, &reg->head);
	}

	/* Initialize device emulation context */
	if ((rc = vmm_devemu_init_context(guest))) {
		return rc;
	}

	/* Probe device emulation for virutal regions */
	list_for_each(l, &guest->aspace.reg_list) {
		reg = list_entry(l, struct vmm_region, head);
		if (reg->flags & VMM_REGION_VIRTUAL) {
			vmm_devemu_probe_region(guest, reg);
		}
	}

	return VMM_OK;
}

