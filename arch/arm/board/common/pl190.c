/**
 * Copyright (c) 2012 Jean-Christophe Dubois.
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
 * @file pl190.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief PL190 Vectored Interrupt Controller source
 */

#include <vmm_error.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <pl190.h>

struct pl190_chip_data {
	u32 irq_offset;
	virtual_addr_t cpu_base;
};

#ifndef PL190_MAX_NR
#define PL190_MAX_NR      1
#endif

static struct pl190_chip_data pl190_data[PL190_MAX_NR];

#define pl190_write(val, addr)	vmm_writel(val, (void *)(addr))
#define pl190_read(addr)	vmm_readl((void *)(addr))

static inline virtual_addr_t pl190_cpu_base(struct vmm_host_irq *irq)
{
	struct pl190_chip_data *pl190_data = vmm_host_irq_get_chip_data(irq);
	return pl190_data->cpu_base;
}

static inline u32 pl190_irq(struct vmm_host_irq *irq)
{
	struct pl190_chip_data *pl190_data = vmm_host_irq_get_chip_data(irq);
	return irq->num - pl190_data->irq_offset;
}

u32 pl190_active_irq(u32 pl190_nr)
{
	u32 ret = 0;
	u32 int_status;

	int_status =
	    pl190_read(pl190_data[pl190_nr].cpu_base + PL190_IRQ_STATUS);

	if (int_status) {
		for (ret = 0; ret < 32; ret++) {
			if ((int_status >> ret) & 1) {
				break;
			}
		}

		ret += pl190_data[pl190_nr].irq_offset;
	}

	return ret;
}

static void pl190_eoi_irq(struct vmm_host_irq *irq)
{
	u32 mask = 1 << pl190_irq(irq);

	pl190_write(mask, pl190_cpu_base(irq) + PL190_INT_ENABLE_CLEAR);

	pl190_write(mask, pl190_cpu_base(irq) + PL190_INT_SOFT_CLEAR);

	pl190_write(mask, pl190_cpu_base(irq) + PL190_INT_ENABLE);
}

static void pl190_mask_irq(struct vmm_host_irq *irq)
{
	u32 mask = 1 << pl190_irq(irq);

	pl190_write(mask, pl190_cpu_base(irq) + PL190_INT_ENABLE_CLEAR);
}

static void pl190_unmask_irq(struct vmm_host_irq *irq)
{
	u32 mask = 1 << pl190_irq(irq);

	pl190_write(mask, pl190_cpu_base(irq) + PL190_INT_ENABLE);
}

static struct vmm_host_irq_chip pl190_chip = {
	.name = "PL190",
	.irq_mask = pl190_mask_irq,
	.irq_unmask = pl190_unmask_irq,
	.irq_eoi = pl190_eoi_irq,
};

void __init pl190_cpu_init(struct pl190_chip_data *pl190)
{
	int i;

	for (i = pl190->irq_offset; i < pl190->irq_offset + 32; i++) {
		vmm_host_irq_set_chip(i, &pl190_chip);
		vmm_host_irq_set_chip_data(i, pl190);
	}

	pl190_write(0, pl190->cpu_base + PL190_INT_SELECT);
	pl190_write(0, pl190->cpu_base + PL190_INT_ENABLE);
	pl190_write(~0, pl190->cpu_base + PL190_INT_ENABLE_CLEAR);
	pl190_write(0, pl190->cpu_base + PL190_IRQ_STATUS);
	pl190_write(0, pl190->cpu_base + PL190_ITCR);
	pl190_write(~0, pl190->cpu_base + PL190_INT_SOFT_CLEAR);

#if 0
	pl190_write(0, pl190->cpu_base + PL190_VECT_ADDR);
	for (i = 0; i < 19; i++) {
		unsigned int value =
		    pl190_read(pl190->cpu_base + PL190_VECT_ADDR);
		pl190_write(value, pl190->cpu_base + PL190_VECT_ADDR);
	}

	for (i = 0; i < 16; i++) {
		pl190_write(VIC_VECT_CNTL_ENABLE | i,
			    pl190->cpu_base + PL190_VECT_CNTL0 + (i * 4));
	}

	pl190_write(32, pl190->cpu_base + PL190_DEF_VECT_ADDR);
#endif
}

int __init pl190_init(u32 pl190_nr, u32 irq_start, virtual_addr_t cpu_base)
{
	struct pl190_chip_data *pl190;

	if (pl190_nr >= PL190_MAX_NR) {
		return VMM_EFAIL;
	}

	pl190 = &pl190_data[pl190_nr];
	pl190->cpu_base = cpu_base;
	pl190->irq_offset = irq_start;

	pl190_cpu_init(pl190);

	return VMM_OK;
}
