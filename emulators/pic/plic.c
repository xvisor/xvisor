/**
 * Copyright (c) 2019 Anup Patel.
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
 * @file plic.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief SiFive Platform Level Interrupt Controller (PLIC) Emulator.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_vcpu_irq.h>
#include <vmm_devemu.h>

#define MODULE_DESC			"RISC-V PLIC Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			plic_emulator_init
#define	MODULE_EXIT			plic_emulator_exit

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
 * ...		    (higher context IDs would fit here, but wouldn't fit
 *		     inside the per-context priority vector)
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

#define REG_SIZE		0x1000000

struct plic_state;

struct plic_context {
	/* State to which this belongs */
	struct plic_state *s;

	/* Static Configuration */
	u32 num;

	/* Local IRQ state */
	vmm_spinlock_t irq_lock;
	u8 irq_priority_threshold;
	u32 irq_enable[MAX_DEVICES/32];
	u32 irq_pending[MAX_DEVICES/32];
	u8 irq_pending_priority[MAX_DEVICES];
	u32 irq_claimed[MAX_DEVICES/32];
};

struct plic_state {
	/* Guest to which this belongs */
	struct vmm_guest *guest;

	/* Static Configuration */
	u32 base_irq;
	u32 num_irq;
	u32 num_irq_word;
	u32 max_prio;
	u32 parent_irq;

	/* Context Array */
	u32 num_context;
	struct plic_context *contexts;

	/* Global IRQ state */
	vmm_spinlock_t irq_lock;
	u8 irq_priority[MAX_DEVICES];
	u32 irq_level[MAX_DEVICES/32];
};

/* Note: Must be called with c->irq_lock held */
static u32 __plic_context_best_pending_irq(struct plic_state *s,
					   struct plic_context *c)
{
	u8 best_irq_prio = 0;
	u32 i, j, irq, best_irq = 0;

	for (i = 0; i < s->num_irq_word; i++) {
		if (!c->irq_pending[i]) {
			continue;
		}

		for (j = 0; j < 32; j++) {
			irq = i * 32 + j;
			if ((s->num_irq <= irq) ||
			    !(c->irq_pending[i] & (1 << j)) ||
			    (c->irq_claimed[i] & (1 << j))) {
				continue;
			}
			if (!best_irq ||
			    (best_irq_prio < c->irq_pending_priority[irq])) {
				best_irq = irq;
				best_irq_prio = c->irq_pending_priority[irq];
			}
		}
	}

	return best_irq;
}

/* Note: Must be called with c->irq_lock held */
static void __plic_context_irq_update(struct plic_state *s,
				      struct plic_context *c)
{
	u32 best_irq = __plic_context_best_pending_irq(s, c);
	struct vmm_vcpu *vcpu = vmm_manager_guest_vcpu(s->guest, c->num / 2);

	if (best_irq) {
		vmm_vcpu_irq_assert(vcpu, s->parent_irq, 0x0);
	} else {
		vmm_vcpu_irq_deassert(vcpu, s->parent_irq);
	}
}

/* Note: Must be called with c->irq_lock held */
static u32 __plic_context_irq_claim(struct plic_state *s,
				    struct plic_context *c)
{
	u32 best_irq = __plic_context_best_pending_irq(s, c);
	struct vmm_vcpu *vcpu = vmm_manager_guest_vcpu(s->guest, c->num / 2);

	vmm_vcpu_irq_clear(vcpu, s->parent_irq);

	if (best_irq) {
		c->irq_claimed[best_irq / 32] |= (1 << (best_irq % 32));
	}

	__plic_context_irq_update(s, c);

	return best_irq;
}

