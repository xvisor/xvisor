/**
 * Copyright (c) 2010 Anup Patel.
 * All rights reserved.
 *
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * Modified by Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 * to add region overlapping debug message.
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
#include <vmm_notifier.h>
#include <arch_guest.h>
#include <libs/stringlib.h>

static BLOCKING_NOTIFIER_CHAIN(guest_aspace_notifier_chain);

int vmm_guest_aspace_register_client(struct vmm_notifier_block *nb)
{
	int rc = vmm_blocking_notifier_register(&guest_aspace_notifier_chain,
						nb);

	return rc;
}

int vmm_guest_aspace_unregister_client(struct vmm_notifier_block *nb)
{
	int rc = vmm_blocking_notifier_unregister(&guest_aspace_notifier_chain,
						  nb);

	return rc;
}

void vmm_guest_iterate_region(struct vmm_guest *guest, u32 reg_flags,
		void (*func)(struct vmm_guest *, struct vmm_region *, void *),
		void *priv)
{
	irq_flags_t flags;
	vmm_rwlock_t *root_lock = NULL;
	struct rb_root *root = NULL;
	struct vmm_region *reg = NULL, *n = NULL;
	struct vmm_guest_aspace *aspace;

	if (!guest || !func) {
		return;
	}
	aspace = &guest->aspace;

	/* Find out region tree root */
	if (reg_flags & VMM_REGION_IO) {
		root = &aspace->reg_iotree;
		root_lock = &aspace->reg_iotree_lock;
	} else {
		root = &aspace->reg_memtree;
		root_lock = &aspace->reg_memtree_lock;
	}

	/* Post-order traversal for rbtree nodes */
	vmm_read_lock_irqsave_lite(root_lock, flags);
	rbtree_postorder_for_each_entry_safe(reg, n, root, head) {
		if ((reg->flags & reg_flags) == reg_flags) {
			vmm_read_unlock_irqrestore_lite(root_lock, flags);
			func(guest, reg, priv);
			vmm_read_lock_irqsave_lite(root_lock, flags);
		}
	}
	vmm_read_unlock_irqrestore_lite(root_lock, flags);
}

