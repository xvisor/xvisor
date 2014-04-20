/**
 * Copyright (c) 2014 Anup Patel.
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
 * @file vmm_devres.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Device driver resource managment
 */

#include <vmm_error.h>
#include <vmm_compiler.h>
#include <vmm_macros.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_devdrv.h>
#include <vmm_devres.h>
#include <libs/stringlib.h>
#include <libs/list.h>

struct vmm_devres_node {
	struct dlist			entry;
	vmm_dr_release_t		release;
};

struct vmm_devres {
	struct vmm_devres_node		node;
	/* -- 3 pointers */
	unsigned long long		data[];	/* guarantee ull alignment */
};

static struct vmm_devres *alloc_dr(vmm_dr_release_t release,
				   size_t size)
{
	size_t tot_size = sizeof(struct vmm_devres) + size;
	struct vmm_devres *dr;

	dr = vmm_malloc(tot_size);
	if (unlikely(!dr))
		return NULL;

	memset(dr, 0, offsetof(struct vmm_devres, data));

	INIT_LIST_HEAD(&dr->node.entry);
	dr->node.release = release;

	return dr;
}

static void add_dr(struct vmm_device *dev, struct vmm_devres_node *node)
{
	BUG_ON(!list_empty(&node->entry));
	list_add_tail(&node->entry, &dev->devres_head);
}

void *vmm_devres_alloc(vmm_dr_release_t release, size_t size)
{
	struct vmm_devres *dr;

	dr = alloc_dr(release, size);
	if (unlikely(!dr))
		return NULL;

	return dr->data;
}

void vmm_devres_for_each_res(struct vmm_device *dev, vmm_dr_release_t release,
			     vmm_dr_match_t match, void *match_data,
			     void (*fn)(struct vmm_device *, void *, void *),
			     void *data)
{
	struct vmm_devres_node *node;
	struct vmm_devres_node *tmp;
	irq_flags_t flags;

	if (!fn)
		return;

	vmm_spin_lock_irqsave(&dev->devres_lock, flags);
	list_for_each_entry_safe_reverse(node, tmp,
			&dev->devres_head, entry) {
		struct vmm_devres *dr =
				container_of(node, struct vmm_devres, node);

		if (node->release != release)
			continue;
		if (match && !match(dev, dr->data, match_data))
			continue;
		fn(dev, dr->data, data);
	}
	vmm_spin_unlock_irqrestore(&dev->devres_lock, flags);
}

void vmm_devres_free(void *res)
{
	if (res) {
		struct vmm_devres *dr =
				container_of(res, struct vmm_devres, data);

		BUG_ON(!list_empty(&dr->node.entry));
		vmm_free(dr);
	}
}

void vmm_devres_add(struct vmm_device *dev, void *res)
{
	struct vmm_devres *dr =
			container_of(res, struct vmm_devres, data);
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&dev->devres_lock, flags);
	add_dr(dev, &dr->node);
	vmm_spin_unlock_irqrestore(&dev->devres_lock, flags);
}

static struct vmm_devres *find_dr(struct vmm_device *dev,
				vmm_dr_release_t release,
				vmm_dr_match_t match, void *match_data)
{
	struct vmm_devres_node *node;

	list_for_each_entry_reverse(node, &dev->devres_head, entry) {
		struct vmm_devres *dr =
				container_of(node, struct vmm_devres, node);

		if (node->release != release)
			continue;
		if (match && !match(dev, dr->data, match_data))
			continue;
		return dr;
	}

	return NULL;
}

void *vmm_devres_find(struct vmm_device *dev, vmm_dr_release_t release,
		      vmm_dr_match_t match, void *match_data)
{
	struct vmm_devres *dr;
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&dev->devres_lock, flags);
	dr = find_dr(dev, release, match, match_data);
	vmm_spin_unlock_irqrestore(&dev->devres_lock, flags);

	if (dr)
		return dr->data;

	return NULL;
}

void *vmm_devres_get(struct vmm_device *dev, void *new_res,
		     vmm_dr_match_t match, void *match_data)
{
	struct vmm_devres *new_dr =
			container_of(new_res, struct vmm_devres, data);
	struct vmm_devres *dr;
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&dev->devres_lock, flags);
	dr = find_dr(dev, new_dr->node.release, match, match_data);
	if (!dr) {
		add_dr(dev, &new_dr->node);
		dr = new_dr;
		new_dr = NULL;
	}
	vmm_spin_unlock_irqrestore(&dev->devres_lock, flags);
	vmm_devres_free(new_dr);

	return dr->data;
}

void *vmm_devres_remove(struct vmm_device *dev, vmm_dr_release_t release,
			vmm_dr_match_t match, void *match_data)
{
	struct vmm_devres *dr;
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&dev->devres_lock, flags);
	dr = find_dr(dev, release, match, match_data);
	if (dr) {
		list_del_init(&dr->node.entry);
	}
	vmm_spin_unlock_irqrestore(&dev->devres_lock, flags);

	if (dr)
		return dr->data;
	return NULL;
}

int vmm_devres_destroy(struct vmm_device *dev, vmm_dr_release_t release,
		       vmm_dr_match_t match, void *match_data)
{
	void *res;

	res = vmm_devres_remove(dev, release, match, match_data);
	if (unlikely(!res))
		return VMM_ENOENT;

	vmm_devres_free(res);

	return VMM_OK;
}

int vmm_devres_release(struct vmm_device *dev, vmm_dr_release_t release,
		       vmm_dr_match_t match, void *match_data)
{
	void *res;

	res = vmm_devres_remove(dev, release, match, match_data);
	if (unlikely(!res))
		return VMM_ENOENT;

	(*release)(dev, res);
	vmm_devres_free(res);

	return VMM_OK;
}

static int release_nodes(struct vmm_device *dev,
			 struct dlist *first,
			 struct dlist *end)
{
	LIST_HEAD(todo);
	irq_flags_t flags;
	struct vmm_devres *dr, *tmp;

	vmm_spin_lock_irqsave(&dev->devres_lock, flags);

	list_for_each_entry_safe_reverse(dr, tmp, &todo, node.entry) {
		dr->node.release(dev, dr->data);
		vmm_free(dr);
	}

	vmm_spin_unlock_irqrestore(&dev->devres_lock, flags);

	return VMM_OK;
}

int vmm_devres_release_all(struct vmm_device *dev)
{
	/* Looks like an uninitialized device structure */
	if (WARN_ON(dev->devres_head.next == NULL))
		return VMM_ENODEV;
	return release_nodes(dev, dev->devres_head.next, &dev->devres_head);
}

