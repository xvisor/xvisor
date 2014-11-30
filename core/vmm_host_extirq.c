/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 *
 * @file vmm_hostirq.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Extended IRQ support, kind of Xvior compatible Linux IRQ domain.
 */

#include <vmm_host_extirq.h>
#include <vmm_mutex.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <libs/list.h>

#define HOST_EXTIRQ_CHUNK		32
#define BITMAP_SIZE(X)							\
	(((X) + (BITS_PER_BYTE) - 1)  / (BITS_PER_BYTE))

/**
 * @bitmap: The extended IRQ map
 */
struct vmm_host_extirqs_ctrl {
	struct vmm_mutex lock;
	struct dlist groups;
	unsigned int count;
	unsigned long *bitmap;
	struct vmm_host_irq **irqs;
};

static struct vmm_host_extirqs_ctrl extirqctrl;

struct vmm_host_irq *vmm_host_extirq_get(unsigned int irq)
{
	if (irq < (CONFIG_HOST_IRQ_COUNT))
		return NULL;
	irq -= CONFIG_HOST_IRQ_COUNT;
	if (irq > extirqctrl.count)
		return NULL;

	return extirqctrl.irqs[irq];
}

int vmm_host_extirq_to_hwirq(struct vmm_host_extirq_group *group,
			     unsigned int irq)
{
	if (irq < group->base || irq > group->end)
		return VMM_ENOTAVAIL;
	return irq - group->base;
}

int vmm_host_extirq_find_mapping(struct vmm_host_extirq_group *group,
				 unsigned int offset)
{
	if (offset > group->count)
		return -1;

	return group->base + offset;
}

void vmm_host_extirq_debug_dump(struct vmm_chardev *cdev)
{
	int idx = 0;
	struct vmm_host_irq *irq = NULL;
	struct vmm_host_extirq_group *group = NULL;

	vmm_cprintf(cdev, "%d extended IRQs\n", extirqctrl.count);
	vmm_cprintf(cdev, "  BITMAP:\n");
	for (idx = 0; idx < BITS_TO_LONGS(extirqctrl.count); ++idx) {
		if (0 == (idx % 4)) {
			vmm_cprintf(cdev, "\n    %d:", idx);
		}
		vmm_cprintf(cdev, " 0x%x", extirqctrl.bitmap[idx]);
	}
	vmm_cprintf(cdev, "\n");

	list_for_each_entry(group, &extirqctrl.groups, head) {
		vmm_cprintf(cdev, "  Group from IRQ %d to %d:\n", group->base,
				  group->end);
		for (idx = group->base; idx < group->end; ++idx) {
			irq = extirqctrl.irqs[idx - CONFIG_HOST_IRQ_COUNT];
			if (!irq)
				continue;
			if (idx != irq->num)
				vmm_cprintf(cdev, "WARNING: IRQ %d "
					    "not correctly set\n");
			vmm_cprintf(cdev, "    IRQ %d mapped, name: %s, "
				    "chip: %s\n", idx, irq->name,
				    irq->chip ? irq->chip->name : "None");
		}
	}
}

static void *realloc(void *ptr,
		     unsigned int old_size,
		     unsigned int new_size)
{
	void *new_ptr = NULL;

	if (new_size < old_size)
		return ptr;
	if (NULL == (new_ptr = vmm_zalloc(new_size)))
		return NULL;

	if (!ptr)
		return new_ptr;

	memcpy(new_ptr, ptr, old_size);
	vmm_free(ptr);

	return new_ptr;
}

static int _extirq_expand(void)
{
	unsigned int old_size = extirqctrl.count;
	unsigned int new_size = extirqctrl.count + HOST_EXTIRQ_CHUNK;
	struct vmm_host_irq **irqs = NULL;
	unsigned long *bitmap = NULL;

	irqs = realloc(extirqctrl.irqs,
		       old_size * sizeof (struct vmm_host_irq *),
		       new_size * sizeof (struct vmm_host_irq *));
	if (!irqs) {
		vmm_printf("%s: Failed to reallocate extended IRQ array from "
			   "%d to %d bytes\n", __func__, old_size, new_size);
		return VMM_ENOMEM;
	}

	old_size = BITMAP_SIZE(old_size);
	new_size = BITMAP_SIZE(new_size);

	bitmap = realloc(extirqctrl.bitmap, old_size, new_size);
	if (!bitmap) {
		vmm_printf("%s: Failed to reallocate extended IRQ bitmap from "
			   "%d to %d bytes\n", __func__, old_size, new_size);
		vmm_free(irqs);
		return VMM_ENOMEM;
	}

	extirqctrl.irqs = irqs;
	extirqctrl.bitmap = bitmap;
	extirqctrl.count += HOST_EXTIRQ_CHUNK;

	return VMM_OK;
}

