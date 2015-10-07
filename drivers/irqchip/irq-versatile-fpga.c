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
 * @file irq-versatile-fpga.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Support for Versatile FPGA-based IRQ controllers
 *
 * The source has been largely adapted from Linux
 * drivers/irqchip/irq-versatile-fpga.c
 *
 * The original code is licensed under the GPL.
 *
 * Support for Versatile FPGA-based IRQ controllers
 */

#include <vmm_error.h>
#include <vmm_limits.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_host_irqdomain.h>
#include <libs/stringlib.h>

#define NR_IRQS			32
#define IRQ_STATUS		0x00
#define IRQ_RAW_STATUS		0x04
#define IRQ_ENABLE_SET		0x08
#define IRQ_ENABLE_CLEAR	0x0c
#define INT_SOFT_SET		0x10
#define INT_SOFT_CLEAR		0x14
#define FIQ_STATUS		0x20
#define FIQ_RAW_STATUS		0x24
#define FIQ_ENABLE		0x28
#define FIQ_ENABLE_SET		0x28
#define FIQ_ENABLE_CLEAR	0x2C
#define PICEN_STATUS		0x20
#define PICEN_SET		0x20
#define PCIEN_CLEAR		0x24

/**
 * struct fpga_irq_data - irq data container for the FPGA IRQ controller
 * @base: memory offset in virtual memory
 * @chip: chip container for this instance
 * @domain: IRQ domain for this instance
 * @valid: mask for valid IRQs on this controller
 * @used_irqs: number of active IRQs on this controller
 */
struct fpga_irq_data {
	struct vmm_host_irqdomain *domain;
	struct vmm_devtree_node *node;
	void *base;
	struct vmm_host_irq_chip chip;
	u32 valid;
	u8 used_irqs;
};

#ifndef CONFIG_VERSATILE_FPGA_IRQ_NR
#define CONFIG_VERSATILE_FPGA_IRQ_NR	4
#endif

/* we cannot allocate memory when the controllers are initially registered */
static struct fpga_irq_data fpga_irq_devices[CONFIG_VERSATILE_FPGA_IRQ_NR];
static int fpga_irq_id = 0;

static void fpga_irq_mask(struct vmm_host_irq *d)
{
	struct fpga_irq_data *f = vmm_host_irq_get_chip_data(d);
	u32 mask = 1 << d->hwirq;

	vmm_writel(mask, f->base + IRQ_ENABLE_CLEAR);
}

static void fpga_irq_unmask(struct vmm_host_irq *d)
{
	struct fpga_irq_data *f = vmm_host_irq_get_chip_data(d);
	u32 mask = 1 << d->hwirq;

	vmm_writel(mask, f->base + IRQ_ENABLE_SET);
}

static u32 fpga_find_active_irq(struct fpga_irq_data *f)
{
	u32 ret = UINT_MAX;
	u32 hwirq, int_status;

	int_status = vmm_readl(f->base + IRQ_STATUS);
	if (!int_status) {
		goto done;
	}

	for (hwirq = 0; hwirq < NR_IRQS; hwirq++) {
		if (!(int_status & (1 << hwirq))) {
			continue;
		}
		ret = vmm_host_irqdomain_find_mapping(f->domain, hwirq);
		goto done;
	}

done:
	return ret;
}

static u32 fpga_active_irq(u32 cpu_nr)
{
	u32 i, ret;

	for (i = 0; i < fpga_irq_id; i++) {
		ret = fpga_find_active_irq(&fpga_irq_devices[i]);
		if (ret != UINT_MAX) {
			return ret;
		}
	}

	return UINT_MAX;
}

static vmm_irq_return_t fpga_handle_cascade_irq(int irq, void *dev)
{
	vmm_host_generic_irq_exec(fpga_find_active_irq(dev));

	return VMM_IRQ_HANDLED;
}

static void __init fpga_cascade_irq(struct fpga_irq_data *f,
				    const char *name,
				    u32 parent_irq)
{
	if (vmm_host_irq_register(parent_irq, name, 
				  fpga_handle_cascade_irq, f)) {
		BUG();
	}
}

