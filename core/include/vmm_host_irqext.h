/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
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
 * @file vmm_host_irqext.h
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Extended Host IRQ support.
 */

#ifndef _VMM_HOST_IRQEXT_H__
#define _VMM_HOST_IRQEXT_H__

#include <vmm_types.h>
#include <vmm_host_irq.h>

struct vmm_chardev;

struct vmm_host_irq *__vmm_host_irqext_get(u32 hirq);

int vmm_host_irqext_alloc_region(u32 size);

int vmm_host_irqext_create_mapping(u32 hirq, u32 hwirq);

int vmm_host_irqext_dispose_mapping(u32 hirq);

void vmm_host_irqext_debug_dump(struct vmm_chardev *cdev);

int vmm_host_irqext_init(void);

u32 vmm_host_irqext_count(void);

#endif /* _VMM_HOST_IRQEXT_H__ */