static void plic_irq_handle(u32 irq, int cpu, int level, void *opaque)
{
	bool irq_marked = FALSE;
	irq_flags_t flags, flags1;
	u8 i, irq_prio, irq_word;
	u32 irq_mask;
	struct plic_context *c = NULL;
	struct plic_state *s = opaque;

	vmm_spin_lock_irqsave(&s->irq_lock, flags);

	if (irq < s->base_irq || (s->base_irq + s->num_irq) <= irq) {
		goto done;
	}
	irq -= s->base_irq;
	if (irq == 0) {
		goto done;
	}

	irq_prio = s->irq_priority[irq];
	irq_word = irq / 32;
	irq_mask = 1 << (irq % 32);

	if (level) {
		s->irq_level[irq_word] |= irq_mask;
	} else {
		s->irq_level[irq_word] &= ~irq_mask;
	}

	/*
	 * Note: PLIC interrupts are level-triggered. As of now,
	 * there is no notion of edge-triggered interrupts.
	 */
	for (i = 0; i < s->num_context; i++) {
		c = &s->contexts[i];
		vmm_spin_lock_irqsave(&c->irq_lock, flags1);
		if (c->irq_enable[irq_word] & irq_mask) {
			if (level) {
				c->irq_pending[irq_word] |= irq_mask;
				c->irq_pending_priority[irq] = irq_prio;
			} else {
				c->irq_pending[irq_word] &= ~irq_mask;
				c->irq_pending_priority[irq] = 0;
				c->irq_claimed[irq_word] &= ~irq_mask;
			}
			__plic_context_irq_update(s, c);
			irq_marked = TRUE;
		}
		vmm_spin_unlock_irqrestore(&c->irq_lock, flags1);
		if (irq_marked) {
			break;
		}
	}

done:
	vmm_spin_unlock_irqrestore(&s->irq_lock, flags);
}

static int plic_priority_read(struct plic_state *s,
			      physical_addr_t offset, u32 *dst)
{
	irq_flags_t flags;
	u32 irq = (offset >> 2);

	if (irq == 0 || irq >= s->num_irq) {
		return VMM_EINVALID;
	}

	vmm_spin_lock_irqsave(&s->irq_lock, flags);
	*dst = s->irq_priority[irq];
	vmm_spin_unlock_irqrestore(&s->irq_lock, flags);

	return VMM_OK;
}

static int plic_priority_write(struct plic_state *s,
			       physical_addr_t offset, u32 src_mask, u32 src)
{
	irq_flags_t flags;
	u32 val, irq = (offset >> 2);

	if (irq == 0 || irq >= s->num_irq) {
		return VMM_EINVALID;
	}

	vmm_spin_lock_irqsave(&s->irq_lock, flags);
	val = s->irq_priority[irq] & src_mask;
	val |= src & ~src_mask;
	val &= ((1 << PRIORITY_PER_ID) - 1);
	s->irq_priority[irq] = val;
	vmm_spin_unlock_irqrestore(&s->irq_lock, flags);

	return VMM_OK;
}

static int plic_context_enable_read(struct plic_state *s,
				    struct plic_context *c,
				    physical_addr_t offset,
				    u32 *dst)
{
	irq_flags_t flags;
	u32 irq_word = offset >> 2;

	if (s->num_irq_word < irq_word) {
		return VMM_EINVALID;
	}

	vmm_spin_lock_irqsave(&c->irq_lock, flags);
	*dst = c->irq_enable[irq_word];
	vmm_spin_unlock_irqrestore(&c->irq_lock, flags);

	return VMM_OK;
}

static int plic_context_enable_write(struct plic_state *s,
				     struct plic_context *c,
				     physical_addr_t offset,
				     u32 src_mask, u32 src)
{
	u8 irq_prio;
	irq_flags_t flags, flags1;
	u32 i, irq, irq_mask;
	u32 irq_word = offset >> 2;
	u32 old_val, new_val, xor_val;

	if (s->num_irq_word < irq_word)
		return VMM_EINVALID;

	vmm_spin_lock_irqsave(&s->irq_lock, flags);

	vmm_spin_lock_irqsave(&c->irq_lock, flags1);

	old_val = c->irq_enable[irq_word];
	new_val = (old_val & src_mask) | (src & ~src_mask);
	if (irq_word == 0) {
		new_val &= ~0x1;
	}
	c->irq_enable[irq_word] = new_val;

	xor_val = old_val ^ new_val;
	for (i = 0; i < 32; i++) {
		irq = irq_word * 32 + i;
		irq_mask = 1 << i;
		irq_prio = s->irq_priority[irq];
		if (!(xor_val & irq_mask))
			continue;
		if ((new_val & irq_mask) &&
		    (s->irq_level[irq_word] & irq_mask)) {
			c->irq_pending[irq_word] |= irq_mask;
			c->irq_pending_priority[irq] = irq_prio;
		} else if (!(new_val & irq_mask)) {
			c->irq_pending[irq_word] &= ~irq_mask;
			c->irq_pending_priority[irq] = 0;
			c->irq_claimed[irq_word] &= ~irq_mask;
		}
	}

	__plic_context_irq_update(s, c);

	vmm_spin_unlock_irqrestore(&c->irq_lock, flags1);

	vmm_spin_unlock_irqrestore(&s->irq_lock, flags);

	return VMM_OK;
}

