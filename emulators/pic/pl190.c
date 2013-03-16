/**
 * Copyright (c) 2012 Jean-Chrsitophe Dubois.
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
 * @brief Versatile pl190 (Vector Interrupt Controller) Emulator.
 * @details This source file implements the Versatile pl190 (Vector Interrupt
 * Controller) emulator.
 *
 * The source has been largely adapted from QEMU 0.14.xx hw/pl190.c
 * 
 * Arm PrimeCell PL190 Vector Interrupt Controller
 *
 * Copyright (c) 2006 CodeSourcery.
 * Written by Paul Brook
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_manager.h>
#include <vmm_scheduler.h>
#include <vmm_vcpu_irq.h>
#include <vmm_host_irq.h>
#include <vmm_host_io.h>
#include <vmm_devemu.h>
#include <vmm_stdio.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"ARM PL190 Emulator"
#define MODULE_AUTHOR			"Jean-Christophe Dubois"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			pl190_emulator_init
#define	MODULE_EXIT			pl190_emulator_exit

/* The number of virtual priority levels.  16 user vectors plus the
   unvectored IRQ.  Chained interrupts would require an additional level
   if implemented.  */

#define PL190_NUM_PRIO 17

struct pl190_emulator_state {
	struct vmm_guest *guest;
	struct vmm_emupic *pic;
	vmm_spinlock_t lock;

	/* Configuration */
	u8 id[8];
	u32 num_irq;
	u32 num_base_irq;
	bool is_child_pic;
	u32 parent_irq;

	u32 level;
	u32 soft_level;
	u32 irq_enable;
	u32 fiq_select;
	u8 vect_control[16];
	u32 vect_addr[PL190_NUM_PRIO];
	/* Mask containing interrupts with higher priority than this one.  */
	u32 prio_mask[PL190_NUM_PRIO + 1];
	int protected;
	/* Current priority level.  */
	int priority;
	int prev_prio[PL190_NUM_PRIO];
	int irq;
	int fiq;
};

static inline u32 pl190_emulator_irq_status(struct pl190_emulator_state *s)
{
	return (s->level | s->soft_level) & s->irq_enable & ~s->fiq_select;
}

/* Update interrupts.  */
static void pl190_emulator_update(struct pl190_emulator_state *s)
{
	u32 irqset, fiqset, status;
	struct vmm_vcpu *vcpu = vmm_manager_guest_vcpu(s->guest, 0);

	if (!vcpu) {
		return;
	}

	status = pl190_emulator_irq_status(s);

	if (s->is_child_pic) {
		vmm_devemu_emulate_irq(s->guest, s->parent_irq, status);
	} else {

		irqset = ((status & s->prio_mask[s->priority]) != 0);
		if (irqset) {
			vmm_vcpu_irq_assert(vcpu, CPU_EXTERNAL_IRQ, 0x0);
		} else {
			vmm_vcpu_irq_deassert(vcpu, CPU_EXTERNAL_IRQ);
		}

		fiqset = (((s->level | s->soft_level) & s->fiq_select) != 0);
		if (fiqset) {
			vmm_vcpu_irq_assert(vcpu, CPU_EXTERNAL_FIQ, 0x0);
		} else {
			vmm_vcpu_irq_deassert(vcpu, CPU_EXTERNAL_FIQ);
		}
	}
}

static void pl190_emulator_set_irq(struct pl190_emulator_state *s, int irq,
				   int level)
{
	if (level) {
		s->level |= 1u << irq;
	} else {
		s->level &= ~(1u << irq);
	}

	pl190_emulator_update(s);
}

/* Process IRQ asserted in device emulation framework */
static int pl190_emulator_irq_handle(struct vmm_emupic *epic, 
				     u32 irq, int cpu, int level)
{
	irq_flags_t flags;
	struct pl190_emulator_state *s =
	    (struct pl190_emulator_state *)epic->priv;

	/* Ensure irq is in range (base_irq, base_irq + num_irq) */
	if ((irq < s->num_base_irq) || ((s->num_base_irq + s->num_irq) <= irq)) {
		return VMM_EMUPIC_IRQ_UNHANDLED;
	}

	irq -= s->num_base_irq;

	if (level == (s->level & (1u << irq))) {
		return VMM_EMUPIC_IRQ_HANDLED;
	}

	vmm_spin_lock_irqsave(&s->lock, flags);

	pl190_emulator_set_irq(s, irq, level);

	vmm_spin_unlock_irqrestore(&s->lock, flags);

	return VMM_EMUPIC_IRQ_HANDLED;
}