struct vmm_region *vmm_guest_find_region(struct vmm_guest *guest,
					 physical_addr_t gphys_addr,
					 u32 reg_flags, bool resolve_alias)
{
	bool found = FALSE;
	u32 cmp_flags;
	irq_flags_t flags;
	vmm_rwlock_t *root_lock = NULL;
	struct rb_root *root = NULL;
	struct rb_node *pos = NULL;
	struct vmm_region *reg = NULL;
	struct vmm_guest_aspace *aspace;

	if (!guest) {
		return NULL;
	}
	aspace = &guest->aspace;

	/* Determine flags we need to compare */
	cmp_flags = reg_flags & ~VMM_REGION_MANIFEST_MASK;

	/* Find out region tree root */
	if (reg_flags & VMM_REGION_IO) {
		root = &aspace->reg_iotree;
		root_lock = &aspace->reg_iotree_lock;
	} else {
		root = &aspace->reg_memtree;
		root_lock = &aspace->reg_memtree_lock;
	}

	/* Try to find region ignoring required manifest flags */
	reg = NULL;
	found = FALSE;
	vmm_read_lock_irqsave_lite(root_lock, flags);
	pos = root->rb_node;
	while (pos) {
		reg = rb_entry(pos, struct vmm_region, head);
		if (gphys_addr < VMM_REGION_GPHYS_START(reg)) {
			pos = pos->rb_left;
		} else if (VMM_REGION_GPHYS_END(reg) <= gphys_addr) {
			pos = pos->rb_right;
		} else {
			if ((reg->flags & cmp_flags) == cmp_flags) {
				found = TRUE;
			}
			break;
		}
	}
	vmm_read_unlock_irqrestore_lite(root_lock, flags);
	if (!found) {
		return NULL;
	}

	/* Check if we can skip resolve alias */
	if (!resolve_alias) {
		goto done;
	}

	/* Resolve aliased regions */
	while (reg->flags & VMM_REGION_ALIAS) {
		gphys_addr = VMM_REGION_GPHYS_TO_APHYS(reg, gphys_addr);
		reg = NULL;
		found = FALSE;
		vmm_read_lock_irqsave_lite(root_lock, flags);
		pos = root->rb_node;
		while (pos) {
			reg = rb_entry(pos, struct vmm_region, head);
			if (gphys_addr < VMM_REGION_GPHYS_START(reg)) {
				pos = pos->rb_left;
			} else if (VMM_REGION_GPHYS_END(reg) <= gphys_addr) {
				pos = pos->rb_right;
			} else {
				if ((reg->flags & cmp_flags) == cmp_flags) {
					found = TRUE;
				}
				break;
			}
		}
		vmm_read_unlock_irqrestore_lite(root_lock, flags);
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

static physical_addr_t mapping_gphys_offset(struct vmm_region *reg,
					    u32 map_index)
{
	if (reg->maps_count <= map_index) {
		return reg->phys_size;
	}

	return ((physical_addr_t)map_index) << reg->map_order;
}

static physical_size_t mapping_phys_size(struct vmm_region *reg,
					 u32 map_index)
{
	physical_size_t map_size;
	physical_size_t size;

	if (reg->maps_count <= map_index) {
		return 0;
	}
	map_size = ((physical_size_t)1) << reg->map_order;

	size = reg->phys_size - mapping_gphys_offset(reg, map_index);

	return (size < map_size) ? size : map_size;
}

static struct vmm_region_mapping *mapping_find(struct vmm_guest *guest,
					       struct vmm_region *reg,
					       u32 *map_index,
					       physical_addr_t gphys_addr)
{
	u32 i;

	if ((gphys_addr < VMM_REGION_GPHYS_START(reg)) ||
	    (VMM_REGION_GPHYS_END(reg) <= gphys_addr)) {
		return NULL;
	}

	i = (gphys_addr - VMM_REGION_GPHYS_START(reg)) >> reg->map_order;
	if (map_index) {
		*map_index = i;
	}

	return &reg->maps[i];
}

void vmm_guest_find_mapping(struct vmm_guest *guest,
			    struct vmm_region *reg,
			    physical_addr_t gphys_addr,
			    physical_addr_t *hphys_addr,
			    physical_size_t *avail_size)
{
	u32 i;
	physical_addr_t map_gphys_addr;
	physical_addr_t hphys = 0;
	physical_size_t size = 0;
	struct vmm_region_mapping *map;

	if (!guest || !reg) {
		goto done;
	}

	map = mapping_find(guest, reg, &i, gphys_addr);
	if (!map) {
		goto done;
	}
	map_gphys_addr = reg->gphys_addr + mapping_gphys_offset(reg, i);

	hphys = map->hphys_addr + (gphys_addr - map_gphys_addr);
	size = map->hphys_addr + mapping_phys_size(reg, i) - hphys;

done:
	if (hphys_addr) {
		*hphys_addr = hphys;
	}
	if (avail_size) {
		*avail_size = size;
	}
}

void vmm_guest_iterate_mapping(struct vmm_guest *guest,
				struct vmm_region *reg,
				void (*func)(struct vmm_guest *guest,
					     struct vmm_region *reg,
					     physical_addr_t gphys_addr,
					     physical_addr_t hphys_addr,
					     physical_size_t phys_size,
					     void *priv),
				void *priv)
{
	u32 i;

	if (!guest || !reg || !func) {
		return;
	}

	for (i = 0; i < reg->maps_count; i++) {
		func(guest, reg,
		     reg->gphys_addr + mapping_gphys_offset(reg, i),
		     reg->maps[i].hphys_addr,
		     mapping_phys_size(reg, i),
		     priv);
	}
}

int vmm_guest_overwrite_real_device_mapping(struct vmm_guest *guest,
					    struct vmm_region *reg,
					    physical_addr_t gphys_addr,
					    physical_addr_t hphys_addr)
{
	struct vmm_region_mapping *map;

	if (!guest || !reg) {
		return VMM_EINVALID;
	}
	if (!(reg->flags & VMM_REGION_REAL) ||
	    !(reg->flags & VMM_REGION_ISDEVICE)) {
		return VMM_EINVALID;
	}

	map = mapping_find(guest, reg, NULL, gphys_addr);
	if (!map) {
		return VMM_EINVALID;
	}

	map->hphys_addr = hphys_addr;

	return VMM_OK;
}

u32 vmm_guest_memory_read(struct vmm_guest *guest,
			  physical_addr_t gphys_addr,
			  void *dst, u32 len, bool cacheable)
{
	u32 bytes_read = 0, to_read;
	physical_size_t avail_size;
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

		vmm_guest_find_mapping(guest, reg, gphys_addr,
				       &hphys_addr, &avail_size);
		to_read = (avail_size < U32_MAX) ? avail_size : U32_MAX;
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
	physical_size_t avail_size;
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

		vmm_guest_find_mapping(guest, reg, gphys_addr,
				       &hphys_addr, &avail_size);
		to_write = (avail_size < U32_MAX) ? avail_size : U32_MAX;
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
			   physical_size_t *phys_size,
			   u32 *reg_flags)
{
	physical_addr_t hphys;
	physical_size_t size;
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
		gphys_addr = VMM_REGION_GPHYS_TO_APHYS(reg, gphys_addr);
		reg = vmm_guest_find_region(guest, gphys_addr,
					    VMM_REGION_MEMORY, FALSE);
		if (!reg) {
			return VMM_EFAIL;
		}
	}

	vmm_guest_find_mapping(guest, reg, gphys_addr, &hphys, &size);

	if (gphys_size < size) {
		size = gphys_size;
	}

	if (hphys_addr) {
		*hphys_addr = hphys;
	}

	if (phys_size) {
		*phys_size = size;
	}

	if (reg_flags) {
		*reg_flags = reg->flags;
	}

	return VMM_OK;
}

int vmm_guest_physical_unmap(struct vmm_guest *guest,
			     physical_addr_t gphys_addr,
			     physical_size_t phys_size)
{
	/* We don't have dynamic mappings for guest regions
	 * so nothing to do here.
	 */
	return VMM_OK;
}

bool is_region_node_valid(struct vmm_devtree_node *rnode)
{
	u32 order;
	const char *aval;
	bool is_real = FALSE;
	bool is_alias = FALSE;
	bool is_alloced = FALSE;
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

	if (vmm_devtree_read_string(rnode,
			VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME, &aval)) {
		return FALSE;
	}
	if (!strcmp(aval, VMM_DEVTREE_DEVICE_TYPE_VAL_ALLOCED_RAM) ||
	    !strcmp(aval, VMM_DEVTREE_DEVICE_TYPE_VAL_ALLOCED_ROM)) {
		is_alloced = TRUE;
	}

	if (vmm_devtree_read_physaddr(rnode,
			VMM_DEVTREE_GUEST_PHYS_ATTR_NAME, &addr)) {
		return FALSE;
	}

	if (is_real && !is_alloced) {
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
				  struct vmm_region *reg,
				  struct vmm_region **overlapping)
{
	bool ret = FALSE;
	irq_flags_t flags;
	vmm_rwlock_t *root_lock = NULL;
	struct rb_root *root = NULL;
	struct rb_node *pos = NULL;
	struct vmm_region *treg = NULL;
	struct vmm_guest_aspace *aspace = &guest->aspace;

	if (reg->flags & VMM_REGION_IO) {
		root = &aspace->reg_iotree;
		root_lock = &aspace->reg_iotree_lock;
	} else {
		root = &aspace->reg_memtree;
		root_lock = &aspace->reg_memtree_lock;
	}

	vmm_read_lock_irqsave_lite(root_lock, flags);

	pos = root->rb_node;
	while (pos) {
		treg = rb_entry(pos, struct vmm_region, head);
		if (VMM_REGION_GPHYS_END(reg) <=
				VMM_REGION_GPHYS_START(treg)) {
			pos = pos->rb_left;
		} else if (VMM_REGION_GPHYS_END(treg) <=
				VMM_REGION_GPHYS_START(reg)) {
			pos = pos->rb_right;
		} else {
			if (overlapping)
				*overlapping = treg;
			ret = TRUE;
			break;
		}
	}

	vmm_read_unlock_irqrestore_lite(root_lock, flags);

	return ret;
}

static void region_overlap_message(const char *func,
				   struct vmm_guest *guest,
				   struct vmm_region *reg,
				   struct vmm_region *reg_overlap)
{
	const physical_size_t reg_size = reg->gphys_addr + reg->phys_size;
	const physical_size_t overlap_reg_size = reg_overlap->gphys_addr +
		reg_overlap->phys_size;

	vmm_printf("%s: Region for %s/%s (0x%"PRIPADDR" - 0x%"PRIPADDR") "
		   "overlaps with region %s/%s "
		   "(0x%"PRIPADDR" - 0x%"PRIPADDR")\n",
		   func, guest->name, reg->node->name,
		   reg->gphys_addr, reg_size,
		   guest->name, reg_overlap->node->name,
		   reg_overlap->gphys_addr, overlap_reg_size);
}

static int region_add(struct vmm_guest *guest,
		      struct vmm_devtree_node *rnode,
		      struct vmm_region **new_reg,
		      void *rpriv,
		      bool add_probe_list)
{
	u32 i;
	int rc;
	const char *aval;
	irq_flags_t flags;
	vmm_rwlock_t *root_lock = NULL;
	struct dlist *root_plist = NULL;
	struct rb_root *root = NULL;
	struct rb_node **new = NULL, *pnode = NULL;
	struct vmm_region *reg = NULL, *pnode_reg = NULL;
	struct vmm_guest_aspace *aspace = &guest->aspace;
	struct vmm_region *reg_overlap = NULL;

	/* Increment ref count of region node */
	vmm_devtree_ref_node(rnode);

	/* Sanity check on region node */
	if (!is_region_node_valid(rnode)) {
		rc = VMM_EINVALID;
		goto region_fail;
	}

	/* Allocate region instance */
	reg = vmm_zalloc(sizeof(struct vmm_region));
	RB_CLEAR_NODE(&reg->head);
	INIT_LIST_HEAD(&reg->phead);

	/* Fillup region details */
	reg->node = rnode;
	reg->aspace = aspace;
	reg->flags = 0x0;

	rc = vmm_devtree_read_string(reg->node,
			VMM_DEVTREE_MANIFEST_TYPE_ATTR_NAME, &aval);
	if (rc) {
		goto region_free_fail;
	}

	if (!strcmp(aval, VMM_DEVTREE_MANIFEST_TYPE_VAL_REAL)) {
		reg->flags |= VMM_REGION_REAL;
	} else if (!strcmp(aval,
			VMM_DEVTREE_MANIFEST_TYPE_VAL_ALIAS)) {
		reg->flags |= VMM_REGION_ALIAS;
	} else {
		reg->flags |= VMM_REGION_VIRTUAL;
	}

	rc = vmm_devtree_read_string(reg->node,
			VMM_DEVTREE_ADDRESS_TYPE_ATTR_NAME, &aval);
	if (rc) {
		goto region_free_fail;
	}

	if (!strcmp(aval, VMM_DEVTREE_ADDRESS_TYPE_VAL_IO)) {
		reg->flags |= VMM_REGION_IO;
	} else {
		reg->flags |= VMM_REGION_MEMORY;
	}

	rc = vmm_devtree_read_string(reg->node,
			VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME, &aval);
	if (rc) {
		goto region_free_fail;
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

	rc = vmm_devtree_read_physaddr(reg->node,
				VMM_DEVTREE_GUEST_PHYS_ATTR_NAME,
				&reg->gphys_addr);
	if (rc) {
		goto region_free_fail;
	}

	if (reg->flags & VMM_REGION_ALIAS) {
		rc = vmm_devtree_read_physaddr(reg->node,
				VMM_DEVTREE_ALIAS_PHYS_ATTR_NAME,
				&reg->aphys_addr);
		if (rc) {
			goto region_free_fail;
		}
	} else {
		reg->aphys_addr = reg->gphys_addr;
	}

	rc = vmm_devtree_read_physsize(reg->node,
			VMM_DEVTREE_PHYS_SIZE_ATTR_NAME,
			&reg->phys_size);
	if (rc) {
		goto region_free_fail;
	}

	rc = vmm_devtree_read_u32(reg->node,
			VMM_DEVTREE_ALIGN_ORDER_ATTR_NAME,
			&reg->align_order);
	if (rc) {
		reg->align_order = 0;
	}

	/* Compute mapping order for guest region */
	reg->map_order = VMM_PAGE_SHIFT;
	for (i = VMM_PAGE_SHIFT; i < 64; i++) {
		if (reg->phys_size <= ((u64)1 << i)) {
			reg->map_order = i;
			break;
		}
	}
	if (i == 64) {
		rc = VMM_EINVALID;
		goto region_free_fail;
	}

	/*
	 * Overwrite mapping order for alloced RAM/ROM regions
	 * based on align_order or map_order DT attribute
	 */
	if (!(reg->flags & (VMM_REGION_ALIAS | VMM_REGION_VIRTUAL)) &&
	    (reg->flags & (VMM_REGION_ISRAM | VMM_REGION_ISROM)) &&
	    (reg->flags & VMM_REGION_ISALLOCED)) {
		if ((VMM_PAGE_SHIFT <= reg->align_order) &&
		    (reg->align_order < reg->map_order)) {
			reg->map_order = reg->align_order;
		}

		i = 0;
		rc = vmm_devtree_read_u32(reg->node,
				VMM_DEVTREE_MAP_ORDER_ATTR_NAME, &i);
		if (!rc && (VMM_PAGE_SHIFT <= i)) {
			reg->map_order = i;
		}
	}

	/* Compute number of mappings for guest region */
	reg->maps_count = reg->phys_size >> reg->map_order;
	if ((((physical_size_t)reg->maps_count) << reg->map_order)
							< reg->phys_size) {
		reg->maps_count++;
	}

	/* Allocate mappings for guest region */
	reg->maps = vmm_zalloc(sizeof(*reg->maps) * reg->maps_count);
	if (!reg->maps) {
		rc = VMM_ENOMEM;
		goto region_free_fail;
	}
	reg->maps[0].hphys_addr = reg->gphys_addr +
				  mapping_gphys_offset(reg, 0);
	reg->maps[0].flags = 0;
	for (i = 1; i < reg->maps_count; i++) {
		reg->maps[i].hphys_addr = reg->gphys_addr +
					  mapping_gphys_offset(reg, i);
		reg->maps[i].flags = 0;
	}

	/* Mapping0 from device tree for non-alloced real guest region */
	if ((reg->flags & VMM_REGION_REAL) &&
	    !(reg->flags & VMM_REGION_ISALLOCED)) {
		rc = vmm_devtree_read_physaddr(reg->node,
					VMM_DEVTREE_HOST_PHYS_ATTR_NAME,
					&reg->maps[0].hphys_addr);
		if (rc) {
			goto region_free_maps_fail;
		}
	}

	reg->devemu_priv = NULL;
	reg->priv = rpriv;

	/* Ensure region does not overlap other regions */
	if (is_region_overlapping(guest, reg, &reg_overlap)) {
		region_overlap_message(__func__, guest, reg, reg_overlap);
		rc = VMM_EINVALID;
		goto region_free_maps_fail;
	}

	/* Reserve host RAM for reserved RAM/ROM regions */
	if (!(reg->flags & (VMM_REGION_ALIAS | VMM_REGION_VIRTUAL)) &&
	    (reg->flags & (VMM_REGION_ISRAM | VMM_REGION_ISROM)) &&
	    (reg->flags & VMM_REGION_ISRESERVED)) {
		for (i = 0; i < reg->maps_count; i++) {
			rc = vmm_host_ram_reserve(reg->maps[i].hphys_addr,
						  mapping_phys_size(reg, i));
			if (rc) {
				vmm_printf("%s: Failed to reserve "
					   "host RAM for %s/%s\n",
					   __func__, guest->name,
					   reg->node->name);
				goto region_ram_free_fail;
			} else {
				reg->maps[i].flags |=
					VMM_REGION_MAPPING_ISHOSTRAM;
			}
		}
	}

	/* Allocate host RAM for alloced RAM/ROM regions */
	if (!(reg->flags & (VMM_REGION_ALIAS | VMM_REGION_VIRTUAL)) &&
	    (reg->flags & (VMM_REGION_ISRAM | VMM_REGION_ISROM)) &&
	    (reg->flags & VMM_REGION_ISALLOCED)) {
		for (i = 0; i < reg->maps_count; i++) {
			if (!vmm_host_ram_alloc(&reg->maps[i].hphys_addr,
						mapping_phys_size(reg, i),
						reg->align_order)) {
				vmm_printf("%s: Failed to alloc "
					   "host RAM for %s/%s\n",
					   __func__, guest->name,
					   reg->node->name);
				rc = VMM_ENOMEM;
				goto region_ram_free_fail;
			} else {
				reg->maps[i].flags |=
					VMM_REGION_MAPPING_ISHOSTRAM;
				if (reg->flags & VMM_REGION_ISROM) {
					vmm_host_memory_set(
						reg->maps[i].hphys_addr, 0,
						mapping_phys_size(reg, i),
						FALSE);
				}
			}
		}
	}

	/* Probe device emulation for real & virtual device regions */
	if ((reg->flags & VMM_REGION_ISDEVICE) &&
	    !(reg->flags & VMM_REGION_ALIAS)) {
		if ((rc = vmm_devemu_probe_region(guest, reg))) {
			goto region_ram_free_fail;
		}
	}

	/* Call arch specific add region callback */
	rc = arch_guest_add_region(guest, reg);
        if (rc) {
		goto region_unprobe_fail;
	}

	/* Add region to tree and probe list */
	if (reg->flags & VMM_REGION_IO) {
		root = &aspace->reg_iotree;
		root_plist = &aspace->reg_ioprobe_list;
		root_lock = &aspace->reg_iotree_lock;
	} else {
		root = &aspace->reg_memtree;
		root_plist = &aspace->reg_memprobe_list;
		root_lock = &aspace->reg_memtree_lock;
	}
	vmm_write_lock_irqsave_lite(root_lock, flags);
	new = &root->rb_node;
	while (*new) {
		pnode = *new;
		pnode_reg = rb_entry(pnode, struct vmm_region, head);
		if (VMM_REGION_GPHYS_END(reg) <=
				VMM_REGION_GPHYS_START(pnode_reg)) {
			new = &pnode->rb_left;
		} else if (VMM_REGION_GPHYS_END(pnode_reg) <=
				VMM_REGION_GPHYS_START(reg)) {
			new = &pnode->rb_right;
		} else {
			rc = VMM_EINVALID;
			vmm_write_unlock_irqrestore_lite(root_lock, flags);
			goto region_arch_del_fail;
		}
	}
	rb_link_node(&reg->head, pnode, new);
	rb_insert_color(&reg->head, root);
	if (add_probe_list) {
		list_add_tail(&reg->phead, root_plist);
	}
	vmm_write_unlock_irqrestore_lite(root_lock, flags);

	if (new_reg) {
		*new_reg = reg;
	}

	return VMM_OK;

region_arch_del_fail:
	arch_guest_del_region(guest, reg);
region_unprobe_fail:
	if ((reg->flags & VMM_REGION_ISDEVICE) &&
	    !(reg->flags & VMM_REGION_ALIAS)) {
		vmm_devemu_remove_region(guest, reg);
	}
region_ram_free_fail:
	if (!(reg->flags & (VMM_REGION_ALIAS | VMM_REGION_VIRTUAL)) &&
	    (reg->flags & (VMM_REGION_ISRAM | VMM_REGION_ISROM))) {
		for (i = 0; i < reg->maps_count; i++) {
			if (!(reg->maps[i].flags &
			      VMM_REGION_MAPPING_ISHOSTRAM))
				continue;
			vmm_host_ram_free(reg->maps[i].hphys_addr,
					  mapping_phys_size(reg, i));
			reg->maps[i].flags &=
				~VMM_REGION_MAPPING_ISHOSTRAM;
		}
	}
region_free_maps_fail:
	vmm_free(reg->maps);
region_free_fail:
	vmm_free(reg);
region_fail:
	vmm_devtree_dref_node(rnode);
	return rc;
}

static int region_del(struct vmm_guest *guest,
		      struct vmm_region *reg,
		      bool del_reg_tree,
		      bool del_probe_list)
{
	u32 i;
	int rc = VMM_OK;
	irq_flags_t flags;
	vmm_rwlock_t *root_lock;
	struct rb_root *root = NULL;
	struct vmm_devtree_node *rnode = reg->node;
	struct vmm_guest_aspace *aspace = &guest->aspace;

	/* Remove it from region tree if not removed already */
	if (del_reg_tree) {
		if (reg->flags & VMM_REGION_IO) {
			root = &aspace->reg_iotree;
			root_lock = &aspace->reg_iotree_lock;
		} else {
			root = &aspace->reg_memtree;
			root_lock = &aspace->reg_memtree_lock;
		}
		vmm_write_lock_irqsave_lite(root_lock, flags);
		rb_erase(&reg->head, root);
		vmm_write_unlock_irqrestore_lite(root_lock, flags);
	}

	/* Remove it from probe list if not removed already */
	if (del_probe_list) {
		if (reg->flags & VMM_REGION_IO) {
			root_lock = &aspace->reg_iotree_lock;
		} else {
			root_lock = &aspace->reg_memtree_lock;
		}
		vmm_write_lock_irqsave_lite(root_lock, flags);
		list_del(&reg->phead);
		vmm_write_unlock_irqrestore_lite(root_lock, flags);
	}

	/* Call arch specific del region callback */
	rc = arch_guest_del_region(guest, reg);
	if (rc) {
		vmm_printf("%s: arch_guest_del_region() failed for %s/%s "
			   "(error %d)\n", __func__, guest->name,
			   reg->node->name, rc);
	}

	/* Remove emulator for if virtual region */
	if ((reg->flags & VMM_REGION_ISDEVICE) &&
	    !(reg->flags & VMM_REGION_ALIAS)) {
		vmm_devemu_remove_region(guest, reg);
	}

	/* Free host RAM if region has alloced/reserved host RAM */
	if (!(reg->flags & (VMM_REGION_ALIAS | VMM_REGION_VIRTUAL)) &&
	    (reg->flags & (VMM_REGION_ISRAM | VMM_REGION_ISROM))) {
		for (i = 0; i < reg->maps_count; i++) {
			if (!(reg->maps[i].flags &
			      VMM_REGION_MAPPING_ISHOSTRAM))
				continue;
			rc = vmm_host_ram_free(reg->maps[i].hphys_addr,
					       mapping_phys_size(reg, i));
			if (rc) {
				vmm_printf("%s: Failed to free host RAM "
					   "for %s/%s (error %d)\n",
					   __func__, guest->name,
					   reg->node->name, rc);
			}
			reg->maps[i].flags &= ~VMM_REGION_MAPPING_ISHOSTRAM;
		}
	}

	/* Free region mappings */
	vmm_free(reg->maps);

	/* Free the region */
	vmm_free(reg);

	/* De-reference the region node */
	vmm_devtree_dref_node(rnode);

	return rc;
}

int vmm_guest_aspace_reset(struct vmm_guest *guest)
{
	irq_flags_t flags;
	vmm_rwlock_t *root_lock = NULL;
	struct rb_root *root = NULL;
	struct vmm_guest_aspace *aspace;
	struct vmm_region *reg = NULL, *next_reg = NULL;
	struct vmm_guest_aspace_event evt;

	/* Sanity Check */
	if (!guest) {
		return VMM_EFAIL;
	}
	aspace = &guest->aspace;

	/* Reset device emulation for io regions */
	root = &aspace->reg_iotree;
	root_lock = &aspace->reg_iotree_lock;
	vmm_read_lock_irqsave_lite(root_lock, flags);
	rbtree_postorder_for_each_entry_safe(reg, next_reg, root, head) {
		vmm_read_unlock_irqrestore_lite(root_lock, flags);
		if ((reg->flags & VMM_REGION_ISDEVICE) &&
		    !(reg->flags & VMM_REGION_ALIAS)) {
			vmm_devemu_reset_region(guest, reg);
		}
		vmm_read_lock_irqsave_lite(root_lock, flags);
	}
	vmm_read_unlock_irqrestore_lite(root_lock, flags);

	/* Reset device emulation for mem regions */
	root = &aspace->reg_memtree;
	root_lock = &aspace->reg_memtree_lock;
	vmm_read_lock_irqsave_lite(root_lock, flags);
	rbtree_postorder_for_each_entry_safe(reg, next_reg, root, head) {
		vmm_read_unlock_irqrestore_lite(root_lock, flags);
		if ((reg->flags & VMM_REGION_ISDEVICE) &&
		    !(reg->flags & VMM_REGION_ALIAS)) {
			vmm_devemu_reset_region(guest, reg);
		}
		vmm_read_lock_irqsave_lite(root_lock, flags);
	}
	vmm_read_unlock_irqrestore_lite(root_lock, flags);

	/*
	 * Notify the listeners about reset event.
	 * No locks taken at this point.
	 */
	evt.guest = guest;
	evt.data = NULL;
	vmm_blocking_notifier_call(&guest_aspace_notifier_chain,
				   VMM_GUEST_ASPACE_EVENT_RESET,
				   &evt);

	/* Reset device emulation context */
	return vmm_devemu_reset_context(guest);
}

int vmm_guest_add_region_from_node(struct vmm_guest *guest,
				   struct vmm_devtree_node *node,
				   void *rpriv)
{
	int rc;
	struct vmm_region *reg = NULL;

	/* Sanity checks */
	if (!guest || !guest->aspace.node || !node) {
		return VMM_EINVALID;
	}

	/* TODO: Make sure aspace node is not parent of given node */
	/* TODO: Make sure aspace node is ancestor of given node */

	/* Add region */
	rc = region_add(guest, node, &reg, rpriv, FALSE);
	if (rc) {
		return rc;
	}

	/* Mark this region as dynamically added */
	reg->flags |= VMM_REGION_ISDYNAMIC;

	return VMM_OK;
}

int vmm_guest_add_region(struct vmm_guest *guest,
			 struct vmm_devtree_node *parent,
			 const char *name,
			 const char *device_type,
			 const char *mainfest_type,
			 const char *address_type,
			 const char *compatible,
			 u32 compatible_len,
			 physical_addr_t gphys_addr,
			 physical_addr_t aphys_addr,
			 physical_size_t phys_size,
			 u32 align_order,
			 physical_addr_t hphys_addr,
			 void *rpriv)
{
	int rc;
	struct vmm_region *reg = NULL;
	struct vmm_devtree_node *rnode;

	/* Sanity checks */
	if (!guest || !guest->aspace.node || !parent ||
	    !name || !device_type|| !mainfest_type || !address_type) {
		rc = VMM_EINVALID;
		goto failed;
	}

	/* TODO: Make sure aspace node is parent/ancestor of given
	 * parent node
	 */

	/* Create region node */
	rnode = vmm_devtree_addnode(parent, name);
	if (!rnode) {
		rc = VMM_EINVALID;
		goto failed;
	}

	/* Set device type */
	rc = vmm_devtree_setattr(rnode,
				 VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME,
				 (void *)device_type,
				 VMM_DEVTREE_ATTRTYPE_STRING,
				 strlen(device_type) + 1, FALSE);
	if (rc) {
		goto failed_delnode;
	}

	/* Set manifest type */
	rc = vmm_devtree_setattr(rnode,
				 VMM_DEVTREE_MANIFEST_TYPE_ATTR_NAME,
				 (void *)mainfest_type,
				 VMM_DEVTREE_ATTRTYPE_STRING,
				 strlen(mainfest_type) + 1, FALSE);
	if (rc) {
		goto failed_delnode;
	}

	/* Set address type */
	rc = vmm_devtree_setattr(rnode,
				 VMM_DEVTREE_ADDRESS_TYPE_ATTR_NAME,
				 (void *)address_type,
				 VMM_DEVTREE_ATTRTYPE_STRING,
				 strlen(address_type) + 1, FALSE);
	if (rc) {
		goto failed_delnode;
	}

	/* Set compatible */
	if (compatible) {
		rc = vmm_devtree_setattr(rnode,
					 VMM_DEVTREE_COMPATIBLE_ATTR_NAME,
					 (void *)compatible,
					 VMM_DEVTREE_ATTRTYPE_STRING,
					 compatible_len, FALSE);
		if (rc) {
			goto failed_delnode;
		}
	}

	/* Set guest physical address */
	rc = vmm_devtree_setattr(rnode,
				 VMM_DEVTREE_GUEST_PHYS_ATTR_NAME,
				 &gphys_addr,
				 VMM_DEVTREE_ATTRTYPE_PHYSADDR,
				 sizeof(gphys_addr), FALSE);
	if (rc) {
		goto failed_delnode;
	}

	if (!strcmp(mainfest_type, VMM_DEVTREE_MANIFEST_TYPE_VAL_REAL)) {
		/* Set host physical address */
		rc = vmm_devtree_setattr(rnode,
					 VMM_DEVTREE_HOST_PHYS_ATTR_NAME,
					 &hphys_addr,
					 VMM_DEVTREE_ATTRTYPE_PHYSADDR,
					 sizeof(hphys_addr), FALSE);
		if (rc) {
			goto failed_delnode;
		}
	} else if (!strcmp(mainfest_type,
			   VMM_DEVTREE_MANIFEST_TYPE_VAL_ALIAS)) {
		/* Set alias physical address */
		rc = vmm_devtree_setattr(rnode,
					 VMM_DEVTREE_ALIAS_PHYS_ATTR_NAME,
					 &aphys_addr,
					 VMM_DEVTREE_ATTRTYPE_PHYSADDR,
					 sizeof(aphys_addr), FALSE);
		if (rc) {
			goto failed_delnode;
		}
	}

	/* Set physical size */
	rc = vmm_devtree_setattr(rnode,
				 VMM_DEVTREE_PHYS_SIZE_ATTR_NAME,
				 &phys_size,
				 VMM_DEVTREE_ATTRTYPE_PHYSSIZE,
				 sizeof(phys_size), FALSE);
	if (rc) {
		goto failed_delnode;
	}

	/* Set alignment order */
	rc = vmm_devtree_setattr(rnode,
				 VMM_DEVTREE_ALIGN_ORDER_ATTR_NAME,
				 &align_order,
				 VMM_DEVTREE_ATTRTYPE_UINT32,
				 sizeof(align_order), FALSE);
	if (rc) {
		goto failed_delnode;
	}

	/* Add region */
	rc = region_add(guest, rnode, &reg, rpriv, FALSE);
	if (rc) {
		goto failed_delnode;
	}

	/* Mark this region as dynamically added */
	reg->flags |= VMM_REGION_ISDYNAMIC;

	return VMM_OK;

failed_delnode:
	vmm_devtree_delnode(rnode);
failed:
	return rc;
	
}

int vmm_guest_del_region(struct vmm_guest *guest,
			 struct vmm_region *reg,
			 bool del_node)
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
	if (!(reg->flags & VMM_REGION_ISDYNAMIC)) {
		return VMM_EINVALID;
	}
	rnode = reg->node;

	/* Delete region */
	rc = region_del(guest, reg, TRUE, FALSE);
	if (rc) {
		return rc;
	}

	/* Delete region node if required */
	if (del_node)
		vmm_devtree_delnode(rnode);

	return VMM_OK;
}

int vmm_guest_aspace_init(struct vmm_guest *guest)
{
	int rc;
	struct vmm_guest_aspace *aspace;
	struct vmm_devtree_node *rnode = NULL;
	struct vmm_guest_aspace_event evt;

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
	INIT_RW_LOCK(&aspace->reg_iotree_lock);
	aspace->reg_iotree = RB_ROOT;
	INIT_LIST_HEAD(&aspace->reg_ioprobe_list);
	INIT_RW_LOCK(&aspace->reg_memtree_lock);
	aspace->reg_memtree = RB_ROOT;
	INIT_LIST_HEAD(&aspace->reg_memprobe_list);
	guest->aspace.devemu_priv = NULL;

	/* Initialize device emulation context */
	if ((rc = vmm_devemu_init_context(guest))) {
		return rc;
	}

	/* Create regions */
	vmm_devtree_for_each_child(rnode, aspace->node) {
		rc = region_add(guest, rnode, NULL, NULL, TRUE);
		if (rc) {
			vmm_devtree_dref_node(rnode);
			return rc;
		}
	}

	/* Mark address space as initialized */
	aspace->initialized = TRUE;

	/*
	 * Notify the listeners that init is complete.
	 * No locks taken at this point.
	 */
	evt.guest = guest;
	evt.data = NULL;
	vmm_blocking_notifier_call(&guest_aspace_notifier_chain,
				   VMM_GUEST_ASPACE_EVENT_INIT,
				   &evt);

	return VMM_OK;
}

int vmm_guest_aspace_deinit(struct vmm_guest *guest)
{
	int rc;
	irq_flags_t flags;
	vmm_rwlock_t *root_lock;
	struct rb_root *root;
	struct dlist *root_plist;
	struct vmm_guest_aspace *aspace;
	struct vmm_region *reg = NULL;
	struct vmm_guest_aspace_event evt;

	/* Sanity Check */
	if (!guest) {
		return VMM_EFAIL;
	}
	aspace = &guest->aspace;

	/*
	 * About to deinit the guest address space. Regions
	 * are still valid. Handler should take care of
	 * internal locking, none taken at this point.
	 */
	evt.guest = guest;
	evt.data = NULL;
	vmm_blocking_notifier_call(&guest_aspace_notifier_chain,
				   VMM_GUEST_ASPACE_EVENT_DEINIT,
				   &evt);

	/* Mark address space as uninitialized */
	aspace->initialized = FALSE;

	/* One-by-one remove all io regions in reverse probing order */
	root = &aspace->reg_iotree;
	root_plist = &aspace->reg_ioprobe_list;
	root_lock = &aspace->reg_iotree_lock;
	vmm_write_lock_irqsave_lite(root_lock, flags);
	while (!list_empty(root_plist)) {
		/* Get last region from probe list */
		reg = list_entry(list_pop_tail(root_plist),
				 struct vmm_region, phead);

		/* Remove region from tree */
		rb_erase(&reg->head, root);

		/* Delete the region */
		vmm_write_unlock_irqrestore_lite(root_lock, flags);
		region_del(guest, reg, FALSE, FALSE);
		vmm_write_lock_irqsave_lite(root_lock, flags);
	}
	*root = RB_ROOT;
	vmm_write_unlock_irqrestore_lite(root_lock, flags);

	/* One-by-one remove all mem regions in reverse probing order */
	root = &aspace->reg_memtree;
	root_plist = &aspace->reg_memprobe_list;
	root_lock = &aspace->reg_memtree_lock;
	vmm_write_lock_irqsave_lite(root_lock, flags);
	while (!list_empty(root_plist)) {
		/* Get last region from probe list */
		reg = list_entry(list_pop_tail(root_plist),
				 struct vmm_region, phead);

		/* Remove region from tree */
		rb_erase(&reg->head, root);

		/* Delete the region */
		vmm_write_unlock_irqrestore_lite(root_lock, flags);
		region_del(guest, reg, FALSE, FALSE);
		vmm_write_lock_irqsave_lite(root_lock, flags);
	}
	*root = RB_ROOT;
	vmm_write_unlock_irqrestore_lite(root_lock, flags);

	/* DeInitialize device emulation context */
	if ((rc = vmm_devemu_deinit_context(guest))) {
		return rc;
	}
	guest->aspace.devemu_priv = NULL;

	/* De-reference address space node */
	if (guest->aspace.node) {
		vmm_devtree_dref_node(guest->aspace.node);
		guest->aspace.node = NULL;
	}

	return VMM_OK;
}

