/**
 * Copyright (c) 2014 Himanshu Chauhan.
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
 * @file vmm_resource.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Resource management for arbitrary resources (including
 * host IO space and host memory space.
 *
 * This source has been largely adapted from Linux sources:
 * commit 97bf6af1f928216fd6c5a66e8a57bfa95a659672
 * Linux 3.19-rc1
 *
 *	linux/kernel/resource.c
 *
 * Copyright (C) 1999	Linus Torvalds
 * Copyright (C) 1999	Martin Mares <mj@ucw.cz>
 *
 * Arbitrary resource management.
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_params.h>
#include <vmm_spinlocks.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <vmm_devdrv.h>
#include <vmm_devres.h>
#include <vmm_completion.h>
#include <vmm_resource.h>
#include <arch_config.h>

#undef _DEBUG
#ifdef _DEBUG
#define DPRINTF(msg...)	vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

struct vmm_resource vmm_hostio_resource = {
	.name	= "Host IO",
	.start	= 0,
	.end	= ARCH_IO_SPACE_LIMIT,
	.flags	= VMM_IORESOURCE_IO,
};

struct vmm_resource vmm_hostmem_resource = {
	.name	= "Host Memory",
	.start	= 0,
	.end	= -1,
	.flags	= VMM_IORESOURCE_MEM,
};

/* constraints to be met while allocating resources */
struct resource_constraint {
	resource_size_t min, max, align;
	resource_size_t (*alignf)(void *, const struct vmm_resource *,
			resource_size_t, resource_size_t);
	void *alignf_data;
};

static DEFINE_RWLOCK(resource_lock);

/*
 * For memory hotplug, there is no way to free resource entries allocated
 * by boot mem after the system is up. So for reusing the resource entry
 * we need to remember the resource.
 */

static struct vmm_resource *next_resource(struct vmm_resource *p,
					  bool sibling_only)
{
	/* Caller wants to traverse through siblings only */
	if (sibling_only)
		return p->sibling;

	if (p->child)
		return p->child;
	while (!p->sibling && p->parent)
		p = p->parent;
	return p->sibling;
}

static void *r_next(void *v, loff_t *pos)
{
	struct vmm_resource *p = v;
	(*pos)++;
	return (void *)next_resource(p, false);
}

#define free_resource(res)	vmm_free(res)
#define alloc_resource()	vmm_zalloc(sizeof(struct vmm_resource))

/* Return the conflict entry if you can't request it */
static struct vmm_resource * __request_resource(struct vmm_resource *root,
						struct vmm_resource *new)
{
	resource_size_t start = new->start;
	resource_size_t end = new->end;
	struct vmm_resource *tmp, **p;

	if (end < start)
		return root;
	if (start < root->start)
		return root;
	if (end > root->end)
		return root;
	p = &root->child;
	for (;;) {
		tmp = *p;
		if (!tmp || tmp->start > end) {
			new->sibling = tmp;
			*p = new;
			new->parent = root;
			return NULL;
		}
		p = &tmp->sibling;
		if (tmp->end < start)
			continue;
		return tmp;
	}
}

static int __release_resource(struct vmm_resource *old)
{
	struct vmm_resource *tmp, **p;

	p = &old->parent->child;
	for (;;) {
		tmp = *p;
		if (!tmp)
			break;
		if (tmp == old) {
			*p = tmp->sibling;
			old->parent = NULL;
			return 0;
		}
		p = &tmp->sibling;
	}
	return VMM_EINVALID;
}

struct vmm_resource *vmm_request_resource_conflict(struct vmm_resource *root,
						   struct vmm_resource *new)
{
	struct vmm_resource *conflict;

	vmm_write_lock(&resource_lock);
	conflict = __request_resource(root, new);
	vmm_write_unlock(&resource_lock);

	return conflict;
}

int vmm_request_resource(struct vmm_resource *root, struct vmm_resource *new)
{
	struct vmm_resource *conflict;

	conflict = vmm_request_resource_conflict(root, new);
	return conflict ? VMM_EBUSY : 0;
}

