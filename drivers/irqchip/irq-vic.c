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
 * @file irq-vic.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief PL190 Vectored Interrupt Controller source
 */

#include <vmm_error.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>

#define VIC_IRQ_STATUS			0x00
#define VIC_FIQ_STATUS			0x04
#define VIC_RAW_STATUS			0x08
#define VIC_INT_SELECT			0x0c	/* 1 = FIQ, 0 = IRQ */
#define VIC_INT_ENABLE			0x10	/* 1 = enable, 0 = disable */
#define VIC_INT_ENABLE_CLEAR		0x14
#define VIC_INT_SOFT			0x18
#define VIC_INT_SOFT_CLEAR		0x1c
#define VIC_PROTECT			0x20
#define VIC_PL190_VECT_ADDR		0x30	/* PL190 only */
#define VIC_PL190_DEF_VECT_ADDR		0x34	/* PL190 only */

#define VIC_VECT_ADDR0			0x100	/* 0 to 15 (0..31 PL192) */
#define VIC_VECT_CNTL0			0x200	/* 0 to 15 (0..31 PL192) */
#define VIC_ITCR			0x300	/* VIC test control register */

#define VIC_VECT_CNTL_ENABLE		(1 << 5)

#define VIC_PL192_VECT_ADDR		0xF00

struct vic_chip_data {
	u32 irq_offset;
	struct vmm_devtree_node *node;
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

static u32 vic_active_irq(u32 cpu_nr)
{
	u32 ret = 0;
	u32 int_status;
	volatile void *base = (volatile void *)vic_data[0].cpu_base;

	int_status = vmm_readl(base + VIC_IRQ_STATUS);

	if (int_status) {
		for (ret = 0; ret < 32; ret++) {
			if ((int_status >> ret) & 1) {
				break;
			}
		}

		ret += vic_data[0].irq_offset;
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
	.irq_ack = vic_ack_irq,
	.irq_mask = vic_mask_irq,
	.irq_unmask =vic_unmask_irq,
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

static int __cpuinit vic_devtree_init(struct vmm_devtree_node *node,
				      struct vmm_devtree_node *parent)
{
	int i, rc;
	virtual_addr_t base;
	struct vic_chip_data *v_data;

	rc = vmm_devtree_regmap(node, &base, 0);
	if (rc) {
		return rc;
	}

	v_data = &vic_data[0];
	v_data->node = node;
	v_data->cpu_base = base;
	v_data->irq_offset = 0;

	for (i = v_data->irq_offset; i < v_data->irq_offset + 32; i++) {
		vmm_host_irq_set_chip(i, &vic_chip);
		vmm_host_irq_set_chip_data(i, v_data);
		vmm_host_irq_set_handler(i, vmm_handle_level_irq);
	}

	/* Disable all interrupts initially. */
	vic_disable((void *)v_data->cpu_base);

	/* Make sure we clear all existing interrupts */
	vic_clear_interrupts((void *)v_data->cpu_base);

	vic_init2((void *)v_data->cpu_base);

	vmm_host_irq_set_active_callback(vic_active_irq);

	return VMM_OK;
}

static int __cpuinit vic_init(struct vmm_devtree_node *node)
{
	BUG_ON(!vmm_smp_is_bootcpu());

	return vic_devtree_init(node, NULL);
}
VMM_HOST_IRQ_INIT_DECLARE(vvic, "arm,versatile-vic", vic_init);