static int plic_context_read(struct plic_state *s,
			     struct plic_context *c,
			     physical_addr_t offset,
			     u32 *dst)
{
	int rc = VMM_OK;
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&c->irq_lock, flags);

	switch (offset) {
	case CONTEXT_THRESHOLD:
		*dst = c->irq_priority_threshold;
		break;
	case CONTEXT_CLAIM:
		*dst = __plic_context_irq_claim(s, c);
		break;
	default:
		rc = VMM_EINVALID;
		break;
	};

	vmm_spin_unlock_irqrestore(&c->irq_lock, flags);

	return rc;
}

static int plic_context_write(struct plic_state *s,
			      struct plic_context *c,
			      physical_addr_t offset,
			      u32 src_mask, u32 src)
{
	u32 val;
	int rc = VMM_OK;
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&c->irq_lock, flags);

	switch (offset) {
	case CONTEXT_THRESHOLD:
		val = c->irq_priority_threshold & src_mask;
		val |= (src & ~src_mask);
		val &= ((1 << PRIORITY_PER_ID) - 1);
		if (val <= s->max_prio) {
			c->irq_priority_threshold = val;
		} else {
			rc = VMM_EINVALID;
		}
		break;
	case CONTEXT_CLAIM:
		break;
	default:
		rc = VMM_EINVALID;
		break;
	};

	if (rc == VMM_OK) {
		__plic_context_irq_update(s, c);
	}

	vmm_spin_unlock_irqrestore(&c->irq_lock, flags);

	return rc;
}

static int plic_emulator_read(struct vmm_emudev *edev,
			      physical_addr_t offset,
			      u32 *dst, u32 size)
{
	u32 cntx;
	int rc = VMM_EINVALID;
	struct plic_state *s = edev->priv;

	offset &= ~0x3;

	if (PRIORITY_BASE <= offset && offset < ENABLE_BASE) {
		rc = plic_priority_read(s, offset, dst);
	} else if (ENABLE_BASE <= offset && offset < CONTEXT_BASE) {
		cntx = (offset - ENABLE_BASE) / ENABLE_PER_HART;
		offset -= cntx * ENABLE_PER_HART + ENABLE_BASE;
		if (cntx < s->num_context) {
			rc = plic_context_enable_read(s, &s->contexts[cntx],
						      offset, dst);
		}
	} else if (CONTEXT_BASE <= offset && offset < REG_SIZE) {
		cntx = (offset - CONTEXT_BASE) / CONTEXT_PER_HART;
		offset -= cntx * CONTEXT_PER_HART + CONTEXT_BASE;
		if (cntx < s->num_context) {
			rc = plic_context_read(s, &s->contexts[cntx],
					       offset, dst);
		}
	}

	return rc;
}

static int plic_emulator_write(struct vmm_emudev *edev,
			       physical_addr_t offset,
			       u32 src_mask, u32 src, u32 size)
{
	u32 cntx;
	int rc = VMM_EINVALID;
	struct plic_state *s = edev->priv;

	offset &= ~0x3;

	if (PRIORITY_BASE <= offset && offset < ENABLE_BASE) {
		rc = plic_priority_write(s, offset, src_mask, src);
	} else if (ENABLE_BASE <= offset && offset < CONTEXT_BASE) {
		cntx = (offset - ENABLE_BASE) / ENABLE_PER_HART;
		offset -= cntx * ENABLE_PER_HART + ENABLE_BASE;
		if (cntx < s->num_context) {
			rc = plic_context_enable_write(s, &s->contexts[cntx],
						       offset, src_mask, src);
		}
	} else if (CONTEXT_BASE <= offset && offset < REG_SIZE) {
		cntx = (offset - CONTEXT_BASE) / CONTEXT_PER_HART;
		offset -= cntx * CONTEXT_PER_HART + CONTEXT_BASE;
		if (cntx < s->num_context) {
			rc = plic_context_write(s, &s->contexts[cntx],
						offset, src_mask, src);
		}
	}

	return rc;
}

static int plic_emulator_reset(struct vmm_emudev *edev)
{
	int i, j;
	irq_flags_t flags;
	struct plic_context *c;
	struct plic_state *s = edev->priv;

	vmm_spin_lock_irqsave(&s->irq_lock, flags);

	for (i = 0; i < MAX_DEVICES; i++) {
		s->irq_priority[i] = 0;
	}
	for (i = 0; i < MAX_DEVICES/32; i++) {
		s->irq_level[i] = 0;
	}

	for (i = 0; i < s->num_context; i++) {
		c = &s->contexts[i];

		vmm_spin_lock_irqsave(&c->irq_lock, flags);

		c->irq_priority_threshold = 0;
		for (j = 0; j < MAX_DEVICES/32; j++) {
			c->irq_enable[i] = 0;
			c->irq_pending[i] = 0;
			c->irq_claimed[i] = 0;
		}
		for (j = 0; j < MAX_DEVICES; j++) {
			c->irq_pending_priority[i] = 0;
		}

		vmm_spin_unlock_irqrestore(&c->irq_lock, flags);
	}

	vmm_spin_unlock_irqrestore(&s->irq_lock, flags);

	return VMM_OK;
}

