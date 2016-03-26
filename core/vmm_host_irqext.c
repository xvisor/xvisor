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
 * @file vmm_host_irqext.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Extended IRQ support, kind of Xvior compatible Linux IRQ domain.
 */

#include <vmm_error.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_host_irqext.h>
#include <libs/list.h>

#define HOST_IRQEXT_CHUNK		32
#define BITMAP_SIZE(X)							\
	(((X) + (BITS_PER_BYTE) - 1)  / (BITS_PER_BYTE))

struct vmm_host_irqext_ctrl {
	vmm_rwlock_t lock;
	unsigned int count;
	unsigned long *bitmap;
	struct vmm_host_irq **irqs;
};

static struct vmm_host_irqext_ctrl iectrl;

struct vmm_host_irq *__vmm_host_irqext_get(unsigned int hirq)
{
	irq_flags_t flags;
	struct vmm_host_irq *irq = NULL;

	if (hirq < CONFIG_HOST_IRQ_COUNT)
		return NULL;
	hirq -= CONFIG_HOST_IRQ_COUNT;

	vmm_read_lock_irqsave_lite(&iectrl.lock, flags);

	if (hirq < iectrl.count)
		irq = iectrl.irqs[hirq];

	vmm_read_unlock_irqrestore_lite(&iectrl.lock, flags);

	return irq;
}

void vmm_host_irqext_debug_dump(struct vmm_chardev *cdev)
{
	int idx = 0;
	irq_flags_t flags;

	vmm_read_lock_irqsave_lite(&iectrl.lock, flags);

	vmm_cprintf(cdev, "%d extended IRQs\n", iectrl.count);
	vmm_cprintf(cdev, "  BITMAP:\n");
	for (idx = 0; idx < BITS_TO_LONGS(iectrl.count); ++idx) {
		if (0 == (idx % 4)) {
			vmm_cprintf(cdev, "\n    %d:", idx);
		}
		vmm_cprintf(cdev, " 0x%lx", iectrl.bitmap[idx]);
	}
	vmm_cprintf(cdev, "\n");

	vmm_read_unlock_irqrestore_lite(&iectrl.lock, flags);
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

static int _irqext_expand(void)
{
	unsigned int old_size = iectrl.count;
	unsigned int new_size = iectrl.count + HOST_IRQEXT_CHUNK;
	struct vmm_host_irq **irqs = NULL;
	unsigned long *bitmap = NULL;

	irqs = realloc(iectrl.irqs,
		       old_size * sizeof (struct vmm_host_irq *),
		       new_size * sizeof (struct vmm_host_irq *));
	if (!irqs) {
		vmm_printf("%s: Failed to reallocate extended IRQ array from "
			   "%d to %d bytes\n", __func__, old_size, new_size);
		return VMM_ENOMEM;
	}

	old_size = BITMAP_SIZE(old_size);
	new_size = BITMAP_SIZE(new_size);

	bitmap = realloc(iectrl.bitmap, old_size, new_size);
	if (!bitmap) {
		vmm_printf("%s: Failed to reallocate extended IRQ bitmap from "
			   "%d to %d bytes\n", __func__, old_size, new_size);
		vmm_free(irqs);
		return VMM_ENOMEM;
	}

	iectrl.irqs = irqs;
	iectrl.bitmap = bitmap;
	iectrl.count += HOST_IRQEXT_CHUNK;

	return VMM_OK;
}

int vmm_host_irqext_alloc_region(unsigned int size)
{
	int tries=3, pos = -1;
	int size_log = 0;
	int idx = 0;
	irq_flags_t flags;

	while ((1 << size_log) < size) {
		++size_log;
	}

	if (!size_log || size_log > BITS_PER_LONG)
		return VMM_ENOTAVAIL;

	vmm_write_lock_irqsave_lite(&iectrl.lock, flags);

try_again:
	for (idx = 0; idx < BITS_TO_LONGS(iectrl.count); ++idx) {
		pos = bitmap_find_free_region(&iectrl.bitmap[idx],
					      BITS_PER_LONG, size_log);
		if (pos >= 0) {
			bitmap_set(&iectrl.bitmap[idx], pos, size_log);
			pos += idx * (BITS_PER_LONG);
			break;
		}
	}

	if (pos < 0) {
		/*
		 * Give a second try, reallocate some memory for extended
		 * IRQs
		 */
		if (VMM_OK == _irqext_expand()) {
			if (tries) {
				tries--;
				goto try_again;
			}
		}
	}

	vmm_write_unlock_irqrestore_lite(&iectrl.lock, flags);

	if (pos < 0) {
		vmm_printf("%s: Failed to find an extended IRQ region\n",
			   __func__);
		return pos;
	}

	return pos + CONFIG_HOST_IRQ_COUNT;
}

int vmm_host_irqext_create_mapping(u32 hirq, u32 hwirq)
{
	int rc = VMM_OK;
	irq_flags_t flags;
	struct vmm_host_irq *irq = NULL;

	if (hirq < CONFIG_HOST_IRQ_COUNT) {
		return vmm_host_irq_set_hwirq(hirq, hwirq);
	}

	vmm_write_lock_irqsave_lite(&iectrl.lock, flags);

	if (iectrl.count <= (hirq - CONFIG_HOST_IRQ_COUNT)) {
		rc = VMM_EINVALID;
		goto done;
	}

	irq = iectrl.irqs[hirq - CONFIG_HOST_IRQ_COUNT];
	if (irq) {
		rc = VMM_OK;
		goto done;
	}

	if (NULL == (irq = vmm_malloc(sizeof(struct vmm_host_irq)))) {
		vmm_printf("%s: Failed to allocate host IRQ\n", __func__);
		rc = VMM_ENOMEM;
		goto done;
	}

	__vmm_host_irq_init_desc(irq, hirq, hwirq);

	iectrl.irqs[hirq - CONFIG_HOST_IRQ_COUNT] = irq;

done:
	vmm_write_unlock_irqrestore_lite(&iectrl.lock, flags);

	return rc;
}

int vmm_host_irqext_dispose_mapping(u32 hirq)
{
	int rc = VMM_OK;
	irq_flags_t flags;
	struct vmm_host_irq *irq = NULL;

	if (hirq < CONFIG_HOST_IRQ_COUNT) {
		return vmm_host_irq_set_hwirq(hirq, hirq);
	}

	vmm_write_lock_irqsave_lite(&iectrl.lock, flags);

	if (iectrl.count <= (hirq - CONFIG_HOST_IRQ_COUNT)) {
		rc = VMM_EINVALID;
		goto done;
	}

	irq = iectrl.irqs[hirq - CONFIG_HOST_IRQ_COUNT];
	iectrl.irqs[hirq - CONFIG_HOST_IRQ_COUNT] = NULL;

	if (irq) {
		if (irq->name) {
			vmm_free((void *)irq->name);
		}
		vmm_free(irq);
	}

done:
	vmm_write_unlock_irqrestore_lite(&iectrl.lock, flags);

	return rc;
}

int __cpuinit vmm_host_irqext_init(void)
{
	memset(&iectrl, 0, sizeof (struct vmm_host_irqext_ctrl));
	INIT_RW_LOCK(&iectrl.lock);

	return VMM_OK;
}