int vmm_release_resource(struct vmm_resource *old)
{
	int retval;

	vmm_write_lock(&resource_lock);
	retval = __release_resource(old);
	vmm_write_unlock(&resource_lock);
	return retval;
}

static void __release_child_resources(struct vmm_resource *r)
{
	struct vmm_resource *tmp, *p;
	resource_size_t size;

	p = r->child;
	r->child = NULL;
	while (p) {
		tmp = p;
		p = p->sibling;

		tmp->parent = NULL;
		tmp->sibling = NULL;
		__release_child_resources(tmp);

		DPRINTF("release child resource %pR\n", tmp);
		/* need to restore size, and keep flags */
		size = vmm_resource_size(tmp);
		tmp->start = 0;
		tmp->end = size - 1;
	}
}

void vmm_release_child_resources(struct vmm_resource *r)
{
	vmm_write_lock(&resource_lock);
	__release_child_resources(r);
	vmm_write_unlock(&resource_lock);
}

/*
 * Finds the lowest hostmem reosurce exists with-in [res->start.res->end)
 * the caller must specify res->start, res->end, res->flags and "name".
 * If found, returns 0, res is overwritten, if not found, returns -1.
 * This walks through whole tree and not just first level children
 * until and unless first_level_children_only is true.
 */
static int find_next_hostmem_res(struct vmm_resource *res, char *name,
			         bool first_level_children_only)
{
	resource_size_t start, end;
	struct vmm_resource *p;
	bool sibling_only = false;

	BUG_ON(!res);

	start = res->start;
	end = res->end;
	BUG_ON(start >= end);

	if (first_level_children_only)
		sibling_only = true;

	vmm_read_lock(&resource_lock);

	for (p = vmm_hostmem_resource.child; p;
	     p = next_resource(p, sibling_only)) {
		if (p->flags != res->flags)
			continue;
		if (name && strcmp(p->name, name))
			continue;
		if (p->start > end) {
			p = NULL;
			break;
		}
		if ((p->end >= start) && (p->start < end))
			break;
	}

	vmm_read_unlock(&resource_lock);
	if (!p)
		return -1;
	/* copy data */
	if (res->start < p->start)
		res->start = p->start;
	if (res->end > p->end)
		res->end = p->end;
	return 0;
}

int vmm_walk_hostmem_res(char *name, unsigned long flags,
			 u64 start, u64 end, void *arg,
			 int (*func)(u64, u64, void *))
{
	struct vmm_resource res;
	u64 orig_end;
	int ret = -1;

	res.start = start;
	res.end = end;
	res.flags = flags;
	orig_end = res.end;
	while ((res.start < res.end) &&
		(!find_next_hostmem_res(&res, name, false))) {
		ret = (*func)(res.start, res.end, arg);
		if (ret)
			break;
		res.start = res.end + 1;
		res.end = orig_end;
	}
	return ret;
}

int vmm_walk_system_ram_res(u64 start, u64 end, void *arg,
			    int (*func)(u64, u64, void *))
{
	struct vmm_resource res;
	u64 orig_end;
	int ret = -1;

	res.start = start;
	res.end = end;
	res.flags = VMM_IORESOURCE_MEM | VMM_IORESOURCE_BUSY;
	orig_end = res.end;
	while ((res.start < res.end) &&
		(!find_next_hostmem_res(&res, "System RAM", true))) {
		ret = (*func)(res.start, res.end, arg);
		if (ret)
			break;
		res.start = res.end + 1;
		res.end = orig_end;
	}
	return ret;
}

#if !defined(CONFIG_ARCH_HAS_WALK_MEMORY)