static struct vmm_devemu_irqchip plic_irqchip = {
	.name = "PLIC",
	.handle = plic_irq_handle,
};

static int plic_emulator_probe(struct vmm_guest *guest,
			       struct vmm_emudev *edev,
			       const struct vmm_devtree_nodeid *eid)
{
	u32 i;
	int rc = VMM_OK;
	struct plic_context *c;
	struct plic_state *s;

	s = vmm_zalloc(sizeof(struct plic_state));
	if (!s) {
		rc = VMM_ENOMEM;
		goto plic_emulator_probe_done;
	}
	s->guest = guest;

	if (vmm_devtree_read_u32(edev->node, "base_irq", &s->base_irq)) {
		s->base_irq = 0;
	}

	if (vmm_devtree_read_u32(edev->node, "num_irq", &s->num_irq)) {
		s->num_irq = MAX_DEVICES;
	}
	if (s->num_irq > MAX_DEVICES) {
		rc = VMM_EINVALID;
		goto plic_emulator_probe_freestate_fail;
	}
	s->num_irq_word = s->num_irq / 32;
	if ((s->num_irq_word * 32) < s->num_irq)
		s->num_irq_word++;

	if (vmm_devtree_read_u32(edev->node, "max_priority", &s->max_prio)) {
		s->max_prio = 1UL << PRIORITY_PER_ID;
	}
	if (s->max_prio > (1UL << PRIORITY_PER_ID)) {
		rc = VMM_EINVALID;
		goto plic_emulator_probe_freestate_fail;
	}

	rc = vmm_devtree_read_u32(edev->node, "parent_irq", &s->parent_irq);
	if (rc) {
		goto plic_emulator_probe_freestate_fail;
	}

	s->num_context = guest->vcpu_count * 2;
	if (s->num_context > MAX_CONTEXTS) {
		rc = VMM_ENODEV;
		goto plic_emulator_probe_freestate_fail;
	}

	s->contexts = vmm_zalloc(sizeof(*s->contexts) * s->num_context);
	if (!s->contexts) {
		rc = VMM_ENOMEM;
		goto plic_emulator_probe_done;
	}
	for (i = 0; i < s->num_context; i++) {
		c = &s->contexts[i];
		c->s = s;
		c->num = i;
		INIT_SPIN_LOCK(&c->irq_lock);
	}

	INIT_SPIN_LOCK(&s->irq_lock);
	edev->priv = s;

	for (i = s->base_irq; i < (s->base_irq + s->num_irq); i++) {
		vmm_devemu_register_irqchip(guest, i, &plic_irqchip, s);
	}

	goto plic_emulator_probe_done;

 plic_emulator_probe_freestate_fail:
	vmm_free(s);

 plic_emulator_probe_done:
	return rc;
}

static int plic_emulator_remove(struct vmm_emudev *edev)
{
	u32 i;
	struct plic_state *s = edev->priv;

	if (!s) {
		return VMM_EFAIL;
	}

	for (i = s->base_irq; i < (s->base_irq + s->num_irq); i++) {
		vmm_devemu_unregister_irqchip(s->guest, i, &plic_irqchip, s);
	}
	vmm_free(s->contexts);
	vmm_free(s);
	edev->priv = NULL;

	return VMM_OK;
}

static struct vmm_devtree_nodeid plic_emulator_emuid_table[] = {
	{.type = "pic",
	 .compatible = "sifive,plic0",
	 },
	{ /* end of list */ },
};

VMM_DECLARE_EMULATOR_SIMPLE(plic_emulator,
			    "plic",
			    plic_emulator_emuid_table,
			    VMM_DEVEMU_LITTLE_ENDIAN,
			    plic_emulator_probe,
			    plic_emulator_remove,
			    plic_emulator_reset,
			    NULL,
			    plic_emulator_read,
			    plic_emulator_write);

static int __init plic_emulator_init(void)
{
	return vmm_devemu_register_emulator(&plic_emulator);
}

static void __exit plic_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&plic_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
