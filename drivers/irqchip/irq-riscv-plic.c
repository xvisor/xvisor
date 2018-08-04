/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file irq-riscv-plic.c
 * @author Oza Pawandeep (oza.pawandeep@gmail.com)
 * @brief riscv PLIC driver
 *
 * 
 */

#include <vmm_error.h>
#include <vmm_limits.h>
#include <vmm_compiler.h>
#include <vmm_stdio.h>
#include <vmm_smp.h>
#include <vmm_heap.h>
#include <vmm_resource.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <vmm_host_irq.h>
#include <vmm_host_irqdomain.h>
#include <vmm_devtree.h>
#include <libs/bitops.h>
#include <drv/irqchip/riscv-intc.h>

/*
 * From the RISC-V Privlidged Spec v1.10:
 *
 * Global interrupt sources are assigned small unsigned integer identifiers,
 * beginning at the value 1.  An interrupt ID of 0 is reserved to mean no
 * interrupt.  Interrupt identifiers are also used to break ties when two or
 * more interrupt sources have the same assigned priority. Smaller values of
 * interrupt ID take precedence over larger values of interrupt ID.
 *
 * While the RISC-V supervisor spec doesn't define the maximum number of
 * devices supported by the PLIC, the largest number supported by devices
 * marked as 'riscv,plic0' (which is the only device type this driver supports,
 * and is the only extant PLIC as of now) is 1024.  As mentioned above, device
 * 0 is defined to be non-existant so this device really only supports 1023
 * devices.
 */
 
#define MAX_DEVICES	1024
#define MAX_CONTEXTS	15872

/*
 * The PLIC consists of memory-mapped control registers, with a memory map as
 * follows:
 *
 * base + 0x000000: Reserved (interrupt source 0 does not exist)
 * base + 0x000004: Interrupt source 1 priority
 * base + 0x000008: Interrupt source 2 priority
 * ...
 * base + 0x000FFC: Interrupt source 1023 priority
 * base + 0x001000: Pending 0
 * base + 0x001FFF: Pending
 * base + 0x002000: Enable bits for sources 0-31 on context 0
 * base + 0x002004: Enable bits for sources 32-63 on context 0
 * ...
 * base + 0x0020FC: Enable bits for sources 992-1023 on context 0
 * base + 0x002080: Enable bits for sources 0-31 on context 1
 * ...
 * base + 0x002100: Enable bits for sources 0-31 on context 2
 * ...
 * base + 0x1F1F80: Enable bits for sources 992-1023 on context 15871
 * base + 0x1F1F84: Reserved
 * ...              (higher context IDs would fit here, but wouldn't fit
 *                   inside the per-context priority vector)
 * base + 0x1FFFFC: Reserved
 * base + 0x200000: Priority threshold for context 0
 * base + 0x200004: Claim/complete for context 0
 * base + 0x200008: Reserved
 * ...
 * base + 0x200FFC: Reserved
 * base + 0x201000: Priority threshold for context 1
 * base + 0x201004: Claim/complete for context 1
 * ...
 * base + 0xFFE000: Priority threshold for context 15871
 * base + 0xFFE004: Claim/complete for context 15871
 * base + 0xFFE008: Reserved
 * ...
 * base + 0xFFFFFC: Reserved
 */

/* Each interrupt source has a priority register associated with it. */
#define PRIORITY_BASE		0
#define PRIORITY_PER_ID		4

/*
 * Each hart context has a vector of interupt enable bits associated with it.
 * There's one bit for each interrupt source.
 */
#define ENABLE_BASE		0x2000
#define ENABLE_PER_HART		0x80

/*
 * Each hart context has a set of control registers associated with it.  Right
 * now there's only two: a source priority threshold over which the hart will
 * take an interrupt, and a register to claim interrupts.
 */
#define CONTEXT_BASE		0x200000
#define CONTEXT_PER_HART	0x1000
#define CONTEXT_THRESHOLD	0
#define CONTEXT_CLAIM		4

struct plic_handler {
	bool present;
	int contextid;
	struct plic_data *data;
};

struct plic_data {
	struct vmm_host_irqdomain *domain;
	virtual_addr_t base_va;
	struct plic_handler *handler;
	int handlers;
	void *base;
	u32 ndev;
};

static struct plic_data plic;

/* Addressing helper functions. */
static inline
u32 *plic_enable_vector(struct plic_data *data, int contextid)
{
	return data->base + ENABLE_BASE + contextid * ENABLE_PER_HART;
}

static inline
u32 *plic_priority(struct plic_data *data, int hwirq)
{
	return data->base + PRIORITY_BASE + hwirq * PRIORITY_PER_ID;
}

static inline
u32 *plic_hart_threshold(struct plic_data *data, int contextid)
{
	return data->base + CONTEXT_BASE + CONTEXT_PER_HART * contextid + CONTEXT_THRESHOLD;
}

static inline
u32 *plic_hart_claim(struct plic_data *data, int contextid)
{
	return data->base + CONTEXT_BASE + CONTEXT_PER_HART * contextid + CONTEXT_CLAIM;
}

/*
 * Handling an interrupt is a two-step process: first you claim the interrupt
 * by reading the claim register, then you complete the interrupt by writing
 * that source ID back to the same claim register.  This automatically enables
 * and disables the interrupt, so there's nothing else to do.
 */
static inline
u32 plic_claim(struct plic_data *data, int contextid)
{
	return vmm_readl(plic_hart_claim(data, contextid));
}

static inline
void plic_complete(struct plic_data *data, int contextid, u32 claim)
{
	vmm_writel(claim, plic_hart_claim(data, contextid));
}

