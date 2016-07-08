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
 * @file vmm_host_irqdomain.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @author Anup Patel (anup@brainfault.org)
 * @brief IRQ domain support, kind of Xvior compatible Linux IRQ domain.
 */

#include <vmm_error.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_host_irq.h>
#include <vmm_host_irqext.h>
#include <vmm_host_irqdomain.h>
#include <libs/list.h>
#include <libs/bitmap.h>

struct vmm_host_irqdomain_ctrl {
	vmm_rwlock_t lock;
	struct dlist domains;
};

static struct vmm_host_irqdomain_ctrl idctrl;

int vmm_host_irqdomain_to_hwirq(struct vmm_host_irqdomain *domain,
				unsigned int hirq)
{
	if (!domain)
		return VMM_EINVALID;

	if (hirq < domain->base || hirq >= domain->end)
		return VMM_ENOTAVAIL;

	return hirq - domain->base;
}

int vmm_host_irqdomain_to_hirq(struct vmm_host_irqdomain *domain,
			       unsigned int hwirq)
{
	if (!domain)
		return VMM_EINVALID;

	if (hwirq >= domain->count)
		return VMM_ERANGE;

	return domain->base + hwirq;
}

int vmm_host_irqdomain_find_mapping(struct vmm_host_irqdomain *domain,
				    unsigned int hwirq)
{
	if (!domain)
		return VMM_EINVALID;

	if (hwirq >= domain->count)
		return VMM_ERANGE;

	if (vmm_host_irq_get(domain->base + hwirq))
		return domain->base + hwirq;

	return VMM_ENOTAVAIL;
}

struct vmm_host_irqdomain *vmm_host_irqdomain_match(void *data,
			int (*fn)(struct vmm_host_irqdomain *, void *))
{
	irq_flags_t flags;
	struct vmm_host_irqdomain *domain = NULL;
	struct vmm_host_irqdomain *found = NULL;

	vmm_read_lock_irqsave_lite(&idctrl.lock, flags);

	list_for_each_entry(domain, &idctrl.domains, head) {
		if (fn(domain, data)) {
			found = domain;
			break;
		}
	}

	vmm_read_unlock_irqrestore_lite(&idctrl.lock, flags);

	return found;
}

void vmm_host_irqdomain_debug_dump(struct vmm_chardev *cdev)
{
	int idx = 0;
	irq_flags_t flags;
	struct vmm_host_irq *irq = NULL;
	struct vmm_host_irqdomain *domain = NULL;

	vmm_read_lock_irqsave_lite(&idctrl.lock, flags);

	list_for_each_entry(domain, &idctrl.domains, head) {
		vmm_cprintf(cdev, "  Group from IRQ %d to %d:\n", domain->base,
				  domain->end);
		for (idx = domain->base; idx < domain->end; ++idx) {
			irq = vmm_host_irq_get(idx);
			if (!irq)
				continue;
			if (idx != irq->num)
				vmm_cprintf(cdev, "WARNING: IRQ %d "
					    "not correctly set\n", irq->num);
			vmm_cprintf(cdev, "    IRQ %d mapped, name: %s, "
				    "chip: %s\n", idx, irq->name,
				    irq->chip ? irq->chip->name : "None");
		}
	}

	vmm_read_unlock_irqrestore_lite(&idctrl.lock, flags);
}

struct vmm_host_irqdomain *vmm_host_irqdomain_get(unsigned int hirq)
{
	irq_flags_t flags;
	struct vmm_host_irqdomain *domain = NULL;

	vmm_read_lock_irqsave_lite(&idctrl.lock, flags);

	list_for_each_entry(domain, &idctrl.domains, head) {
		if ((hirq >= domain->base) && (hirq < domain->end)) {
			vmm_read_unlock_irqrestore_lite(&idctrl.lock, flags);
			return domain;
		}
	}

	vmm_read_unlock_irqrestore_lite(&idctrl.lock, flags);

	vmm_printf("%s: Failed to find host IRQ %d domain\n", __func__, hirq);

	return NULL;
}

static int __irqdomain_create_mapping(struct vmm_host_irqdomain *domain,
				      unsigned int hirq,
				      unsigned int hwirq)
{
	int rc = VMM_OK;

	if (hirq < CONFIG_HOST_IRQ_COUNT) {
		rc = __vmm_host_irq_set_hwirq(hirq, hwirq);
	} else {
		rc = vmm_host_irqext_create_mapping(hirq, hwirq);
	}
	if (rc) {
		return rc;
	}

	if (domain->ops && domain->ops->map) {
		rc = domain->ops->map(domain, hirq, hwirq);
		if (rc) {
			if (hirq < CONFIG_HOST_IRQ_COUNT) {
				__vmm_host_irq_set_hwirq(hirq, hirq);
			} else {
				vmm_host_irqext_dispose_mapping(hirq);
			}
			return rc;
		}
	}

	return VMM_OK;
}