int vmm_walk_system_ram_range(unsigned long start_pfn,
			unsigned long nr_pages, void *arg,
			int (*func)(unsigned long, unsigned long, void *))
{
	struct vmm_resource res;
	unsigned long pfn, end_pfn;
	u64 orig_end;
	int ret = -1;

	res.start = (u64) start_pfn << VMM_PAGE_SHIFT;
	res.end = ((u64)(start_pfn + nr_pages) << VMM_PAGE_SHIFT) - 1;
	res.flags = VMM_IORESOURCE_MEM | VMM_IORESOURCE_BUSY;
	orig_end = res.end;
	while ((res.start < res.end) &&
		(find_next_hostmem_res(&res, "System RAM", TRUE) >= 0)) {
		pfn = (res.start + VMM_PAGE_SIZE - 1) >> VMM_PAGE_SHIFT;
		end_pfn = (res.end + 1) >> VMM_PAGE_SHIFT;
		if (end_pfn > pfn)
			ret = (*func)(pfn, end_pfn - pfn, arg);
		if (ret)
			break;
		res.start = res.end + 1;
		res.end = orig_end;
	}
	return ret;
}

#endif

void __weak arch_remove_reservations(struct vmm_resource *avail)
{
}

static resource_size_t simple_align_resource(void *data,
					     const struct vmm_resource *avail,
					     resource_size_t size,
					     resource_size_t align)
{
	return avail->start;
}

static void resource_clip(struct vmm_resource *res, resource_size_t min,
			  resource_size_t max)
{
	if (res->start < min)
		res->start = min;
	if (res->end > max)
		res->end = max;
}

/*
 * Find empty slot in the resource tree with the given range and
 * alignment constraints
 */
static int __find_resource(struct vmm_resource *root,
			   struct vmm_resource *old,
			   struct vmm_resource *new,
			   resource_size_t  size,
			   struct resource_constraint *constraint)
{
	struct vmm_resource *this = root->child;
	struct vmm_resource tmp = *new, avail, alloc;

	tmp.start = root->start;
	/*
	 * Skip past an allocated resource that starts at 0, since the assignment
	 * of this->start - 1 to tmp->end below would cause an underflow.
	 */
	if (this && this->start == root->start) {
		tmp.start = (this == old) ? old->start : this->end + 1;
		this = this->sibling;
	}
	for(;;) {
		if (this)
			tmp.end = (this == old) ? this->end : this->start - 1;
		else
			tmp.end = root->end;

		if (tmp.end < tmp.start)
			goto next;

		resource_clip(&tmp, constraint->min, constraint->max);
		arch_remove_reservations(&tmp);

		/* Check for overflow after align() */
		avail.start = align(tmp.start, constraint->align);
		avail.end = tmp.end;
		avail.flags = new->flags & ~VMM_IORESOURCE_UNSET;
		if (avail.start >= tmp.start) {
			alloc.flags = avail.flags;
			alloc.start =
				constraint->alignf(constraint->alignf_data,
					&avail, size, constraint->align);
			alloc.end = alloc.start + size - 1;
			if (vmm_resource_contains(&avail, &alloc)) {
				new->start = alloc.start;
				new->end = alloc.end;
				return 0;
			}
		}

next:		if (!this || this->end == root->end)
			break;

		if (this != old)
			tmp.start = this->end + 1;
		this = this->sibling;
	}
	return VMM_EBUSY;
}

/*
 * Find empty slot in the resource tree given range and alignment.
 */
static int find_resource(struct vmm_resource *root,
			 struct vmm_resource *new,
			 resource_size_t size,
			 struct resource_constraint  *constraint)
{
	return  __find_resource(root, NULL, new, size, constraint);
}

/**
 * Allocate a slot in the resource tree given range & alignment.
 * The resource will be relocated if the new size cannot be reallocated in the
 * current location.
 *
 * @root: root resource descriptor
 * @old:  resource descriptor desired by caller
 * @newsize: new size of the resource descriptor
 * @constraint: the size and alignment constraints to be met.
 */
static int reallocate_resource(struct vmm_resource *root,
				struct vmm_resource *old,
				resource_size_t newsize,
				struct resource_constraint  *constraint)
{
	int err=0;
	struct vmm_resource new = *old;
	struct vmm_resource *conflict;

