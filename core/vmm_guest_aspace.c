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
	irq_flags_t flags;
	struct vmm_guest_aspace *aspace;
	struct vmm_region *reg = NULL;

	if (!guest) {
		return NULL;
	}
	if (!guest->aspace.initialized) {
		return NULL;
	}
	aspace = &guest->aspace;

	/* Determine flags we need to compare */
	cmp_flags = reg_flags & ~VMM_REGION_MANIFEST_MASK;

	/* Try to find region ignoring required manifest flags */
	reg = NULL;
	found = FALSE;
	vmm_read_lock_irqsave_lite(&aspace->reg_list_lock, flags);
	list_for_each_entry(reg, &aspace->reg_list, head) {
		if (((reg->flags & cmp_flags) == cmp_flags) &&
		    (reg->gphys_addr <= gphys_addr) &&
		    (gphys_addr < (reg->gphys_addr + reg->phys_size))) {
			found = TRUE;
			break;
		}
	}
	vmm_read_unlock_irqrestore_lite(&aspace->reg_list_lock, flags);
	if (!found) {
		return NULL;
	}

	/* Check if we can skip resolve alias */
	if (!resolve_alias) {
		goto done;
	}

	/* Resolve aliased regions */
	while (reg->flags & VMM_REGION_ALIAS) {
		gphys_addr = reg->hphys_addr + (gphys_addr - reg->gphys_addr);
		vmm_read_lock_irqsave_lite(&aspace->reg_list_lock, flags);
		reg = NULL;
		found = FALSE;
		list_for_each_entry(reg, &aspace->reg_list, head) {
			if (((reg->flags & cmp_flags) == cmp_flags) &&
			    (reg->gphys_addr <= gphys_addr) &&
			    (gphys_addr < (reg->gphys_addr + reg->phys_size))) {
				found = TRUE;
				break;
			}
		}
		vmm_read_unlock_irqrestore_lite(&aspace->reg_list_lock, flags);
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

	if (!guest || !guest->aspace.initialized || !dst || !len) {
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

	if (!guest || !guest->aspace.initialized || !src || !len) {
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
	if (!guest->aspace.initialized) {
		return VMM_ENOTAVAIL;
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
	bool ret = FALSE;
	irq_flags_t flags;
	struct vmm_guest_aspace *aspace;
	struct vmm_region *treg = NULL;
	physical_addr_t reg_start, reg_end;
	physical_addr_t treg_start, treg_end;

	aspace = &guest->aspace;
	reg_start = reg->gphys_addr;
	reg_end = reg->gphys_addr + reg->phys_size;

	vmm_read_lock_irqsave_lite(&aspace->reg_list_lock, flags);
	list_for_each_entry(treg, &aspace->reg_list, head) {
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
			ret = TRUE;
			break;
		}
		if ((treg_start < reg_end) && (reg_end < treg_end)) {
			ret = TRUE;
			break;
		}
	}
	vmm_read_unlock_irqrestore_lite(&aspace->reg_list_lock, flags);

	return ret;
}

static int region_add(struct vmm_guest *guest,
		      struct vmm_devtree_node *rnode,
		      struct vmm_region **new_reg)
{
	int rc;
	const char *aval;
	irq_flags_t flags;
	struct vmm_region *reg = NULL;
	struct vmm_guest_aspace *aspace = &guest->aspace;

	/* Sanity check on region node */
	if (!is_region_node_valid(rnode)) {
		return VMM_EINVALID;
	}

	/* Allocate region instance */
	reg = vmm_zalloc(sizeof(struct vmm_region));
	INIT_LIST_HEAD(&reg->head);

	/* Fillup region details */
	reg->node = rnode;
	reg->aspace = aspace;
	reg->flags = 0x0;

	rc = vmm_devtree_read_string(rnode,
			VMM_DEVTREE_MANIFEST_TYPE_ATTR_NAME, &aval);
	if (rc) {
		vmm_free(reg);
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
		vmm_free(reg);
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
		vmm_free(reg);
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
		vmm_free(reg);
		return rc;
	}

	if (reg->flags & VMM_REGION_REAL) {
		rc = vmm_devtree_read_physaddr(rnode,
				VMM_DEVTREE_HOST_PHYS_ATTR_NAME,
				&reg->hphys_addr);
		if (rc) {
			vmm_free(reg);
			return rc;
		}
	} else if (reg->flags & VMM_REGION_ALIAS) {
		rc = vmm_devtree_read_physaddr(rnode,
				VMM_DEVTREE_ALIAS_PHYS_ATTR_NAME,
				&reg->hphys_addr);
		if (rc) {
			vmm_free(reg);
			return rc;
		}
	} else {
		reg->hphys_addr = reg->gphys_addr;
	}

	rc = vmm_devtree_read_physsize(rnode,
			VMM_DEVTREE_PHYS_SIZE_ATTR_NAME,
			&reg->phys_size);
	if (rc) {
		vmm_free(reg);
		return rc;
	}

	rc = vmm_devtree_read_u32(rnode,
			VMM_DEVTREE_ALIGN_ORDER_ATTR_NAME,
			&reg->align_order);
	if (rc) {
		reg->align_order = 0;
	}

	reg->devemu_priv = NULL;

	/* Ensure region does not overlap other regions */
	if (is_region_overlapping(guest, reg)) {
		vmm_printf("%s: Region for %s/%s overlapping with "
			   "a previous node\n", __func__, 
			   guest->name, rnode->name);
		vmm_free(reg);
		return VMM_EINVALID;
	}

	/* Reserve host RAM for reserved RAM/ROM regions */
	if (!(reg->flags & (VMM_REGION_ALIAS | VMM_REGION_VIRTUAL)) &&
	    (reg->flags & (VMM_REGION_ISRAM | VMM_REGION_ISROM)) &&
	    (reg->flags & VMM_REGION_ISRESERVED)) {
		rc = vmm_host_ram_reserve(reg->hphys_addr,
					  reg->phys_size);
		if (rc) {
			vmm_printf("%s: Failed to reserve "
				   "host RAM for %s/%s\n",
				   __func__, guest->name,
				   rnode->name);
			vmm_free(reg);
			return rc;
		} else {
			reg->flags |= VMM_REGION_ISHOSTRAM;
		}
	}

	/* Allocate host RAM for alloced RAM/ROM regions */
	if (!(reg->flags & (VMM_REGION_ALIAS | VMM_REGION_VIRTUAL)) &&
	    (reg->flags & (VMM_REGION_ISRAM | VMM_REGION_ISROM)) &&
	    (reg->flags & VMM_REGION_ISALLOCED)) {
		if (!vmm_host_ram_alloc(&reg->hphys_addr,
					reg->phys_size,
					reg->align_order)) {
			vmm_printf("%s: Failed to alloc "
				   "host RAM for %s/%s\n",
				   __func__, guest->name,
				   rnode->name);
			vmm_free(reg);
			return VMM_ENOMEM;
		} else {
			reg->flags |= VMM_REGION_ISHOSTRAM;
		}
	}

	/* Probe device emulation for virtual regions */
	if (reg->flags & VMM_REGION_VIRTUAL) {
		if ((rc = vmm_devemu_probe_region(guest, reg))) {
			vmm_free(reg);
			return rc;
		}
	}

	/* Add region to region list */
	vmm_write_lock_irqsave_lite(&aspace->reg_list_lock, flags);
	list_add_tail(&reg->head, &aspace->reg_list);
	vmm_write_unlock_irqrestore_lite(&aspace->reg_list_lock, flags);

	if (new_reg) {
		*new_reg = reg;
	}

	return VMM_OK;
}

static int region_del(struct vmm_guest *guest,
		      struct vmm_region *reg,
		      bool reg_list_del)
{
	int rc = VMM_OK;
	irq_flags_t flags;
	struct vmm_guest_aspace *aspace = &guest->aspace;

	/* Remove it from region list if not removed already */
	if (reg_list_del) {
		vmm_write_lock_irqsave_lite(&aspace->reg_list_lock, flags);
		list_del(&reg->head);
		vmm_write_unlock_irqrestore_lite(&aspace->reg_list_lock, flags);
	}

	/* Remove emulator for if virtual region */
	if (reg->flags & VMM_REGION_VIRTUAL) {
		vmm_devemu_remove_region(guest, reg);
	}

	/* Free host RAM if region has alloced/reserved host RAM */
	if (!(reg->flags & (VMM_REGION_ALIAS | VMM_REGION_VIRTUAL)) &&
	    (reg->flags & (VMM_REGION_ISRAM | VMM_REGION_ISROM)) &&
	    (reg->flags & VMM_REGION_ISHOSTRAM)) {
		rc = vmm_host_ram_free(reg->hphys_addr,
					reg->phys_size);
		if (rc) {
			vmm_printf("%s: Failed to free host RAM "
				   "for %s/%s (error %d)\n",
				   __func__, guest->name,
				   reg->node->name, rc);
		}
	}

	/* Free the region */
	vmm_free(reg);

	return rc;
}

int vmm_guest_aspace_reset(struct vmm_guest *guest)
{
	irq_flags_t flags;
	struct dlist *l, *l1;
	struct vmm_guest_aspace *aspace;
	struct vmm_region *reg = NULL;

	/* Sanity Check */
	if (!guest) {
		return VMM_EFAIL;
	}
	if (!guest->aspace.initialized) {
		return VMM_ENOTAVAIL;
	}
	aspace = &guest->aspace;

	/* Reset device emulation for virtual regions */
	vmm_read_lock_irqsave_lite(&aspace->reg_list_lock, flags);
	list_for_each_safe(l, l1, &aspace->reg_list) {
		reg = list_entry(l, struct vmm_region, head);
		vmm_read_unlock_irqrestore_lite(&aspace->reg_list_lock, flags);
		if (reg->flags & VMM_REGION_VIRTUAL) {
			vmm_devemu_reset_region(guest, reg);
		}
		vmm_read_lock_irqsave_lite(&aspace->reg_list_lock, flags);
	}
	vmm_read_unlock_irqrestore_lite(&aspace->reg_list_lock, flags);

	/* Reset device emulation context */
	return vmm_devemu_reset_context(guest);
}

int vmm_guest_add_region(struct vmm_guest *guest,
			 const char *name,
			 const char *device_type,
			 const char *mainfest_type,
			 const char *address_type,
			 const char *compatible,
			 physical_addr_t gphys_addr,
			 physical_addr_t hphys_addr,
			 physical_size_t phys_size,
			 u32 align_order)
{
	int rc;
	struct vmm_devtree_node *rnode;

	/* Sanity checks */
	if (!guest || !guest->aspace.node ||
	    !name || !device_type|| !mainfest_type || !address_type) {
		rc = VMM_EINVALID;
		goto failed;
	}
	if (!guest->aspace.initialized) {
		rc = VMM_ENOTAVAIL;
		goto failed;
	}

	/* Create region node */
	rnode = vmm_devtree_addnode(guest->aspace.node, name);
	if (!rnode) {
		rc = VMM_EINVALID;
		goto failed;
	}

	/* Set device type */
	rc = vmm_devtree_setattr(rnode,
				 VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME,
				 (void *)device_type,
				 VMM_DEVTREE_ATTRTYPE_STRING,
				 strlen(device_type));
	if (rc) {
		goto failed_delnode;
	}

	/* Set manifest type */
	rc = vmm_devtree_setattr(rnode,
				 VMM_DEVTREE_MANIFEST_TYPE_ATTR_NAME,
				 (void *)mainfest_type,
				 VMM_DEVTREE_ATTRTYPE_STRING,
				 strlen(mainfest_type));
	if (rc) {
		goto failed_delnode;
	}

	/* Set address type */
	rc = vmm_devtree_setattr(rnode,
				 VMM_DEVTREE_ADDRESS_TYPE_ATTR_NAME,
				 (void *)address_type,
				 VMM_DEVTREE_ATTRTYPE_STRING,
				 strlen(address_type));
	if (rc) {
		goto failed_delnode;
	}

	/* Set compatible */
	if (compatible) {
		rc = vmm_devtree_setattr(rnode,
					 VMM_DEVTREE_COMPATIBLE_ATTR_NAME,
					 (void *)compatible,
					 VMM_DEVTREE_ATTRTYPE_STRING,
					 strlen(compatible));
		if (rc) {
			goto failed_delnode;
		}
	}

	/* Set guest physical address */
	rc = vmm_devtree_setattr(rnode,
				 VMM_DEVTREE_GUEST_PHYS_ATTR_NAME,
				 &gphys_addr,
				 VMM_DEVTREE_ATTRTYPE_PHYSADDR,
				 sizeof(gphys_addr));
	if (rc) {
		goto failed_delnode;
	}

	if (!strcmp(mainfest_type, VMM_DEVTREE_MANIFEST_TYPE_VAL_REAL)) {
		/* Set host physical address */
		rc = vmm_devtree_setattr(rnode,
					 VMM_DEVTREE_HOST_PHYS_ATTR_NAME,
					 &hphys_addr,
					 VMM_DEVTREE_ATTRTYPE_PHYSADDR,
					 sizeof(hphys_addr));
		if (rc) {
			goto failed_delnode;
		}
	} else if (!strcmp(mainfest_type,
			   VMM_DEVTREE_MANIFEST_TYPE_VAL_ALIAS)) {
		/* Set alias physical address */
		rc = vmm_devtree_setattr(rnode,
					 VMM_DEVTREE_ALIAS_PHYS_ATTR_NAME,
					 &hphys_addr,
					 VMM_DEVTREE_ATTRTYPE_PHYSADDR,
					 sizeof(hphys_addr));
		if (rc) {
			goto failed_delnode;
		}
	}

	/* Set physical size */
	rc = vmm_devtree_setattr(rnode,
				 VMM_DEVTREE_PHYS_SIZE_ATTR_NAME,
				 &phys_size,
				 VMM_DEVTREE_ATTRTYPE_PHYSSIZE,
				 sizeof(phys_size));
	if (rc) {
		goto failed_delnode;
	}

	/* Set alignment order */
	rc = vmm_devtree_setattr(rnode,
				 VMM_DEVTREE_ALIGN_ORDER_ATTR_NAME,
				 &align_order,
				 VMM_DEVTREE_ATTRTYPE_UINT32,
				 sizeof(align_order));
	if (rc) {
		goto failed_delnode;
	}

	/* Add region */
	rc = region_add(guest, rnode, NULL);
	if (rc) {
		goto failed_delnode;
	}

	return VMM_OK;

failed_delnode:
	vmm_devtree_delnode(rnode);
failed:
	return rc;
	
}

int vmm_guest_del_region(struct vmm_guest *guest,
			 struct vmm_region *reg)
{
	int rc;
	struct vmm_devtree_node *rnode;

	/* Sanity checks */
	if (!guest || !reg || !reg->node) {
		return VMM_EINVALID;
	}
	if (reg->aspace->guest != guest) {
		return VMM_EINVALID;
	}
	if (!guest->aspace.initialized) {
		return VMM_ENOTAVAIL;
	}
	rnode = reg->node;

	/* Delete region */
	rc = region_del(guest, reg, TRUE);
	if (rc) {
		return rc;
	}

	/* Delete region node */
	vmm_devtree_delnode(rnode);

	return VMM_OK;
}

int vmm_guest_aspace_init(struct vmm_guest *guest)
{
	int rc;
	struct vmm_guest_aspace *aspace;
	struct vmm_devtree_node *rnode = NULL;

	/* Sanity Check */
	if (!guest) {
		return VMM_EFAIL;
	}
	if (guest->aspace.initialized) {
		return VMM_EINVALID;
	}
	aspace = &guest->aspace;

	/* Reset the address space for guest */
	memset(aspace, 0, sizeof(struct vmm_guest_aspace));

	/* Initialize address space of guest */
	aspace->node = vmm_devtree_getchild(guest->node, 
					VMM_DEVTREE_ADDRSPACE_NODE_NAME);
	if (!aspace->node) {
		vmm_printf("%s: %s/aspace node not found\n",
			   __func__, guest->name);
		return VMM_EFAIL;
	}
	aspace->guest = guest;
	INIT_RW_LOCK(&aspace->reg_list_lock);
	INIT_LIST_HEAD(&aspace->reg_list);
	guest->aspace.devemu_priv = NULL;

	/* Initialize device emulation context */
	if ((rc = vmm_devemu_init_context(guest))) {
		return rc;
	}

	/* Create regions */
	list_for_each_entry(rnode, &aspace->node->child_list, head) {
		rc = region_add(guest, rnode, NULL);
		if (rc) {
			return rc;
		}
	}

	/* Mark address space as initialized */
	aspace->initialized = TRUE;

	return VMM_OK;
}

int vmm_guest_aspace_deinit(struct vmm_guest *guest)
{
	int rc;
	irq_flags_t flags;
	struct vmm_guest_aspace *aspace;
	struct vmm_region *reg = NULL;

	/* Sanity Check */
	if (!guest) {
		return VMM_EFAIL;
	}
	aspace = &guest->aspace;

	/* Mark address space as uninitialized */
	aspace->initialized = FALSE;

	/* Lock region list */
	vmm_write_lock_irqsave_lite(&aspace->reg_list_lock, flags);

	/* One-by-one remove all regions */
	while (!list_empty(&guest->aspace.reg_list)) {
		reg = list_first_entry(&guest->aspace.reg_list,
					struct vmm_region, head);
		list_del(&reg->head);

		/* Delete the region */
		vmm_write_unlock_irqrestore_lite(&aspace->reg_list_lock, flags);
		region_del(guest, reg, FALSE);
		vmm_write_lock_irqsave_lite(&aspace->reg_list_lock, flags);
	}

	/* Reset region list */
	INIT_LIST_HEAD(&aspace->reg_list);

	/* Unlock region list */
	vmm_write_unlock_irqrestore_lite(&aspace->reg_list_lock, flags);

	/* DeInitialize device emulation context */
	if ((rc = vmm_devemu_deinit_context(guest))) {
		return rc;
	}

	/* Reset devemu_priv pointer of aspace */
	guest->aspace.devemu_priv = NULL;

	return VMM_OK;
}

