/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_devemu.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for device emulation framework
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_host_irq.h>
#include <vmm_mutex.h>
#include <vmm_guest_aspace.h>
#include <vmm_devemu.h>
#include <libs/stringlib.h>

struct vmm_devemu_vcpu_context {
	u32 rd_victim;
	physical_addr_t rd_gstart[CONFIG_VGPA2REG_CACHE_SIZE];
	physical_size_t rd_gend[CONFIG_VGPA2REG_CACHE_SIZE];
	struct vmm_region *rd_reg[CONFIG_VGPA2REG_CACHE_SIZE];
	u32 wr_victim;
	physical_addr_t wr_gstart[CONFIG_VGPA2REG_CACHE_SIZE];
	physical_size_t wr_gend[CONFIG_VGPA2REG_CACHE_SIZE];
	struct vmm_region *wr_reg[CONFIG_VGPA2REG_CACHE_SIZE];
};

struct vmm_devemu_h2g_irq {
	struct vmm_guest *guest;
	u32 host_irq;
	u32 guest_irq;
};

struct vmm_devemu_guest_context {
	struct dlist emupic_list;
	u32 h2g_irq_count;
	struct vmm_devemu_h2g_irq *h2g_irq;
};

struct vmm_devemu_ctrl {
	struct vmm_mutex emu_lock;
        struct dlist emu_list;
};

static struct vmm_devemu_ctrl dectrl;

int vmm_devemu_emulate_read(struct vmm_vcpu *vcpu, 
			    physical_addr_t gphys_addr,
			    void *dst, u32 dst_len)
{
	u32 ite;
	bool found;
	struct vmm_devemu_vcpu_context *ev;
	struct vmm_emudev *edev;
	struct vmm_region *reg;

	if (!vcpu || !(vcpu->guest)) {
		return VMM_EFAIL;
	}

	ev = vcpu->devemu_priv;
	found = FALSE;
	for (ite = 0; ite < CONFIG_VGPA2REG_CACHE_SIZE; ite++) {
		if (ev->rd_reg[ite] && 
		    (ev->rd_gstart[ite] <= gphys_addr) &&
		    (gphys_addr < ev->rd_gend[ite])) {
			reg = ev->rd_reg[ite];
			found = TRUE;
			break;
		}
	}

	if (!found) {
		reg = vmm_guest_find_region(vcpu->guest, gphys_addr, FALSE);
		if (!reg || !(reg->flags & VMM_REGION_VIRTUAL)) {
			return VMM_EFAIL;
		}
		ev->rd_gstart[ev->rd_victim] = reg->gphys_addr;
		ev->rd_gend[ev->rd_victim] = reg->gphys_addr + reg->phys_size;
		ev->rd_reg[ev->rd_victim] = reg;
		if (ev->rd_victim == (CONFIG_VGPA2REG_CACHE_SIZE - 1)) {
			ev->rd_victim = 0;
		} else {
			ev->rd_victim++;
		}
	}

	edev = (struct vmm_emudev *)reg->devemu_priv;
	if (!edev || !edev->read) {
		return VMM_EFAIL;
	}

	return edev->read(edev, gphys_addr - reg->gphys_addr, dst, dst_len);
}

int vmm_devemu_emulate_write(struct vmm_vcpu *vcpu, 
			     physical_addr_t gphys_addr,
			     void *src, u32 src_len)
{
	u32 ite;
	bool found;
	struct vmm_devemu_vcpu_context *ev;
	struct vmm_emudev *edev;
	struct vmm_region *reg;

	if (!vcpu || !(vcpu->guest)) {
		return VMM_EFAIL;
	}

	ev = vcpu->devemu_priv;
	found = FALSE;
	for (ite = 0; ite < CONFIG_VGPA2REG_CACHE_SIZE; ite++) {
		if (ev->wr_reg[ite] && 
		    (ev->wr_gstart[ite] <= gphys_addr) &&
		    (gphys_addr < ev->wr_gend[ite])) {
			reg = ev->wr_reg[ite];
			found = TRUE;
			break;
		}
	}

	if (!found) {
		reg = vmm_guest_find_region(vcpu->guest, gphys_addr, FALSE);
		if (!reg || !(reg->flags & VMM_REGION_VIRTUAL)) {
			return VMM_EFAIL;
		}
		ev->wr_gstart[ev->wr_victim] = reg->gphys_addr;
		ev->wr_gend[ev->wr_victim] = gphys_addr + reg->phys_size;
		ev->wr_reg[ev->wr_victim] = reg;
		if (ev->wr_victim == (CONFIG_VGPA2REG_CACHE_SIZE - 1)) {
			ev->wr_victim = 0;
		} else {
			ev->wr_victim++;
		}
	}

	edev = (struct vmm_emudev *)reg->devemu_priv;
	if (!edev || !edev->write) {
		return VMM_EFAIL;
	}

	return edev->write(edev, gphys_addr - reg->gphys_addr, src, src_len);
}

