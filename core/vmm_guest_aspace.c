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
#include <vmm_guest_aspace.h>

bool vmm_guest_aspace_isvirtual(vmm_guest_t *guest,
				physical_addr_t gphys_addr)
{
	struct dlist *l;
	vmm_guest_region_t *reg = NULL;

	if (guest == NULL) {
		return FALSE;
	}

	list_for_each(l, &guest->aspace.reg_list) {
		reg = list_entry(l, vmm_guest_region_t, head);
		if (reg->gphys_addr <= gphys_addr &&
		    gphys_addr < (reg->gphys_addr + reg->phys_size)) {
			return reg->is_virtual;
		}
	}

	return FALSE;
}

vmm_guest_region_t *vmm_guest_aspace_getregion(vmm_guest_t *guest,
					       physical_addr_t gphys_addr)
{
	bool found = FALSE;
	struct dlist *l;
	vmm_guest_region_t *reg = NULL;

	if (guest == NULL) {
		return NULL;
	}

	list_for_each(l, &guest->aspace.reg_list) {
		reg = list_entry(l, vmm_guest_region_t, head);
		if (reg->gphys_addr <= gphys_addr &&
		    gphys_addr < (reg->gphys_addr + reg->phys_size)) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return NULL;
	}

	return reg;
}

bool is_address_node_valid(vmm_devtree_node_t * anode)
{
	const char *attrval;

	attrval = vmm_devtree_attrval(anode, VMM_DEVTREE_MANIFEST_TYPE_ATTR_NAME);
	if (!attrval) {
		return FALSE;
	}
	if (vmm_strcmp(attrval, VMM_DEVTREE_MANIFEST_TYPE_VAL_REAL) != 0 &&
	    vmm_strcmp(attrval, VMM_DEVTREE_MANIFEST_TYPE_VAL_VIRTUAL) != 0) {
		return FALSE;
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

	attrval = vmm_devtree_attrval(anode, VMM_DEVTREE_HOST_PHYS_ATTR_NAME);
	if (!attrval) {
		return FALSE;
	}

	attrval = vmm_devtree_attrval(anode, VMM_DEVTREE_PHYS_SIZE_ATTR_NAME);
	if (!attrval) {
		return FALSE;
	}

	return TRUE;
}

int vmm_guest_aspace_initguest(vmm_guest_t *guest)
{
	const char *attrval;
	struct dlist *l;
	vmm_devtree_node_t *gnode = guest->node;
	vmm_devtree_node_t *anode = NULL;
	vmm_guest_region_t *reg = NULL;

	/* Reset the address space for guest */
	vmm_memset(&guest->aspace, 0, sizeof(vmm_guest_aspace_t));

	/* Get address space node under guest node */
	guest->aspace.node =
	    vmm_devtree_getchildnode(gnode, VMM_DEVTREE_ADDRSPACE_NODE_NAME);
	if (!guest->aspace.node) {
		return VMM_EFAIL;
	}

	/* Point to parent guest */
	guest->aspace.guest = guest;

	/* Initialize region list */
	INIT_LIST_HEAD(&guest->aspace.reg_list);

	/* Initialize priv pointer of aspace */
	guest->aspace.priv = NULL;

	/* Populate valid regions */
	list_for_each(l, &(guest->aspace.node->child_list)) {
		anode = list_entry(l, vmm_devtree_node_t, head);
		if (!anode) {
			continue;
		}

		if (!is_address_node_valid(anode)) {
			continue;
		}

		reg = vmm_malloc(sizeof(vmm_guest_region_t));

		vmm_memset(reg, 0, sizeof(vmm_guest_region_t));

		INIT_LIST_HEAD(&reg->head);

		reg->node = anode;
		reg->aspace = &guest->aspace;

		attrval = vmm_devtree_attrval(anode,
					      VMM_DEVTREE_MANIFEST_TYPE_ATTR_NAME);
		if (vmm_strcmp(attrval, VMM_DEVTREE_MANIFEST_TYPE_VAL_REAL) == 0) {
			reg->is_virtual = FALSE;
		} else {
			reg->is_virtual = TRUE;
		}

		attrval = vmm_devtree_attrval(anode,
					      VMM_DEVTREE_ADDRESS_TYPE_ATTR_NAME);
		if (vmm_strcmp(attrval, VMM_DEVTREE_ADDRESS_TYPE_VAL_IO) == 0) {
			reg->is_memory = FALSE;
		} else {
			reg->is_memory = TRUE;
		}

		attrval = vmm_devtree_attrval(anode,
					      VMM_DEVTREE_GUEST_PHYS_ATTR_NAME);
		reg->gphys_addr = *((physical_addr_t *) attrval);

		attrval = vmm_devtree_attrval(anode,
					      VMM_DEVTREE_HOST_PHYS_ATTR_NAME);
		reg->hphys_addr = *((physical_addr_t *) attrval);

		attrval = vmm_devtree_attrval(anode,
					      VMM_DEVTREE_PHYS_SIZE_ATTR_NAME);
		reg->phys_size = *((physical_size_t *) attrval);

		reg->priv = NULL;

		list_add_tail(&guest->aspace.reg_list, &reg->head);
	}

	return VMM_OK;
}