static void __irqdomain_dispose_mapping(struct vmm_host_irqdomain *domain,
					unsigned int hirq)
{
	if (domain->ops && domain->ops->unmap) {
		domain->ops->unmap(domain, hirq);
	}

	if (hirq < CONFIG_HOST_IRQ_COUNT) {
		__vmm_host_irq_set_hwirq(hirq, hirq);
	} else {
		vmm_host_irqext_dispose_mapping(hirq);
	}
}

int vmm_host_irqdomain_create_mapping(struct vmm_host_irqdomain *domain,
				      unsigned int hwirq)
{
	int rc = VMM_OK;
	unsigned int hirq;
	irq_flags_t flags;

	if (!domain)
		return VMM_ENOTAVAIL;

	if (hwirq >= domain->count)
		return VMM_ENOTAVAIL;

	hirq = domain->base + hwirq;

	vmm_spin_lock_irqsave_lite(&domain->bmap_lock, flags);
	if (bitmap_isset(domain->bmap, hwirq)) {
		vmm_spin_unlock_irqrestore_lite(&domain->bmap_lock, flags);
		return hirq;
	}
	bitmap_set(domain->bmap, hwirq, 1);
	vmm_spin_unlock_irqrestore_lite(&domain->bmap_lock, flags);

	rc = __irqdomain_create_mapping(domain, hirq, hwirq);
	if (rc) {
		vmm_spin_lock_irqsave_lite(&domain->bmap_lock, flags);
		bitmap_clear(domain->bmap, hwirq, 1);
		vmm_spin_unlock_irqrestore_lite(&domain->bmap_lock, flags);
		return rc;
	}

	return hirq;
}

void vmm_host_irqdomain_dispose_mapping(unsigned int hirq)
{
	int hwirq;
	irq_flags_t flags;
	struct vmm_host_irqdomain *domain = vmm_host_irqdomain_get(hirq);

	if (!domain) {
		return;
	}

	hwirq = vmm_host_irqdomain_to_hwirq(domain, hirq);
	if (hwirq < 0) {
		return;
	}

	vmm_spin_lock_irqsave_lite(&domain->bmap_lock, flags);
	if (!bitmap_isset(domain->bmap, hwirq)) {
		vmm_spin_unlock_irqrestore_lite(&domain->bmap_lock, flags);
		return;
	}
	bitmap_clear(domain->bmap, hwirq, 1);
	vmm_spin_unlock_irqrestore_lite(&domain->bmap_lock, flags);

	__irqdomain_dispose_mapping(domain, hirq);
}

int vmm_host_irqdomain_alloc(struct vmm_host_irqdomain *domain,
			     unsigned int irq_count)
{
	int rc;
	irq_flags_t flags;
	bool found = false;
	unsigned int i, j, hirq, hwirq, count;

	if (!domain || !irq_count || (domain->count > irq_count)) {
		return VMM_EINVALID;
	}

	vmm_spin_lock_irqsave_lite(&domain->bmap_lock, flags);
	count = 0;
	for (hwirq = 0; hwirq < domain->count; hwirq++) {
		if (bitmap_isset(domain->bmap, hwirq))
			count = 0;
		else
			count++;
		if (count == irq_count) {
			found = true;
			hwirq = hwirq - (count - 1);
			break;
		}
	}
	if (!found) {
		vmm_spin_unlock_irqrestore_lite(&domain->bmap_lock, flags);
		return VMM_ENOENT;
	}
	bitmap_set(domain->bmap, hwirq, irq_count);
	vmm_spin_unlock_irqrestore_lite(&domain->bmap_lock, flags);

	hirq = domain->base + hwirq;
	for (i = 0; i < irq_count; i++) {
		rc = __irqdomain_create_mapping(domain, hirq + i, hwirq + i);
		if (rc) {
			for (j = 0; j < i; j++) {
				__irqdomain_dispose_mapping(domain, hirq + j);
			}
			return rc;
		}
	}

	return hirq;
}