	vmm_write_lock(&resource_lock);

	if ((err = __find_resource(root, old, &new, newsize, constraint)))
		goto out;

	if (vmm_resource_contains(&new, old)) {
		old->start = new.start;
		old->end = new.end;
		goto out;
	}

	if (old->child) {
		err = VMM_EBUSY;
		goto out;
	}

	if (vmm_resource_contains(old, &new)) {
		old->start = new.start;
		old->end = new.end;
	} else {
		__release_resource(old);
		*old = new;
		conflict = __request_resource(root, old);
		BUG_ON(conflict);
	}
out:
	vmm_write_unlock(&resource_lock);
	return err;
}

int vmm_allocate_resource(struct vmm_resource *root, struct vmm_resource *new,
			  resource_size_t size, resource_size_t min,
			  resource_size_t max, resource_size_t align,
			  resource_size_t (*alignf)(void *,
						const struct vmm_resource *,
						resource_size_t,
						resource_size_t),
						void *alignf_data)
{
	int err;
	struct resource_constraint constraint;

	if (!alignf)
		alignf = simple_align_resource;

	constraint.min = min;
	constraint.max = max;
	constraint.align = align;
	constraint.alignf = alignf;
	constraint.alignf_data = alignf_data;

	if ( new->parent ) {
		/* resource is already allocated, try reallocating with
		   the new constraints */
		return reallocate_resource(root, new, size, &constraint);
	}

	vmm_write_lock(&resource_lock);
	err = find_resource(root, new, size, &constraint);
	if (err >= 0 && __request_resource(root, new))
		err = VMM_EBUSY;
	vmm_write_unlock(&resource_lock);
	return err;
}

struct vmm_resource *vmm_lookup_resource(struct vmm_resource *root,
					 resource_size_t start)
{
	struct vmm_resource *res;

	vmm_read_lock(&resource_lock);
	for (res = root->child; res; res = res->sibling) {
		if (res->start == start)
			break;
	}
	vmm_read_unlock(&resource_lock);

	return res;
}

/*
 * Insert a resource into the resource tree. If successful, return NULL,
 * otherwise return the conflicting resource (compare to __request_resource())
 */
static struct vmm_resource * __insert_resource(struct vmm_resource *parent,
						struct vmm_resource *new)
{
	struct vmm_resource *first, *next;

	for (;; parent = first) {
		first = __request_resource(parent, new);
		if (!first)
			return first;

		if (first == parent)
			return first;
		if (WARN_ON(first == new))	/* duplicated insertion */
			return first;

		if ((first->start > new->start) || (first->end < new->end))
			break;
		if ((first->start == new->start) && (first->end == new->end))
			break;
	}

	for (next = first; ; next = next->sibling) {
		/* Partial overlap? Bad, and unfixable */
		if (next->start < new->start || next->end > new->end)
			return next;
		if (!next->sibling)
			break;
		if (next->sibling->start > new->end)
			break;
	}

	new->parent = parent;
	new->sibling = next->sibling;
	new->child = first;

	next->sibling = NULL;
	for (next = first; next; next = next->sibling)
		next->parent = new;

	if (parent->child == first) {
		parent->child = new;
	} else {
		next = parent->child;
		while (next->sibling != first)
			next = next->sibling;
		next->sibling = new;
	}
	return NULL;
}

struct vmm_resource *vmm_insert_resource_conflict(struct vmm_resource *parent,
						  struct vmm_resource *new)
{
	struct vmm_resource *conflict;

	vmm_write_lock(&resource_lock);
	conflict = __insert_resource(parent, new);
	vmm_write_unlock(&resource_lock);
	return conflict;
}

int vmm_insert_resource(struct vmm_resource *parent,
			struct vmm_resource *new)
{
	struct vmm_resource *conflict;

	conflict = vmm_insert_resource_conflict(parent, new);
	return conflict ? VMM_EBUSY : 0;
}