static void pl190_emulator_update_vectors(struct pl190_emulator_state *s)
{
	u32 mask;
	int i;
	int n;

	mask = 0;

	for (i = 0; i < 16; i++) {
		s->prio_mask[i] = mask;
		if (s->vect_control[i] & 0x20) {
			n = s->vect_control[i] & 0x1f;
			mask |= 1 << n;
		}
	}

	s->prio_mask[16] = mask;

	pl190_emulator_update(s);
}

static int pl190_emulator_read(struct pl190_emulator_state *s, u32 offset,
			       u32 * dst)
{
	int i;

	if (!s || !dst) {
		return VMM_EFAIL;
	}

	if (offset >= 0xfe0 && offset < 0x1000) {
		*dst = s->id[(offset - 0xfe0) >> 2];
		return VMM_OK;
	}

	if (offset >= 0x100 && offset < 0x140) {
		*dst = s->vect_addr[(offset - 0x100) >> 2];
		return VMM_OK;
	}

	if (offset >= 0x200 && offset < 0x240) {
		*dst = s->vect_control[(offset - 0x200) >> 2];
		return VMM_OK;
	}

	switch (offset >> 2) {
	case 0:		/* IRQSTATUS */
		*dst = pl190_emulator_irq_status(s);
		break;
	case 1:		/* FIQSATUS */
		*dst = (s->level | s->soft_level) & s->fiq_select;
		break;
	case 2:		/* RAWINTR */
		*dst = s->level | s->soft_level;
		break;
	case 3:		/* INTSELECT */
		*dst = s->fiq_select;
		break;
	case 4:		/* INTENABLE */
		*dst = s->irq_enable;
		break;
	case 6:		/* SOFTINT */
		*dst = s->soft_level;
		break;
	case 8:		/* PROTECTION */
		*dst = s->protected;
		break;
	case 12:		/* VECTADDR */
		/* Read vector address at the start of an ISR.  Increases the
		   current priority level to that of the current interrupt.  */
		for (i = 0; i < s->priority; i++) {
			if ((s->level | s->soft_level) & s->prio_mask[i])
				break;
		}
		/* Reading this value with no pending interrupts is undefined.
		   We return the default address.  */
		if (i == PL190_NUM_PRIO) {
			*dst = s->vect_addr[16];
		} else {
			if (i < s->priority) {
				s->prev_prio[i] = s->priority;
				s->priority = i;
				pl190_emulator_update(s);
			}
			*dst = s->vect_addr[s->priority];
		}
		break;
	case 13:		/* DEFVECTADDR */
		*dst = s->vect_addr[16];
		break;
	default:
		return VMM_EFAIL;
		break;
	}

	return VMM_OK;
}

static int pl190_emulator_write(struct pl190_emulator_state *s, u32 offset,
				u32 src_mask, u32 src)
{
	if (!s) {
		return VMM_EFAIL;
	}

	src = src & ~src_mask;

	if (offset >= 0x100 && offset < 0x140) {
		s->vect_addr[(offset - 0x100) >> 2] = src;
		pl190_emulator_update_vectors(s);
		return VMM_OK;
	}

	if (offset >= 0x200 && offset < 0x240) {
		s->vect_control[(offset - 0x200) >> 2] = src;
		pl190_emulator_update_vectors(s);
		return VMM_OK;
	}

	switch (offset >> 2) {
	case 0:		/* SELECT */
		/* This is a readonly register, but linux tries to write to it
		   anyway.  Ignore the write.  */
		break;
	case 3:		/* INTSELECT */
		s->fiq_select = src;
		break;
	case 4:		/* INTENABLE */
		s->irq_enable |= src;
		break;
	case 5:		/* INTENCLEAR */
		s->irq_enable &= ~src;
		break;
	case 6:		/* SOFTINT */
		s->soft_level |= src;
		break;
	case 7:		/* SOFTINTCLEAR */
		s->soft_level &= ~src;
		break;
	case 8:		/* PROTECTION */
		/* TODO: Protection (supervisor only access) is not implemented.  */
		s->protected = src & 1;
		break;
	case 12:		/* VECTADDR */
		/* Restore the previous priority level.  The value written is
		   ignored.  */
		if (s->priority < PL190_NUM_PRIO) {
			s->priority = s->prev_prio[s->priority];
		}
		break;
	case 13:		/* DEFVECTADDR */
		s->vect_addr[16] = src;
		break;
	case 0xc0:		/* ITCR */
		if (src) {
			/* Test mode not implemented */
			return VMM_EFAIL;
		}
		break;
	default:
		return VMM_EFAIL;
		break;
	}

	pl190_emulator_update(s);

	return VMM_OK;
}