/* Explicit interrupt masking. */
static void plic_disable(struct plic_data *data, int contextid, int hwirq)
{
	u32 *reg = plic_enable_vector(data, contextid) + (hwirq / 32);
	u32 mask = ~(1 << (hwirq % 32));

	vmm_writel(vmm_readl(reg) & mask, reg);
}

static void plic_enable(struct plic_data *data, int contextid, int hwirq)
{
	u32 *reg = plic_enable_vector(data, contextid) + (hwirq / 32);
	u32 bit = 1 << (hwirq % 32);

	vmm_writel(vmm_readl(reg) | bit, reg);
}

static void plic_irq_enable(struct vmm_host_irq *d)
{
	void *priority;
	int i;
 
	priority = plic_priority(&plic, d->hwirq);
 	vmm_writel(1, priority);

	for (i = 0; i < plic.handlers; ++i)
		if (plic.handler[i].present)
			plic_enable(&plic, plic.handler[i].contextid, d->hwirq);
}

static void plic_irq_disable(struct vmm_host_irq *d)
{
	int i;
	void *priority = plic_priority(&plic, d->hwirq);
	
	vmm_writel(0, priority);
	for (i = 0; i < plic.handlers; ++i)
		if (plic.handler[i].present)
			plic_disable(&plic, plic.handler[i].contextid, d->hwirq);
}

static struct vmm_host_irq_chip plic_chip = {
	.name		= "riscv-plic",
	.irq_enable	= plic_irq_enable,
	.irq_disable	= plic_irq_disable,
};

static vmm_irq_return_t plic_chained_handle_irq(int hirq, void *dev)
{
	struct plic_handler *handler = dev;
	int pirq, virq;

	while ((pirq = plic_claim(&plic, handler->contextid))) {
		virq = vmm_host_irqdomain_find_mapping(plic.domain, pirq);
		vmm_host_generic_irq_exec(virq);
		plic_complete(&plic, handler->contextid, pirq);
	}

	return VMM_IRQ_HANDLED;
}

static struct vmm_host_irqdomain_ops plic_ops = {
	.xlate = vmm_host_irqdomain_xlate_onecell,
};

static void __cpuinit plic_handler_init(struct plic_handler *handler,
					struct vmm_devtree_node *node,
					int irq_index,
					const char *irq_name)
{
	int parent_irq, hwirq;

	/* Disable all interrupts for this handler */
	for (hwirq = 1; hwirq <= plic.ndev; ++hwirq) {
		plic_disable(&plic, handler->contextid, hwirq);
	}

	/* Find parent IRQ for this handler */	
	parent_irq = vmm_devtree_irq_parse_map(node, irq_index);
	if (!parent_irq) {
		return;
	}

	/* Register parent IRQ for this handler */
	if (vmm_host_irq_register(parent_irq, irq_name,
				  plic_chained_handle_irq, handler)) {
		return;
	}

	/* HWIRQ prio must be > this to trigger an interrupt */
	vmm_writel(0, plic_hart_threshold(&plic, handler->contextid));

	/* Mark handler as present */
	handler->present = true;
}

static int __cpuinit plic_init(struct vmm_devtree_node *node)
{
	int i, rc;
	struct plic_handler *handler;
	int hwirq, virq;
	physical_addr_t reg_phys;
	physical_size_t reg_size;
	u32 cpu = vmm_smp_processor_id();

	if (!vmm_smp_is_bootcpu()) {
		goto common;
	}

	/* Allocate handlers */
	plic.handlers = CONFIG_CPU_COUNT * 2;
	plic.handler = vmm_zalloc(sizeof(*plic.handler) * plic.handlers);

	if (!plic.handler) {
		return VMM_ENOMEM;
	}
	
	for (i = 0; i < plic.handlers; ++i) {
		handler = &plic.handler[i];
		handler->present = false;
		handler->contextid = i;
		handler->data = &plic;
	}

	/* Find number of devices */
	if (vmm_devtree_read_u32(node, "riscv,ndev", &plic.ndev)) {
		plic.ndev = MAX_DEVICES;
	}
	plic.ndev = plic.ndev + 1; 

	/* Create IRQ domain */
	plic.domain = vmm_host_irqdomain_add(node, (int)RISCV_IRQ_COUNT,
					     plic.ndev, &plic_ops, NULL);
	if (!plic.domain) {
		vmm_free(plic.handler);
		return VMM_EFAIL;
	}

	/* Find register base and size */
	rc = vmm_devtree_regaddr(node, &reg_phys, 0);
	if (rc) {
		vmm_host_irqdomain_remove(plic.domain);
		vmm_free(plic.handler);
		return rc;
	}
	reg_size = CONTEXT_BASE + plic.handlers * CONTEXT_PER_HART;

	/* Map registers */
	vmm_request_mem_region(reg_phys, reg_size, "RISCV PLIC");
	plic.base_va = vmm_host_iomap(reg_phys, reg_size);
	plic.base = (void *)plic.base_va;

	/* interrupt 0 is no device/interrupt. */
	for (hwirq = 1; hwirq < plic.ndev; ++hwirq) {
		virq = vmm_host_irqdomain_create_mapping(plic.domain, hwirq);
		vmm_host_irq_set_chip(virq, &plic_chip);
		vmm_host_irq_set_handler(virq, vmm_handle_fast_eoi);
				
	}

common:
	/* Machine CPU handler */
	plic_handler_init(&plic.handler[cpu * 2], node, 0,
			  "riscv-plic-m");

	/* Supervisor CPU handler */
	plic_handler_init(&plic.handler[cpu * 2 + 1], node, 1,
			  "riscv-plic-s");

	return VMM_OK;
}

VMM_HOST_IRQ_INIT_DECLARE(riscvplic, "riscv,plic0", plic_init);