static struct vmm_host_irqdomain_ops fpga_ops = {
	.xlate = vmm_host_irqdomain_xlate_onecell,
};

void __init fpga_irq_init(void *base, const char *name,
			  u32 irq_start, u32 parent_irq,
			  u32 valid, struct vmm_devtree_node *node)
{
	int hirq;
	u32 hwirq;
	struct fpga_irq_data *f;

	if (fpga_irq_id >= array_size(fpga_irq_devices)) {
		vmm_printf("%s: too few FPGA IRQ controllers, "
			   "increase CONFIG_VERSATILE_FPGA_IRQ_NR\n",
			   __func__);
		return;
	}
	f = &fpga_irq_devices[fpga_irq_id];

	f->domain = vmm_host_irqdomain_add(node, (int)irq_start, NR_IRQS,
					   &fpga_ops, NULL);
	if (!f->domain) {
		vmm_printf("%s: failed to add vmm_host_irqdomain\n",
			   __func__);
		return;
	}

	f->node = node;
	f->base = base;
	f->chip.name = name;
	f->chip.irq_ack = fpga_irq_mask;
	f->chip.irq_mask = fpga_irq_mask;
	f->chip.irq_unmask = fpga_irq_unmask;
	f->valid = valid;
	f->used_irqs = 0;

	if (parent_irq != UINT_MAX) {
		fpga_cascade_irq(f, name, parent_irq);
	} else {
		vmm_host_irq_set_active_callback(fpga_active_irq);
	}

	/* This will allocate all valid descriptors in the linear case */
	for (hwirq = 0; hwirq < NR_IRQS; hwirq++) {
		if (!(valid & (1 << hwirq))) {
			continue;
		}

		hirq = vmm_host_irqdomain_create_mapping(f->domain, hwirq);
		BUG_ON(hirq < 0);
		vmm_host_irq_set_chip(hirq, &f->chip);
		vmm_host_irq_set_chip_data(hirq, f);
		vmm_host_irq_set_handler(hirq, vmm_handle_level_irq);

		f->used_irqs++;
	}

	fpga_irq_id++;
}

static int __init fpga_init(struct vmm_devtree_node *node)
{
	int rc;
	virtual_addr_t base;
	u32 clear_mask;
	u32 valid_mask;
	u32 picen_mask;
	u32 irq_start;
	u32 parent_irq;

	BUG_ON(!vmm_smp_is_bootcpu());

	rc = vmm_devtree_request_regmap(node, &base, 0, "Versatile SIC");
	WARN(rc, "unable to map fpga irq registers\n");

	if (vmm_devtree_read_u32(node, "irq_start", &irq_start)) {
		irq_start = 0;
	}

	if (vmm_devtree_read_u32(node, "clear-mask", &clear_mask)) {
		clear_mask = 0;
	}

	if (vmm_devtree_read_u32(node, "valid-mask", &valid_mask)) {
		valid_mask = 0;
	}

	/* Some chips are cascaded from a parent IRQ */
	parent_irq = vmm_devtree_irq_parse_map(node, 0);
	if (!parent_irq) {
		parent_irq = UINT_MAX;
	}

	fpga_irq_init((void *)base, "FPGA",
		      irq_start, parent_irq,
		      valid_mask, node);

	vmm_writel(clear_mask, (void *)base + IRQ_ENABLE_CLEAR);
	vmm_writel(clear_mask, (void *)base + FIQ_ENABLE_CLEAR);

	/* For VersatilePB, we have interrupts from 21 to 31 capable
	 * of being routed directly to the parent interrupt controller
	 * (i.e. VIC). This is controlled by setting PIC_ENABLEx.
	 */
	if (!vmm_devtree_read_u32(node, "picen-mask", &picen_mask)) {
		vmm_writel(picen_mask, (void *)base + PICEN_SET);
	}

	return 0;
}

VMM_HOST_IRQ_INIT_DECLARE(vfpga, "arm,versatile-sic", fpga_init);