void vmm_insert_resource_expand_to_fit(struct vmm_resource *root,
					struct vmm_resource *new)
{
	if (new->parent)
		return;

	vmm_write_lock(&resource_lock);
	for (;;) {
		struct vmm_resource *conflict;

		conflict = __insert_resource(root, new);
		if (!conflict)
			break;
		if (conflict == root)
			break;

		/* Ok, expand resource to cover the conflict, then try again .. */
		if (conflict->start < new->start)
			new->start = conflict->start;
		if (conflict->end > new->end)
			new->end = conflict->end;

		vmm_printf("Expanded resource %s due to conflict with %s\n",
			   new->name, conflict->name);
	}
	vmm_write_unlock(&resource_lock);
}

static int __adjust_resource(struct vmm_resource *res,
			     resource_size_t start,
			     resource_size_t size)
{
	struct vmm_resource *tmp, *parent = res->parent;
	resource_size_t end = start + size - 1;
	int result = VMM_EBUSY;

	if (!parent)
		goto skip;

	if ((start < parent->start) || (end > parent->end))
		goto out;

	if (res->sibling && (res->sibling->start <= end))
		goto out;

	tmp = parent->child;
	if (tmp != res) {
		while (tmp->sibling != res)
			tmp = tmp->sibling;
		if (start <= tmp->end)
			goto out;
	}

skip:
	for (tmp = res->child; tmp; tmp = tmp->sibling)
		if ((tmp->start < start) || (tmp->end > end))
			goto out;

	res->start = start;
	res->end = end;
	result = 0;

 out:
	return result;
}

int vmm_adjust_resource(struct vmm_resource *res,
			resource_size_t start,
			resource_size_t size)
{
	int result;

	vmm_write_lock(&resource_lock);
	result = __adjust_resource(res, start, size);
	vmm_write_unlock(&resource_lock);
	return result;
}

resource_size_t vmm_resource_alignment(struct vmm_resource *res)
{
	switch (res->flags &
		(VMM_IORESOURCE_SIZEALIGN | VMM_IORESOURCE_STARTALIGN)) {
	case VMM_IORESOURCE_SIZEALIGN:
		return vmm_resource_size(res);
	case VMM_IORESOURCE_STARTALIGN:
		return res->start;
	default:
		return 0;
	}
}

DECLARE_COMPLETION(__req_reg_completion);

static void __init __reserve_region_with_split(struct vmm_resource *root,
						resource_size_t start,
						resource_size_t end,
						const char *name)
{
	struct vmm_resource *parent = root;
	struct vmm_resource *conflict;
	struct vmm_resource *res = alloc_resource();
	struct vmm_resource *next_res = NULL;

	if (!res)
		return;

	res->name = name;
	res->start = start;
	res->end = end;
	res->flags = VMM_IORESOURCE_BUSY;

	while (1) {

		conflict = __request_resource(parent, res);
		if (!conflict) {
			if (!next_res)
				break;
			res = next_res;
			next_res = NULL;
			continue;
		}

		/* conflict covered whole area */
		if (conflict->start <= res->start &&
				conflict->end >= res->end) {
			free_resource(res);
			WARN_ON(next_res);
			break;
		}

		/* failed, split and try again */
		if (conflict->start > res->start) {
			end = res->end;
			res->end = conflict->start - 1;
			if (conflict->end < end) {
				next_res = alloc_resource();
				if (!next_res) {
					free_resource(res);
					break;
				}
				next_res->name = name;
				next_res->start = conflict->end + 1;
				next_res->end = end;
				next_res->flags = VMM_IORESOURCE_BUSY;
			}
		} else {
			res->start = conflict->end + 1;
		}
	}

}