static int pl190_emulator_device_read(struct vmm_emudev *edev,
					physical_addr_t offset, void *dst,
					u32 dst_len)
{
	struct vmm_vcpu *vcpu = NULL;
	int rc = VMM_OK;
	u32 regval = 0x0;
	struct pl190_emulator_state *s = edev->priv;

	vcpu = vmm_scheduler_current_vcpu();
	if (!vcpu || !vcpu->guest) {
		return VMM_EFAIL;
	}
	if (s->guest->id != vcpu->guest->id) {
		return VMM_EFAIL;
	}

	rc = pl190_emulator_read(s, offset & 0xFFC, &regval);

	if (!rc) {
		regval = (regval >> ((offset & 0x3) * 8));
		switch (dst_len) {
		case 1:
			*(u8 *) dst = regval & 0xFF;
			break;
		case 2:
			*(u16 *) dst = vmm_cpu_to_le16(regval & 0xFFFF);
			break;
		case 4:
			*(u32 *) dst = vmm_cpu_to_le32(regval);
			break;
		default:
			rc = VMM_EFAIL;
			break;
		};
	}

	return rc;
}

static int pl190_emulator_device_write(struct vmm_emudev *edev,
					 physical_addr_t offset, void *src,
					 u32 src_len)
{
	struct vmm_vcpu *vcpu = NULL;
	int rc = VMM_OK, i;
	u32 regmask = 0x0, regval = 0x0;
	struct pl190_emulator_state *s = edev->priv;

	switch (src_len) {
	case 1:
		regmask = 0xFFFFFF00;
		regval = *(u8 *) src;
		break;
	case 2:
		regmask = 0xFFFF0000;
		regval = vmm_le16_to_cpu(*(u16 *) src);
		break;
	case 4:
		regmask = 0x00000000;
		regval = vmm_le32_to_cpu(*(u32 *) src);
		break;
	default:
		return VMM_EFAIL;
		break;
	};

	for (i = 0; i < (offset & 0x3); i++) {
		regmask = (regmask << 8) | ((regmask >> 24) & 0xFF);
	}
	regval = (regval << ((offset & 0x3) * 8));

	vcpu = vmm_scheduler_current_vcpu();
	if (!vcpu || !vcpu->guest) {
		return VMM_EFAIL;
	}
	if (s->guest->id != vcpu->guest->id) {
		return VMM_EFAIL;
	}

	rc = pl190_emulator_write(s, offset & 0xFFC, regmask, regval);

	return rc;
}

static int pl190_emulator_reset(struct vmm_emudev *edev)
{
	int i;
	struct pl190_emulator_state *s = edev->priv;

	for (i = 0; i < 16; i++) {
		s->vect_addr[i] = 0;
		s->vect_control[i] = 0;
	}

	s->vect_addr[16] = 0;
	s->prio_mask[17] = 0xffffffff;
	s->priority = PL190_NUM_PRIO;
	pl190_emulator_update_vectors(s);

	return VMM_OK;
}

