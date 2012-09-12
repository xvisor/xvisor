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
#include <vic.h>

struct vic_chip_data {
	u32 irq_offset;
	virtual_addr_t cpu_base;
};

#ifndef VIC_MAX_NR
#define VIC_MAX_NR	1
#endif

static struct vic_chip_data vic_data[VIC_MAX_NR];


static inline virtual_addr_t vic_cpu_base(struct vmm_host_irq *irq)
{
	struct vic_chip_data *v_data = vmm_host_irq_get_chip_data(irq);

	return v_data->cpu_base;
}

static inline u32 vic_irq(struct vmm_host_irq *irq)
{
	struct vic_chip_data *v_data = vmm_host_irq_get_chip_data(irq);

	return irq->num - v_data->irq_offset;
}

u32 vic_active_irq(u32 vic_nr)
{
	u32 ret = 0;
	u32 int_status;
	volatile void *base = (volatile void *) vic_data[vic_nr].cpu_base;

	int_status = vmm_readl(base + VIC_IRQ_STATUS);

	if (int_status) {
		for (ret = 0; ret < 32; ret++) {
			if ((int_status >> ret) & 1) {
				break;
			}
		}

		ret += vic_data[vic_nr].irq_offset;
	}

	return ret;
}

static void vic_mask_irq(struct vmm_host_irq *irq)
{
	volatile void *base = (volatile void *) vic_cpu_base(irq);
	unsigned int irq_no = vic_irq(irq);

	vmm_writel(1 << irq_no, base + VIC_INT_ENABLE_CLEAR);
}

static void vic_unmask_irq(struct vmm_host_irq *irq)
{
	volatile void *base = (volatile void *) vic_cpu_base(irq);
	unsigned int irq_no = vic_irq(irq);

	vmm_writel(1 << irq_no, base + VIC_INT_ENABLE);
}

static void vic_ack_irq(struct vmm_host_irq *irq)
{
	volatile void *base = (volatile void *) vic_cpu_base(irq);
	unsigned int irq_no = vic_irq(irq);

	vmm_writel(1 << irq_no, base + VIC_INT_ENABLE_CLEAR);
	/* moreover, clear the soft-triggered, in case it was the reason */
	vmm_writel(1 << irq_no, base + VIC_INT_SOFT_CLEAR);

	vmm_writel(1 << irq_no, base + VIC_INT_ENABLE);
}

static struct vmm_host_irq_chip vic_chip = {
	.name = "VIC",
	.irq_mask = vic_mask_irq,
	.irq_unmask =vic_unmask_irq,
	.irq_eoi = vic_ack_irq,
};

static void vic_disable(void *base)
{
	vmm_writel(0, base + VIC_INT_SELECT);
	vmm_writel(0, base + VIC_INT_ENABLE);
	vmm_writel(~0, base + VIC_INT_ENABLE_CLEAR);
	vmm_writel(0, base + VIC_ITCR);
	vmm_writel(~0, base + VIC_INT_SOFT_CLEAR);
}

static void vic_clear_interrupts(void *base)
{
	unsigned int i;

	vmm_writel(0, base + VIC_PL190_VECT_ADDR);
	for (i = 0; i < 19; i++) {
		unsigned int value;

		value = vmm_readl(base + VIC_PL190_VECT_ADDR);
		vmm_writel(value, base + VIC_PL190_VECT_ADDR);
	}
}

static void vic_init2(void *base)
{
	int i;

	for (i = 0; i < 16; i++) {
		void *reg = base + VIC_VECT_CNTL0 + (i * 4);
		vmm_writel(VIC_VECT_CNTL_ENABLE | i, reg);
	}

	vmm_writel(32, base + VIC_PL190_DEF_VECT_ADDR);
}

void __init vic_cpu_init(struct vic_chip_data *v_data)
{
	int i;
	void *base = (void *) v_data->cpu_base;

	for (i = v_data->irq_offset; i < v_data->irq_offset + 32; i++) {
		vmm_host_irq_set_chip(i, &vic_chip);
		vmm_host_irq_set_chip_data(i, v_data);
	}

	/* Disable all interrupts initially. */
	vic_disable(base);

	/* Make sure we clear all existing interrupts */
	vic_clear_interrupts(base);

	vic_init2(base);
}

int __init vic_init(u32 vic_nr, u32 irq_start, virtual_addr_t cpu_base)
{
	struct vic_chip_data *v_data;

	if (vic_nr >= VIC_MAX_NR) {
		return VMM_EFAIL;
	}

	v_data = &vic_data[vic_nr];
	v_data->cpu_base = cpu_base;
	v_data->irq_offset = irq_start;

	vic_cpu_init(v_data);

	return VMM_OK;
}