void __init vmm_reserve_region_with_split(struct vmm_resource *root,
					  resource_size_t start,
					  resource_size_t end,
					  const char *name)
{
	int abort = 0;

	vmm_write_lock(&resource_lock);
	if (root->start > start || root->end < end) {
		vmm_printf("requested range [0x%llx-0x%llx] not in root %pr\n",
			   (unsigned long long)start, (unsigned long long)end,
			   root);
		if (start > root->end || end < root->start)
			abort = 1;
		else {
			if (end > root->end)
				end = root->end;
			if (start < root->start)
				start = root->start;
			vmm_printf("fixing request to [0x%llx-0x%llx]\n",
				   (unsigned long long)start,
				   (unsigned long long)end);
		}
	}
	if (!abort)
		__reserve_region_with_split(root, start, end, name);
	vmm_write_unlock(&resource_lock);
}

struct vmm_resource * __vmm_request_region(struct vmm_resource *parent,
					   resource_size_t start,
					   resource_size_t n,
					   const char *name, int flags)
{
	struct vmm_resource *res = alloc_resource();

	if (!res)
		return NULL;

	res->name = name;
	res->start = start;
	res->end = start + n - 1;
	res->flags = vmm_resource_type(parent);
	res->flags |= VMM_IORESOURCE_BUSY | flags;

	vmm_write_lock(&resource_lock);

	for (;;) {
		struct vmm_resource *conflict;

		conflict = __request_resource(parent, res);
		if (!conflict)
			break;
		if (conflict != parent) {
			parent = conflict;
			if (!(conflict->flags & VMM_IORESOURCE_BUSY))
				continue;
		}
		if (conflict->flags & flags & VMM_IORESOURCE_MUXED) {
			vmm_write_unlock(&resource_lock);
			vmm_completion_wait(&__req_reg_completion);
			vmm_write_lock(&resource_lock);
			continue;
		}
		/* Uhhuh, that didn't work out.. */
		free_resource(res);
		res = NULL;
		break;
	}
	vmm_write_unlock(&resource_lock);
	return res;
}

int __vmm_check_region(struct vmm_resource *parent,
			resource_size_t start,
			resource_size_t n)
{
	struct vmm_resource * res;

	res = __vmm_request_region(parent, start, n, "check-region", 0);
	if (!res)
		return VMM_EBUSY;

	vmm_release_resource(res);
	free_resource(res);
	return 0;
}

void __vmm_release_region(struct vmm_resource *parent,
			  resource_size_t start,
			  resource_size_t n)
{
	struct vmm_resource **p;
	resource_size_t end;

	p = &parent->child;
	end = start + n - 1;

	vmm_write_lock(&resource_lock);

	for (;;) {
		struct vmm_resource *res = *p;

		if (!res)
			break;
		if (res->start <= start && res->end >= end) {
			if (!(res->flags & VMM_IORESOURCE_BUSY)) {
				p = &res->child;
				continue;
			}
			if (res->start != start || res->end != end)
				break;
			*p = res->sibling;
			vmm_write_unlock(&resource_lock);
			if (res->flags & VMM_IORESOURCE_MUXED)
				vmm_completion_complete(&__req_reg_completion);

			free_resource(res);
			return;
		}
		p = &res->sibling;
	}

	vmm_write_unlock(&resource_lock);

	vmm_printf("Trying to free nonexistent resource "
		   "<%016llx-%016llx>\n",
		   (unsigned long long)start,
		   (unsigned long long)end);
}