int __vmm_devemu_emulate_irq(struct vmm_guest *guest, u32 irq_num, int cpu, int irq_level)
{
	int rc;
	struct dlist *l;
	struct vmm_emupic *ep;
	struct vmm_devemu_guest_context *eg;

	if (!guest) {
		return VMM_EFAIL;
	}

	eg = (struct vmm_devemu_guest_context *)guest->aspace.devemu_priv;

	list_for_each(l, &eg->emupic_list) {
		ep = list_entry(l, struct vmm_emupic, head);
		rc = ep->handle(ep, irq_num, cpu, irq_level);
		if (rc == VMM_EMUPIC_IRQ_HANDLED) {
			break;
		}
	}

	return VMM_OK;
}

static vmm_irq_return_t vmm_devemu_handle_h2g_irq(u32 irq_no, 
						  arch_regs_t *regs, 
						  void *dev)
{
	struct vmm_devemu_h2g_irq *irq = dev;

	if (irq) {
		vmm_host_irq_disable(irq->host_irq);
		vmm_devemu_emulate_irq(irq->guest, irq->guest_irq, 1);
	}

	return VMM_IRQ_HANDLED;
}

int vmm_devemu_complete_h2g_irq(struct vmm_guest *guest, u32 irq_num)
{
	u32 i;
	struct vmm_devemu_guest_context *eg;

	if (!guest) {
		return VMM_EFAIL;
	}

	eg = (struct vmm_devemu_guest_context *)guest->aspace.devemu_priv;

	/* Check if a host IRQ to guest IRQ ended */
	if (eg->h2g_irq) {
		for (i = 0; i < eg->h2g_irq_count; i++) {
			if (irq_num == eg->h2g_irq[i].guest_irq) {
				vmm_devemu_emulate_irq(eg->h2g_irq[i].guest, 
						       eg->h2g_irq[i].guest_irq, 
						       0);
				vmm_host_irq_enable(eg->h2g_irq[i].host_irq);
				break;
			}
		}
	}

	return VMM_OK;
}

int vmm_devemu_register_pic(struct vmm_guest *guest, 
			    struct vmm_emupic *pic)
{
	bool found, added;
	struct dlist *l;
	struct vmm_emupic *ep;
	struct vmm_devemu_guest_context *eg;

	/* Sanity checks */
	if (!guest || !pic) {
		return VMM_EFAIL;
	}

	/* Ensure pic has unique name within a guest */
	eg = (struct vmm_devemu_guest_context *)guest->aspace.devemu_priv;
	ep = NULL;
	found = FALSE;
	list_for_each(l, &eg->emupic_list) {
		ep = list_entry(l, struct vmm_emupic, head);
		if (strcmp(ep->name, pic->name) == 0) {
			found = TRUE;
			break;
		}
	}
	if (found) {
		return VMM_EINVALID;
	}

	/* Initialize pic list head */
	INIT_LIST_HEAD(&pic->head);

	/* Add pic such that pic list is sorted by pic type */
	ep = NULL;
	added = FALSE;
	list_for_each(l, &eg->emupic_list) {
		ep = list_entry(l, struct vmm_emupic, head);
		if (pic->type < ep->type) {
			list_add_tail(&pic->head, &ep->head);
			added = TRUE;
			break;
		}
	}
	if (!added) {
		list_add_tail(&pic->head, &eg->emupic_list);
	}

	return VMM_OK;
}

int vmm_devemu_unregister_pic(struct vmm_guest *guest, 
			      struct vmm_emupic *pic)
{
	bool found;
	struct dlist *l;
	struct vmm_emupic *ep;
	struct vmm_devemu_guest_context *eg;