void vmm_host_irqdomain_free(struct vmm_host_irqdomain *domain,
			     unsigned int hirq, unsigned int irq_count)
{
	irq_flags_t flags;
	unsigned int i, hwirq;

	if (!domain ||
	    (hirq < domain->base) ||
	    ((hirq + irq_count) < domain->base) ||
	    ((domain->base + domain->count) <= hirq) ||
	    ((domain->base + domain->count) <= (hirq + irq_count)))
		return;

	for (i = 0; i < irq_count; i++) {
		hwirq = hirq - domain->base + i;

		vmm_spin_lock_irqsave_lite(&domain->bmap_lock, flags);
		if (!bitmap_isset(domain->bmap, hwirq)) {
			vmm_spin_unlock_irqrestore_lite(&domain->bmap_lock, flags);
			continue;
		}
		bitmap_clear(domain->bmap, hwirq, 1);
		vmm_spin_unlock_irqrestore_lite(&domain->bmap_lock, flags);

		__irqdomain_dispose_mapping(domain, hirq);
	}
}

int vmm_host_irqdomain_xlate(struct vmm_host_irqdomain *domain,
			     const u32 *intspec, unsigned int intsize,
			     unsigned long *out_hwirq, unsigned int *out_type)
{
	if (!domain || !intspec || !out_hwirq || !out_type) {
		return VMM_EINVALID;
	}

	/* If domain has no translation, then we assume interrupt line */
	if (!domain->ops || !domain->ops->xlate) {
		*out_hwirq = intspec[0];
	} else {
		return domain->ops->xlate(domain, domain->of_node, intspec,
					  intsize, out_hwirq, out_type);
	}

	return VMM_OK;
}

int vmm_host_irqdomain_xlate_onecell(struct vmm_host_irqdomain *domain,
			struct vmm_devtree_node *node,
			const u32 *intspec, unsigned int intsize,
			unsigned long *out_hwirq, unsigned int *out_type)
{
	if (WARN_ON(intsize != 1))
		return VMM_EINVALID;

	*out_hwirq = intspec[0];
	*out_type = VMM_IRQ_TYPE_NONE;

	return VMM_OK;
}

struct vmm_host_irqdomain *vmm_host_irqdomain_add(
				struct vmm_devtree_node *of_node,
				int base, unsigned int size,
				const struct vmm_host_irqdomain_ops *ops,
				void *host_data)
{
	int pos = 0;
	irq_flags_t flags;
	unsigned long *bmap;
	struct vmm_host_irqdomain *newdomain = NULL;

	if (!of_node || !size || !ops) {
		return NULL;
	}
	if ((base >= 0) &&
	    ((CONFIG_HOST_IRQ_COUNT <= base) ||
	     (CONFIG_HOST_IRQ_COUNT <= (base + size)))) {
		return NULL;
	}

	bmap = vmm_zalloc(bitmap_estimate_size(size));
	if (!bmap)
		return NULL;

	newdomain = vmm_zalloc(sizeof(struct vmm_host_irqdomain));
	if (!newdomain) {
		vmm_free(bmap);
		return NULL;
	}

	if (base < 0) {
		if ((pos = vmm_host_irqext_alloc_region(size)) < 0) {
			vmm_printf("%s: Failed to find available slot for IRQ\n",
				   __func__);
			vmm_free(bmap);
			vmm_free(newdomain);
			return NULL;
		}
	} else {
		pos = base;
	}

	vmm_devtree_ref_node(of_node);
	INIT_LIST_HEAD(&newdomain->head);
	newdomain->base = pos;
	newdomain->count = size;
	newdomain->end = newdomain->base + size;
	newdomain->host_data = host_data;
	newdomain->of_node = of_node;
	newdomain->ops = ops;
	INIT_SPIN_LOCK(&newdomain->bmap_lock);
	newdomain->bmap = bmap;

	vmm_write_lock_irqsave_lite(&idctrl.lock, flags);
	list_add_tail(&newdomain->head, &idctrl.domains);
	vmm_write_unlock_irqrestore_lite(&idctrl.lock, flags);

	return newdomain;
}

void vmm_host_irqdomain_remove(struct vmm_host_irqdomain *domain)
{
	unsigned int pos = 0;
	irq_flags_t flags;

	if (!domain)
		return;

	vmm_write_lock_irqsave_lite(&idctrl.lock, flags);
	list_del(&domain->head);
	vmm_write_unlock_irqrestore_lite(&idctrl.lock, flags);

	for (pos = domain->base; pos < domain->end; ++pos) {
		vmm_host_irqext_dispose_mapping(pos);
	}

	vmm_devtree_dref_node(domain->of_node);
	vmm_free(domain);
}

int __cpuinit vmm_host_irqdomain_init(void)
{
	memset(&idctrl, 0, sizeof(struct vmm_host_irqdomain_ctrl));
	INIT_RW_LOCK(&idctrl.lock);
	INIT_LIST_HEAD(&idctrl.domains);

	return VMM_OK;
}

/* For future use */
const struct vmm_host_irqdomain_ops irqdomain_simple_ops = {
	/* .xlate = extirq_xlate, */
};