#ifdef CONFIG_MEMORY_HOTREMOVE
int vmm_release_mem_region_adjustable(struct vmm_resource *parent,
				      resource_size_t start,
				      resource_size_t size)
{
	struct vmm_resource **p;
	struct vmm_resource *res;
	struct vmm_resource *new_res;
	resource_size_t end;
	int ret = VMM_EINVALID;

	end = start + size - 1;
	if ((start < parent->start) || (end > parent->end))
		return ret;

	/* The alloc_resource() result gets checked later */
	new_res = alloc_resource();

	p = &parent->child;
	vmm_write_lock(&resource_lock);

	while ((res = *p)) {
		if (res->start >= end)
			break;

		/* look for the next resource if it does not fit into */
		if (res->start > start || res->end < end) {
			p = &res->sibling;
			continue;
		}

		if (!(res->flags & VMM_IORESOURCE_MEM))
			break;

		if (!(res->flags & VMM_IORESOURCE_BUSY)) {
			p = &res->child;
			continue;
		}

		/* found the target resource; let's adjust accordingly */
		if (res->start == start && res->end == end) {
			/* free the whole entry */
			*p = res->sibling;
			free_resource(res);
			ret = 0;
		} else if (res->start == start && res->end != end) {
			/* adjust the start */
			ret = __adjust_resource(res, end + 1,
						res->end - end);
		} else if (res->start != start && res->end == end) {
			/* adjust the end */
			ret = __adjust_resource(res, res->start,
						start - res->start);
		} else {
			/* split into two entries */
			if (!new_res) {
				ret = VMM_ENOMEM;
				break;
			}
			new_res->name = res->name;
			new_res->start = end + 1;
			new_res->end = res->end;
			new_res->flags = res->flags;
			new_res->parent = res->parent;
			new_res->sibling = res->sibling;
			new_res->child = NULL;

			ret = __adjust_resource(res, res->start,
						start - res->start);
			if (ret)
				break;
			res->sibling = new_res;
			new_res = NULL;
		}

		break;
	}

	vmm_write_unlock(&resource_lock);
	free_resource(new_res);
	return ret;
}
#endif	/* CONFIG_MEMORY_HOTREMOVE */

static void devm_resource_release(struct vmm_device *dev, void *ptr)
{
	struct vmm_resource **r = ptr;

	vmm_release_resource(*r);
}

int vmm_devm_request_resource(struct vmm_device *dev,
			      struct vmm_resource *root,
			      struct vmm_resource *new)
{
	struct vmm_resource *conflict, **ptr;

	ptr = vmm_devres_alloc(devm_resource_release, sizeof(*ptr));
	if (!ptr)
		return VMM_ENOMEM;

	*ptr = new;

	conflict = vmm_request_resource_conflict(root, new);
	if (conflict) {
		vmm_printf("%s: resource collision: %pR conflicts with %s %pR\n",
			   dev->name, new, conflict->name, conflict);
		vmm_devres_free(ptr);
		return VMM_EBUSY;
	}

	vmm_devres_add(dev, ptr);
	return 0;
}

static int devm_resource_match(struct vmm_device *dev,
				   void *res, void *data)
{
	struct vmm_resource **ptr = res;

	return *ptr == data;
}

void vmm_devm_release_resource(struct vmm_device *dev,
				struct vmm_resource *new)
{
	WARN_ON(vmm_devres_release(dev,
		devm_resource_release, devm_resource_match, new));
}

struct region_devres {
	struct vmm_resource *parent;
	resource_size_t start;
	resource_size_t n;
};

static void devm_region_release(struct vmm_device *dev, void *res)
{
	struct region_devres *this = res;

	__vmm_release_region(this->parent, this->start, this->n);
}

static int devm_region_match(struct vmm_device *dev,
			     void *res, void *match_data)
{
	struct region_devres *this = res, *match = match_data;

	return this->parent == match->parent &&
		this->start == match->start && this->n == match->n;
}

struct vmm_resource * __vmm_devm_request_region(struct vmm_device *dev,
						struct vmm_resource *parent,
						resource_size_t start,
						resource_size_t n,
						const char *name)
{
	struct region_devres *dr = NULL;
	struct vmm_resource *res;

	dr = vmm_devres_alloc(devm_region_release,
			      sizeof(struct region_devres));
	if (!dr)
		return NULL;

	dr->parent = parent;
	dr->start = start;
	dr->n = n;

	res = __vmm_request_region(parent, start, n, name, 0);
	if (res)
		vmm_devres_add(dev, dr);
	else
		vmm_devres_free(dr);

	return res;
}