static int extirq_find_free_region(unsigned int size)
{
	int pos = -1;
	int size_log = 0;
	int idx = 0;

	while ((1 << size_log) < size) {
		++size_log;
	}

	if (!size_log || size_log > BITS_PER_LONG)
		return VMM_ENOTAVAIL;

	vmm_mutex_lock(&extirqctrl.lock);
	for (idx = 0; idx < (BITS_TO_LONGS(extirqctrl.count)); ++idx) {
		pos = bitmap_find_free_region(&extirqctrl.bitmap[idx],
					      BITS_PER_LONG, size_log);
		if (pos >= 0)
			break;
	}

	if (pos < 0) {
		/*
		 * Give a second try, reallocate some memory for extended
		 * IRQs
		 */
		if (VMM_OK == _extirq_expand()) {
			pos = bitmap_find_free_region(&extirqctrl.bitmap[idx],
						      BITS_PER_LONG, size_log);
		}
	}
	vmm_mutex_unlock(&extirqctrl.lock);

	if (pos < 0) {
		vmm_printf("%s: Failed to find an extended IRQ region\n",
			   __func__);
		return pos;
	}

	pos += idx * (BITS_PER_LONG);
	return pos;
}

struct vmm_host_extirq_group *
vmm_host_extirq_group_get(unsigned int	irq_num)
{
	struct vmm_host_extirq_group *group = NULL;

	if (irq_num < (CONFIG_HOST_IRQ_COUNT))
		return NULL;

	vmm_mutex_lock(&extirqctrl.lock);
	list_for_each_entry(group, &extirqctrl.groups, head) {
		if ((irq_num > group->base) && (irq_num < group->end))
			return group;
	}
	vmm_mutex_unlock(&extirqctrl.lock);
	vmm_printf("%s: Failed to find IRQ %d group\n", __func__, irq_num);

	return NULL;
}

int vmm_host_extirq_create_mapping(struct vmm_host_extirq_group *group,
				   unsigned int	irq_num)
{
	int name_len = 0;
	char *name = NULL;
	struct vmm_host_irq *irq = NULL;

	if (!group)
		return VMM_ENOTAVAIL;

	if (irq_num > group->count)
		return VMM_ENOTAVAIL;

	if (NULL == (irq = vmm_malloc(sizeof (struct vmm_host_irq)))) {
		vmm_printf("%s: Failed to allocate IRQ\n", __func__);
		return VMM_ENOMEM;
	}

	name_len = strlen(group->of_node->name) + 6;
	__vmm_host_irq_init_desc(irq, group->base + irq_num);

	name = vmm_malloc(name_len);
	if (!name) {
		vmm_printf("%s: Failed to allocate IRQ name\n", __func__);
		vmm_free(irq);
		return VMM_ENOMEM;
	}

	vmm_snprintf(name, name_len, "%s.%d", group->of_node->name,
		     irq_num);
	irq->name = name;

	extirqctrl.irqs[group->base - (CONFIG_HOST_IRQ_COUNT) + irq_num] = irq;

	return group->base + irq_num;
}

void vmm_host_extirq_dispose_mapping(unsigned int irq_num)
{
	struct vmm_host_irq *irq = NULL;

	if (irq_num < (CONFIG_HOST_IRQ_COUNT))
		return;
	extirqctrl.irqs[irq_num - (CONFIG_HOST_IRQ_COUNT)] = NULL;

	irq = vmm_host_extirq_get(irq_num);
	vmm_free((void *)irq->name);
	vmm_free(irq);
}

struct vmm_host_extirq_group *
vmm_host_extirq_add(struct vmm_devtree_node *of_node,
		    unsigned int size,
		    const struct vmm_host_extirq_group_ops *ops,
		    void *host_data)
{
	int pos = 0;
	struct vmm_host_extirq_group *newgroup = NULL;

	newgroup = vmm_zalloc(sizeof (struct vmm_host_extirq_group));

	if (0 > (pos = extirq_find_free_region(size))) {
		vmm_printf("%s: Failed to find available slot for IRQ\n",
			   __func__);
		return NULL;
	}

	INIT_LIST_HEAD(&newgroup->head);
	newgroup->base = pos + (CONFIG_HOST_IRQ_COUNT);
	newgroup->count = size;
	newgroup->end = newgroup->base + size;
	newgroup->host_data = host_data;
	newgroup->of_node = of_node;
	newgroup->ops = ops;

	vmm_mutex_lock(&extirqctrl.lock);
	list_add_tail(&newgroup->head, &extirqctrl.groups);
	vmm_mutex_unlock(&extirqctrl.lock);

	return newgroup;
}

void vmm_host_extirq_remove(struct vmm_host_extirq_group *group)
{
	unsigned int pos = 0;

	if (!group)
		return;

	vmm_mutex_lock(&extirqctrl.lock);
	list_del(&group->head);
	vmm_mutex_unlock(&extirqctrl.lock);
	for (pos = group->base; pos < group->end; ++pos) {
		if (extirqctrl.irqs[pos - (CONFIG_HOST_IRQ_COUNT)])
			vmm_host_extirq_dispose_mapping(pos);
	}
	vmm_free(group);
}

int __cpuinit vmm_host_extirq_init(void)
{
	memset(&extirqctrl, 0, sizeof (struct vmm_host_extirqs_ctrl));
	INIT_MUTEX(&extirqctrl.lock);
	INIT_LIST_HEAD(&extirqctrl.groups);

	return VMM_OK;
}

/* For future use */
const struct vmm_host_extirq_group_ops extirq_simple_ops = {
	/* .xlate = extirq_xlate, */
};