	if (!guest || !pic) {
		return VMM_EFAIL;
	}

	eg = (struct vmm_devemu_guest_context *)guest->aspace.devemu_priv;

	if (list_empty(&eg->emupic_list)) {
		return VMM_EFAIL;
	}

	ep = NULL;
	found = FALSE;
	list_for_each(l, &eg->emupic_list) {
		ep = list_entry(l, struct vmm_emupic, head);
		if (strcmp(ep->name, pic->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return VMM_ENOTAVAIL;
	}

	list_del(&ep->head);

	return VMM_OK;
}

struct vmm_emupic *vmm_devemu_find_pic(struct vmm_guest *guest, 
					const char *name)
{
	bool found;
	struct dlist *l;
	struct vmm_devemu_guest_context *eg;
	struct vmm_emupic *ep;

	if (!guest || !name) {
		return NULL;
	}

	eg = (struct vmm_devemu_guest_context *)guest->aspace.devemu_priv;
	found = FALSE;
	ep = NULL;

	list_for_each(l, &eg->emupic_list) {
		ep = list_entry(l, struct vmm_emupic, head);
		if (strcmp(ep->name, name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return NULL;
	}

	return ep;
}

struct vmm_emupic *vmm_devemu_pic(struct vmm_guest *guest, int index)
{
	bool found;
	struct dlist *l;
	struct vmm_devemu_guest_context *eg;
	struct vmm_emupic *retval;

	if (!guest) {
		return NULL;
	}
	if (index < 0) {
		return NULL;
	}

	eg = (struct vmm_devemu_guest_context *)guest->aspace.devemu_priv;
	retval = NULL;
	found = FALSE;

	list_for_each(l, &eg->emupic_list) {
		retval = list_entry(l, struct vmm_emupic, head);
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	if (!found) {
		return NULL;
	}

	return retval;
}

u32 vmm_devemu_pic_count(struct vmm_guest *guest)
{
	u32 retval = 0;
	struct vmm_devemu_guest_context *eg;
	struct dlist *l;

	if (!guest) {
		return 0;
	}

	eg = (struct vmm_devemu_guest_context *)guest->aspace.devemu_priv;

	list_for_each(l, &eg->emupic_list) {
		retval++;
	}

	return retval;
}

int vmm_devemu_register_emulator(struct vmm_emulator *emu)
{
	bool found;
	struct dlist *l;
	struct vmm_emulator *e;

	if (emu == NULL) {
		return VMM_EFAIL;
	}

	vmm_mutex_lock(&dectrl.emu_lock);

	e = NULL;
	found = FALSE;
	list_for_each(l, &dectrl.emu_list) {
		e = list_entry(l, struct vmm_emulator, head);
		if (strcmp(e->name, emu->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		vmm_mutex_unlock(&dectrl.emu_lock);
		return VMM_EINVALID;
	}

	INIT_LIST_HEAD(&emu->head);

	list_add_tail(&emu->head, &dectrl.emu_list);

	vmm_mutex_unlock(&dectrl.emu_lock);

	return VMM_OK;
}

int vmm_devemu_unregister_emulator(struct vmm_emulator *emu)
{
	bool found;
	struct dlist *l;
	struct vmm_emulator *e;

	vmm_mutex_lock(&dectrl.emu_lock);

	if (emu == NULL || list_empty(&dectrl.emu_list)) {
		vmm_mutex_unlock(&dectrl.emu_lock);
		return VMM_EFAIL;
	}

	e = NULL;
	found = FALSE;
	list_for_each(l, &dectrl.emu_list) {
		e = list_entry(l, struct vmm_emulator, head);
		if (strcmp(e->name, emu->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		vmm_mutex_unlock(&dectrl.emu_lock);
		return VMM_ENOTAVAIL;
	}

	list_del(&e->head);

	vmm_mutex_unlock(&dectrl.emu_lock);

	return VMM_OK;
}

struct vmm_emulator *vmm_devemu_find_emulator(const char *name)
{
	bool found;
	struct dlist *l;
	struct vmm_emulator *emu;

	if (!name) {
		return NULL;
	}

	found = FALSE;
	emu = NULL;

	vmm_mutex_lock(&dectrl.emu_lock);

	list_for_each(l, &dectrl.emu_list) {
		emu = list_entry(l, struct vmm_emulator, head);
		if (strcmp(emu->name, name) == 0) {
			found = TRUE;
			break;
		}
	}

	vmm_mutex_unlock(&dectrl.emu_lock);

	if (!found) {
		return NULL;
	}

	return emu;
}

struct vmm_emulator *vmm_devemu_emulator(int index)
{
	bool found;
	struct dlist *l;
	struct vmm_emulator *retval;

	if (index < 0) {
		return NULL;
	}

	retval = NULL;
	found = FALSE;

	vmm_mutex_lock(&dectrl.emu_lock);

	list_for_each(l, &dectrl.emu_list) {
		retval = list_entry(l, struct vmm_emulator, head);
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	vmm_mutex_unlock(&dectrl.emu_lock);

	if (!found) {
		return NULL;
	}

	return retval;
}

u32 vmm_devemu_emulator_count(void)
{
	u32 retval;
	struct dlist *l;

	retval = 0;

	vmm_mutex_lock(&dectrl.emu_lock);

	list_for_each(l, &dectrl.emu_list) {
		retval++;
	}

	vmm_mutex_unlock(&dectrl.emu_lock);

	return retval;
}

int devemu_device_is_compatible(struct vmm_devtree_node *node, const char *compat)
{
	const char *cp;
	int cplen, l;

	cp = vmm_devtree_attrval(node, VMM_DEVTREE_COMPATIBLE_ATTR_NAME);
	cplen = vmm_devtree_attrlen(node, VMM_DEVTREE_COMPATIBLE_ATTR_NAME);
	if (cp == NULL)
		return 0;
	while (cplen > 0) {
		if (strcmp(cp, compat) == 0)
			return 1;
		l = strlen(cp) + 1;
		cp += l;
		cplen -= l;
	}

	return 0;
}

const struct vmm_emuid *devemu_match_node(const struct vmm_emuid *matches,
					  struct vmm_devtree_node *node)
{
	const char *node_type;

	if (!matches || !node) {
		return NULL;
	}

	node_type = vmm_devtree_attrval(node,
					VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME);
	while (matches->name[0] || matches->type[0] || matches->compatible[0]) {
		int match = 1;
		if (matches->name[0])
			match &= node->name
			    && !strcmp(matches->name, node->name);
		if (matches->type[0])
			match &= node_type
			    && !strcmp(matches->type, node_type);
		if (matches->compatible[0])
			match &= devemu_device_is_compatible(node,
							     matches->
							     compatible);
		if (match)
			return matches;
		matches++;
	}

	return NULL;
}

int vmm_devemu_reset_context(struct vmm_guest *guest)
{
	u32 ite;
	struct dlist *l;
	struct vmm_devemu_guest_context *eg;
	struct vmm_devemu_vcpu_context *ev;
	struct vmm_vcpu *vcpu;

	if (!guest) {
		return VMM_EFAIL;
	}

	eg = (struct vmm_devemu_guest_context *)guest->aspace.devemu_priv;

	if (eg->h2g_irq) {
		for (ite = 0; ite < eg->h2g_irq_count; ite++) {
			vmm_host_irq_enable(eg->h2g_irq[ite].host_irq);
		}
	}

	list_for_each(l, &guest->vcpu_list) {
		vcpu = list_entry(l, struct vmm_vcpu, head);
		if (vcpu->devemu_priv) {
			ev = vcpu->devemu_priv;
			ev->rd_victim = 0;
			ev->wr_victim = 0;
			for (ite = 0; 
			     ite < CONFIG_VGPA2REG_CACHE_SIZE; 
			     ite++) {
				ev->rd_gstart[ite] = 0;
				ev->rd_gend[ite] = 0;
				ev->rd_reg[ite] = NULL;
				ev->wr_gstart[ite] = 0;
				ev->wr_gend[ite] = 0;
				ev->wr_reg[ite] = NULL;
			}
		}
	}

	return VMM_OK;
}

int vmm_devemu_reset_region(struct vmm_guest *guest, struct vmm_region *reg)
{
	struct vmm_emudev *edev;

	if (!guest || !reg) {
		return VMM_EFAIL;
	}

	if (!(reg->flags & VMM_REGION_VIRTUAL)) {
		return VMM_EFAIL;
	}

	edev = (struct vmm_emudev *)reg->devemu_priv;
	if (!edev || !edev->reset) {
		return VMM_EFAIL;
	}

	return edev->reset(edev);
}

int vmm_devemu_probe_region(struct vmm_guest *guest, struct vmm_region *reg)
{
	int rc;
	bool found;
	struct dlist *l1;
	struct vmm_emudev *einst;
	struct vmm_emulator *emu;
	const struct vmm_emuid *matches;
	const struct vmm_emuid *match;

	if (!guest || !reg) {
		return VMM_EFAIL;
	}

	if (!(reg->flags & VMM_REGION_VIRTUAL)) {
		return VMM_EFAIL;
	}

	vmm_mutex_lock(&dectrl.emu_lock);

	found = FALSE;
	list_for_each(l1, &dectrl.emu_list) {
		emu = list_entry(l1, struct vmm_emulator, head);
		matches = emu->match_table;
		match = devemu_match_node(matches, reg->node);
		if (match) {
			found = TRUE;
			einst = vmm_malloc(sizeof(struct vmm_emudev));
			if (einst == NULL) {
				/* FIXME: There is more cleanup to do */
				vmm_mutex_unlock(&dectrl.emu_lock);
				return VMM_EFAIL;
			}
			memset(einst, 0, sizeof(struct vmm_emudev));
			INIT_SPIN_LOCK(&einst->lock);
			einst->node = reg->node;
			einst->probe = emu->probe;
			einst->read = emu->read;
			einst->write = emu->write;
			einst->reset = emu->reset;
			einst->remove = emu->remove;
			einst->priv = NULL;
			reg->devemu_priv = einst;
#if defined(CONFIG_VERBOSE_MODE)
			vmm_printf("Probe edevice %s/%s\n",
				   guest->node->name, reg->node->name);
#endif
			if ((rc = einst->probe(guest, einst, match))) {
				vmm_printf("%s: %s/%s probe error %d\n", 
				__func__, guest->node->name, reg->node->name, rc);
				vmm_free(einst);
				reg->devemu_priv = NULL;
				vmm_mutex_unlock(&dectrl.emu_lock);
				return rc;
			}
			if ((rc = einst->reset(einst))) {
				vmm_printf("%s: %s/%s reset error %d\n", 
				__func__, guest->node->name, reg->node->name, rc);
				vmm_free(einst);
				reg->devemu_priv = NULL;
				vmm_mutex_unlock(&dectrl.emu_lock);
				return rc;
			}
			break;
		}
	}

	vmm_mutex_unlock(&dectrl.emu_lock);

	if (!found) {
		vmm_printf("%s: No compatible emulator found for %s/%s\n", 
		__func__, guest->node->name, reg->node->name);
		return VMM_ENOTAVAIL;
	}

	return VMM_OK;
}

int vmm_devemu_remove_region(struct vmm_guest *guest, struct vmm_region *reg)
{
	int rc;
	struct vmm_emudev *einst;

	if (!guest || !reg) {
		return VMM_EFAIL;
	}

	if (!(reg->flags & VMM_REGION_VIRTUAL)) {
		return VMM_EFAIL;
	}

	if (reg->devemu_priv) {
		einst = reg->devemu_priv;

		if ((rc = einst->remove(einst))) {
			return rc;
		}

		vmm_free(reg->devemu_priv);
		reg->devemu_priv = NULL;
	}

	return VMM_OK;
}

int vmm_devemu_init_context(struct vmm_guest *guest)
{
	int rc = VMM_OK;
	u32 ite;
	struct dlist *l;
	const char *attr;
	struct vmm_devemu_vcpu_context *ev;
	struct vmm_devemu_guest_context *eg;
	struct vmm_vcpu *vcpu;

	if (!guest) {
		rc = VMM_EFAIL;
		goto devemu_init_context_done;
	}
	if (guest->aspace.devemu_priv) {
		rc = VMM_EFAIL;
		goto devemu_init_context_done;
	}

	eg = vmm_malloc(sizeof(struct vmm_devemu_guest_context));
	if (!eg) {
		rc = VMM_EFAIL;
		goto devemu_init_context_done;
	}
	memset(eg, 0, sizeof(struct vmm_devemu_guest_context));
	INIT_LIST_HEAD(&eg->emupic_list);
	guest->aspace.devemu_priv = eg;

	eg->h2g_irq = NULL;
	eg->h2g_irq_count = 0;
	attr = vmm_devtree_attrval(guest->aspace.node, 
				   VMM_DEVTREE_H2GIRQMAP_ATTR_NAME);
	if (attr) {
		eg->h2g_irq_count = vmm_devtree_attrlen(guest->aspace.node, 
					VMM_DEVTREE_H2GIRQMAP_ATTR_NAME) >> 3;
		if (!(eg->h2g_irq_count)) {
			rc = VMM_EFAIL;
			goto devemu_init_context_free;
		}

		eg->h2g_irq = vmm_malloc(sizeof(struct vmm_devemu_h2g_irq) * 
							(eg->h2g_irq_count));
		if (!eg->h2g_irq) {
			rc = VMM_EFAIL;
			goto devemu_init_context_free;
		}
		memset(eg->h2g_irq, 0, sizeof(struct vmm_devemu_h2g_irq) *
							(eg->h2g_irq_count));

		for (ite = 0; ite < eg->h2g_irq_count; ite++) {
			eg->h2g_irq[ite].guest = guest;
			eg->h2g_irq[ite].host_irq = ((u32 *)attr)[2 * ite];
			eg->h2g_irq[ite].guest_irq = ((u32 *)attr)[(2 * ite) + 1];
			rc = vmm_host_irq_register(eg->h2g_irq[ite].host_irq,
						   "devemu_h2g", 
						   vmm_devemu_handle_h2g_irq, 
						   &eg->h2g_irq[ite]);
			if (rc) {
				goto devemu_init_context_free_h2g;
			}
		}
	}

	list_for_each(l, &guest->vcpu_list) {
		vcpu = list_entry(l, struct vmm_vcpu, head);
		if (!vcpu->devemu_priv) {
			ev = vmm_malloc(sizeof(struct vmm_devemu_vcpu_context));
			memset(ev, 0, sizeof(struct vmm_devemu_vcpu_context));
			ev->rd_victim = 0;
			ev->wr_victim = 0;
			for (ite = 0; 
			     ite < CONFIG_VGPA2REG_CACHE_SIZE; 
			     ite++) {
				ev->rd_gstart[ite] = 0;
				ev->rd_gend[ite] = 0;
				ev->rd_reg[ite] = NULL;
				ev->wr_gstart[ite] = 0;
				ev->wr_gend[ite] = 0;
				ev->wr_reg[ite] = NULL;
			}
			vcpu->devemu_priv = ev;
		}
	}

	goto devemu_init_context_done;

devemu_init_context_free_h2g:
	vmm_free(eg->h2g_irq);
devemu_init_context_free:
	vmm_free(eg);
devemu_init_context_done:
	return rc;

}

int vmm_devemu_deinit_context(struct vmm_guest *guest)
{
	int rc = VMM_OK;
	u32 ite;
	struct dlist *l;
	struct vmm_vcpu *vcpu;
	struct vmm_devemu_guest_context *eg;

	if (!guest) {
		return VMM_EFAIL;
	}

	list_for_each(l, &guest->vcpu_list) {
		vcpu = list_entry(l, struct vmm_vcpu, head);
		if (!vcpu->devemu_priv) {
			vmm_free(vcpu->devemu_priv);
			vcpu->devemu_priv = NULL;
		}
	}

	eg = guest->aspace.devemu_priv;

	if (eg->h2g_irq) {
		for (ite = 0; ite < eg->h2g_irq_count; ite++) {
			rc = vmm_host_irq_unregister(eg->h2g_irq[ite].host_irq,
						&eg->h2g_irq[ite]);
			if (rc) {
				break;
			}
		}
		vmm_free(eg->h2g_irq);
		eg->h2g_irq = NULL;
	}

	vmm_free(guest->aspace.devemu_priv);
	guest->aspace.devemu_priv = NULL;

	return rc;
}

int __init vmm_devemu_init(void)
{
	memset(&dectrl, 0, sizeof(dectrl));

	INIT_MUTEX(&dectrl.emu_lock);
	INIT_LIST_HEAD(&dectrl.emu_list);

	return VMM_OK;
}