static int pl190_emulator_probe(struct vmm_guest *guest,
					 struct vmm_emudev *edev,
					 const struct vmm_devtree_nodeid *eid)
{
	static int pic_number = 0;
	int rc = VMM_OK;
	struct pl190_emulator_state *s;
	const char *attr;
	u32 attrlen;

	s = vmm_malloc(sizeof(struct pl190_emulator_state));
	if (!s) {
		rc = VMM_EFAIL;
		goto pl190_emulator_probe_done;
	}
	memset(s, 0x0, sizeof(struct pl190_emulator_state));

	s->pic = vmm_malloc(sizeof(struct vmm_emupic));
	if (!s->pic) {
		rc = VMM_EFAIL;
		goto pl190_emulator_probe_freestate_fail;
	}
	memset(s->pic, 0x0, sizeof(struct vmm_emupic));

	vmm_sprintf(s->pic->name, "pl190-pic%d", pic_number);
	pic_number++;

	s->pic->type = VMM_EMUPIC_IRQCHIP;
	s->pic->handle = pl190_emulator_irq_handle;
	s->pic->priv = s;

	if ((rc = vmm_devemu_register_pic(guest, s->pic))) {
		rc = VMM_EFAIL;
		vmm_printf("failed to register pic\n");
		goto pl190_emulator_probe_freepic_fail;
	}

	if (eid->data) {
		s->num_irq = ((u32 *) eid->data)[0];
		s->num_base_irq = ((u32 *) eid->data)[1];
		s->id[0] = ((u32 *) eid->data)[2];
		s->id[1] = ((u32 *) eid->data)[3];
		s->id[2] = ((u32 *) eid->data)[4];
		s->id[3] = ((u32 *) eid->data)[5];
		s->id[4] = ((u32 *) eid->data)[6];
		s->id[5] = ((u32 *) eid->data)[7];
		s->id[6] = ((u32 *) eid->data)[8];
		s->id[7] = ((u32 *) eid->data)[9];
	}

	attr = vmm_devtree_attrval(edev->node, "base_irq");
	attrlen = vmm_devtree_attrlen(edev->node, "base_irq");
	if (attr && (attrlen == sizeof(u32))) {
		s->num_base_irq = *(u32 *) attr;
	} else {
		rc = VMM_EFAIL;
		goto pl190_emulator_probe_unregpic_fail;
	}

	attr = vmm_devtree_attrval(edev->node, "child_pic");
	if (attr) {
		s->is_child_pic = TRUE;
	} else {
		s->is_child_pic = FALSE;
	}

	if (s->is_child_pic) {
		attr = vmm_devtree_attrval(edev->node, "parent_irq");
		attrlen = vmm_devtree_attrlen(edev->node, "parent_irq");
		if (attr && (attrlen == sizeof(u32))) {
			s->parent_irq = *(u32 *) attr;
		} else {
			rc = VMM_EFAIL;
			goto pl190_emulator_probe_unregpic_fail;
		}
	}

	edev->priv = s;

	s->guest = guest;
	INIT_SPIN_LOCK(&s->lock);

	goto pl190_emulator_probe_done;

 pl190_emulator_probe_unregpic_fail:
	vmm_devemu_unregister_pic(s->guest, s->pic);
 pl190_emulator_probe_freepic_fail:
	vmm_free(s->pic);
 pl190_emulator_probe_freestate_fail:
	vmm_free(s);

 pl190_emulator_probe_done:
	return rc;
}

static int pl190_emulator_remove(struct vmm_emudev *edev)
{
	int rc;
	struct pl190_emulator_state *s = edev->priv;

	if (s) {
		if (s->pic) {
			rc = vmm_devemu_unregister_pic(s->guest, s->pic);
			if (rc) {
				return rc;
			}
			vmm_free(s->pic);
		}
		vmm_free(s);
		edev->priv = NULL;
	}

	return VMM_OK;
}

static u32 pl190_emulator_configs[] = {
	/* === realview === */
	/* num_irq */ 32,
	/* num_base_irq */ 0,
	/* id0 */ 0x90,
	/* id1 */ 0x11,
	/* id2 */ 0x04,
	/* id3 */ 0x00,
	/* id4 */ 0x0d,
	/* id5 */ 0xf0,
	/* id6 */ 0x05,
	/* id7 */ 0x81,
	/* reserved */ 0,
	/* reserved */ 0,
	/* reserved */ 0,
	/* reserved */ 0,
	/* reserved */ 0,
};

static struct vmm_devtree_nodeid pl190_emulator_emuid_table[] = {
	{.type = "pic",
	 .compatible = "versatilepb,pl190",
	 .data = pl190_emulator_configs,
	 },
	{ /* end of list */ },
};

static struct vmm_emulator pl190_emulator = {
	.name = "pl190",
	.match_table = pl190_emulator_emuid_table,
	.probe = pl190_emulator_probe,
	.read = pl190_emulator_device_read,
	.write = pl190_emulator_device_write,
	.reset = pl190_emulator_reset,
	.remove = pl190_emulator_remove,
};

static int __init pl190_emulator_init(void)
{
	return vmm_devemu_register_emulator(&pl190_emulator);
}

static void __exit pl190_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&pl190_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