void __vmm_devm_release_region(struct vmm_device *dev,
				struct vmm_resource *parent,
				resource_size_t start, resource_size_t n)
{
	struct region_devres match_data = { parent, start, n };

	__vmm_release_region(parent, start, n);
	WARN_ON(vmm_devres_destroy(dev,
		devm_region_release, devm_region_match, &match_data));
}

/*
 * Called from init/main.c to reserve IO ports.
 */
#define MAXRESERVE 4
static int __init reserve_setup(char *str)
{
	static int reserved;
	static struct vmm_resource reserve[MAXRESERVE];

	for (;;) {
		unsigned int io_start, io_num;
		int x = reserved;

		if (vmm_get_option (&str, (int *)&io_start) != 2)
			break;
		if (vmm_get_option (&str, (int *)&io_num)   == 0)
			break;
		if (x < MAXRESERVE) {
			struct vmm_resource *res = reserve + x;
			res->name = "reserved";
			res->start = io_start;
			res->end = io_start + io_num - 1;
			res->flags = VMM_IORESOURCE_BUSY;
			res->child = NULL;
			if (vmm_request_resource(res->start >= 0x10000 ?
				&vmm_hostmem_resource : &vmm_hostio_resource, res) == 0)
				reserved = x+1;
		}
	}
	return 1;
}
vmm_early_param("reserve=", reserve_setup);

int vmm_hostmem_map_sanity_check(resource_size_t addr, unsigned long size)
{
	struct vmm_resource *p = &vmm_hostmem_resource;
	int err = 0;
	loff_t l;

	vmm_read_lock(&resource_lock);
	for (p = p->child; p ; p = r_next(p, &l)) {
		/*
		 * We can probably skip the resources without
		 * VMM_IORESOURCE_IO attribute?
		 */
		if (p->start >= addr + size)
			continue;
		if (p->end < addr)
			continue;
		if (VMM_PFN_DOWN(p->start) <= VMM_PFN_DOWN(addr) &&
		    VMM_PFN_DOWN(p->end) >= VMM_PFN_DOWN(addr + size - 1))
			continue;
		/*
		 * if a resource is "BUSY", it's not a hardware resource
		 * but a driver mapping of such a resource; we don't want
		 * to warn for those; some drivers legitimately map only
		 * partial hardware resources. (example: vesafb)
		 */
		if (p->flags & VMM_IORESOURCE_BUSY)
			continue;

		vmm_printf("resource sanity check: requesting "
			   "[mem %#010llx-%#010llx], which spans "
			   "more than %s %pR\n",
			   (unsigned long long)addr,
			   (unsigned long long)(addr + size - 1),
			   p->name, p);
		err = -1;
		break;
	}
	vmm_read_unlock(&resource_lock);

	return err;
}

#ifdef CONFIG_STRICT_DEVMEM
static int strict_hostmem_checks = 1;
#else
static int strict_hostmem_checks;
#endif

int vmm_hostmem_is_exclusive(u64 addr)
{
	struct vmm_resource *p = &vmm_hostmem_resource;
	int err = 0;
	loff_t l;
	int size = VMM_PAGE_SIZE;

	if (!strict_hostmem_checks)
		return 0;

	addr = addr & VMM_PAGE_MASK;

	vmm_read_lock(&resource_lock);
	for (p = p->child; p ; p = r_next(p, &l)) {
		/*
		 * We can probably skip the resources without
		 * VMM_IORESOURCE_IO attribute?
		 */
		if (p->start >= addr + size)
			break;
		if (p->end < addr)
			continue;
		if (p->flags & VMM_IORESOURCE_BUSY &&
		     p->flags & VMM_IORESOURCE_EXCLUSIVE) {
			err = 1;
			break;
		}
	}
	vmm_read_unlock(&resource_lock);

	return err;
}

static int __init strict_hostmem(char *str)
{
	if (strstr(str, "relaxed"))
		strict_hostmem_checks = 0;
	if (strstr(str, "strict"))
		strict_hostmem_checks = 1;
	return 1;
}
vmm_early_param("hostmem=", strict_hostmem);
