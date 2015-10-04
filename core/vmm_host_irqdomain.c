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

struct vmm_host_irqdomain_ctrl {
	vmm_rwlock_t lock;
	struct dlist domains;
};

static struct vmm_host_irqdomain_ctrl idctrl;

int vmm_host_irqdomain_to_hwirq(struct vmm_host_irqdomain *domain,
				unsigned int hirq)
{
	if (hirq < domain->base || hirq >= domain->end)
		return VMM_ENOTAVAIL;

	return hirq - domain->base;
}

int vmm_host_irqdomain_find_mapping(struct vmm_host_irqdomain *domain,
				    unsigned int hwirq)
{
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
	struct vmm_host_irqdomain *domain= NULL;
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
					    "not correctly set\n");
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

int vmm_host_irqdomain_create_mapping(struct vmm_host_irqdomain *domain,
				      unsigned int hwirq)
{
	int rc;

	if (!domain)
		return VMM_ENOTAVAIL;

	if (hwirq >= domain->count)
		return VMM_ENOTAVAIL;

	rc = vmm_host_irqext_create_mapping(domain->base + hwirq, hwirq);
	if (rc) {
		return rc;
	}

	if (domain->ops && domain->ops->map) {
		rc = domain->ops->map(domain, domain->base + hwirq, hwirq);
		if (rc) {
			vmm_host_irqext_dispose_mapping(domain->base + hwirq);
			return rc;
		}
	}

	return domain->base + hwirq;
}

int vmm_host_irqdomain_dispose_mapping(unsigned int hirq)
{
	struct vmm_host_irqdomain *domain = vmm_host_irqdomain_get(hirq);

	if (!domain) {
		return VMM_ENOTAVAIL;
	}

	if (domain->ops && domain->ops->unmap) {
		domain->ops->unmap(domain, hirq);
	}

	return vmm_host_irqext_dispose_mapping(hirq);
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
	struct vmm_host_irqdomain *newdomain = NULL;

	if (!of_node) {
		return NULL;
	}
	if ((base >= 0) &&
	    ((CONFIG_HOST_IRQ_COUNT <= base) ||
	     (CONFIG_HOST_IRQ_COUNT <= (base + size)))) {
		return NULL;
	}

	newdomain = vmm_zalloc(sizeof(struct vmm_host_irqdomain));
	if (!newdomain) {
		return NULL;
	}

	if (base < 0) {
		if ((pos = vmm_host_irqext_alloc_region(size)) < 0) {
			vmm_printf("%s: Failed to find available slot for IRQ\n",
				   __func__);
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
