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
	u32 rd_mem_victim;
	physical_addr_t rd_mem_gstart[CONFIG_VGPA2REG_CACHE_SIZE];
	physical_size_t rd_mem_gend[CONFIG_VGPA2REG_CACHE_SIZE];
	struct vmm_region *rd_mem_reg[CONFIG_VGPA2REG_CACHE_SIZE];
	u32 wr_mem_victim;
	physical_addr_t wr_mem_gstart[CONFIG_VGPA2REG_CACHE_SIZE];
	physical_size_t wr_mem_gend[CONFIG_VGPA2REG_CACHE_SIZE];
	struct vmm_region *wr_mem_reg[CONFIG_VGPA2REG_CACHE_SIZE];
	u32 rd_io_victim;
	physical_addr_t rd_io_gstart[CONFIG_VGPA2REG_CACHE_SIZE];
	physical_size_t rd_io_gend[CONFIG_VGPA2REG_CACHE_SIZE];
	struct vmm_region *rd_io_reg[CONFIG_VGPA2REG_CACHE_SIZE];
	u32 wr_io_victim;
	physical_addr_t wr_io_gstart[CONFIG_VGPA2REG_CACHE_SIZE];
	physical_size_t wr_io_gend[CONFIG_VGPA2REG_CACHE_SIZE];
	struct vmm_region *wr_io_reg[CONFIG_VGPA2REG_CACHE_SIZE];
};

struct vmm_devemu_guest_irq {
	struct dlist head;
	const char *name;
	void (*handle) (u32 irq, int cpu, int level, void *opaque);
	void *opaque;
};

struct vmm_devemu_host2guest_irq {
	struct vmm_guest *guest;
	u32 host_irq;
	u32 guest_irq;
};

struct vmm_devemu_guest_context {
	u32 g_irq_count;
	struct dlist *g_irq;
	u32 h2g_irq_count;
	struct vmm_devemu_host2guest_irq *h2g_irq;
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
		if (ev->rd_mem_reg[ite] && 
		    (ev->rd_mem_gstart[ite] <= gphys_addr) &&
		    (gphys_addr < ev->rd_mem_gend[ite])) {
			reg = ev->rd_mem_reg[ite];
			found = TRUE;
			break;
		}
	}

	if (!found) {
		reg = vmm_guest_find_region(vcpu->guest, gphys_addr, 
				VMM_REGION_VIRTUAL | VMM_REGION_MEMORY, FALSE);
		if (!reg) {
			return VMM_EFAIL;
		}
		ev->rd_mem_gstart[ev->rd_mem_victim] = reg->gphys_addr;
		ev->rd_mem_gend[ev->rd_mem_victim] = 
					reg->gphys_addr + reg->phys_size;
		ev->rd_mem_reg[ev->rd_mem_victim] = reg;
		if (ev->rd_mem_victim == (CONFIG_VGPA2REG_CACHE_SIZE - 1)) {
			ev->rd_mem_victim = 0;
		} else {
			ev->rd_mem_victim++;
		}
	}

	edev = (struct vmm_emudev *)reg->devemu_priv;
	if (!edev || !edev->emu->read) {
		return VMM_EFAIL;
	}

	return edev->emu->read(edev, gphys_addr - reg->gphys_addr, 
				dst, dst_len);
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
		if (ev->wr_mem_reg[ite] && 
		    (ev->wr_mem_gstart[ite] <= gphys_addr) &&
		    (gphys_addr < ev->wr_mem_gend[ite])) {
			reg = ev->wr_mem_reg[ite];
			found = TRUE;
			break;
		}
	}

	if (!found) {
		reg = vmm_guest_find_region(vcpu->guest, gphys_addr, 
				VMM_REGION_VIRTUAL | VMM_REGION_MEMORY, FALSE);
		if (!reg) {
			return VMM_EFAIL;
		}
		ev->wr_mem_gstart[ev->wr_mem_victim] = reg->gphys_addr;
		ev->wr_mem_gend[ev->wr_mem_victim] = 
					reg->gphys_addr + reg->phys_size;
		ev->wr_mem_reg[ev->wr_mem_victim] = reg;
		if (ev->wr_mem_victim == (CONFIG_VGPA2REG_CACHE_SIZE - 1)) {
			ev->wr_mem_victim = 0;
		} else {
			ev->wr_mem_victim++;
		}
	}

	edev = (struct vmm_emudev *)reg->devemu_priv;
	if (!edev || !edev->emu->write) {
		return VMM_EFAIL;
	}

	return edev->emu->write(edev, gphys_addr - reg->gphys_addr, 
				src, src_len);
}

int vmm_devemu_emulate_ioread(struct vmm_vcpu *vcpu, 
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
		if (ev->rd_io_reg[ite] && 
		    (ev->rd_io_gstart[ite] <= gphys_addr) &&
		    (gphys_addr < ev->rd_io_gend[ite])) {
			reg = ev->rd_io_reg[ite];
			found = TRUE;
			break;
		}
	}

	if (!found) {
		reg = vmm_guest_find_region(vcpu->guest, gphys_addr, 
				VMM_REGION_VIRTUAL | VMM_REGION_IO, FALSE);
		if (!reg) {
			return VMM_EFAIL;
		}
		ev->rd_io_gstart[ev->rd_io_victim] = reg->gphys_addr;
		ev->rd_io_gend[ev->rd_io_victim] = 
					reg->gphys_addr + reg->phys_size;
		ev->rd_io_reg[ev->rd_io_victim] = reg;
		if (ev->rd_io_victim == (CONFIG_VGPA2REG_CACHE_SIZE - 1)) {
			ev->rd_io_victim = 0;
		} else {
			ev->rd_io_victim++;
		}
	}

	edev = (struct vmm_emudev *)reg->devemu_priv;
	if (!edev || !edev->emu->read) {
		return VMM_EFAIL;
	}

	return edev->emu->read(edev, gphys_addr - reg->gphys_addr, 
				dst, dst_len);
}

int vmm_devemu_emulate_iowrite(struct vmm_vcpu *vcpu, 
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
		if (ev->wr_io_reg[ite] && 
		    (ev->wr_io_gstart[ite] <= gphys_addr) &&
		    (gphys_addr < ev->wr_io_gend[ite])) {
			reg = ev->wr_io_reg[ite];
			found = TRUE;
			break;
		}
	}

	if (!found) {
		reg = vmm_guest_find_region(vcpu->guest, gphys_addr, 
				VMM_REGION_VIRTUAL | VMM_REGION_IO, FALSE);
		if (!reg) {
			return VMM_EFAIL;
		}
		ev->wr_io_gstart[ev->wr_io_victim] = reg->gphys_addr;
		ev->wr_io_gend[ev->wr_io_victim] = 
					reg->gphys_addr + reg->phys_size;
		ev->wr_io_reg[ev->wr_io_victim] = reg;
		if (ev->wr_io_victim == (CONFIG_VGPA2REG_CACHE_SIZE - 1)) {
			ev->wr_io_victim = 0;
		} else {
			ev->wr_io_victim++;
		}
	}

	edev = (struct vmm_emudev *)reg->devemu_priv;
	if (!edev || !edev->emu->write) {
		return VMM_EFAIL;
	}

	return edev->emu->write(edev, gphys_addr - reg->gphys_addr, 
				src, src_len);
}

int __vmm_devemu_emulate_irq(struct vmm_guest *guest, u32 irq, int cpu, int level)
{
	struct dlist *l;
	struct vmm_devemu_guest_irq *gi;
	struct vmm_devemu_guest_context *eg;

	if (!guest) {
		return VMM_EFAIL;
	}

	eg = (struct vmm_devemu_guest_context *)guest->aspace.devemu_priv;

	if (eg->g_irq_count <= irq) {
		return VMM_EINVALID;
	}

	list_for_each(l, &eg->g_irq[irq]) {
		gi = list_entry(l, struct vmm_devemu_guest_irq, head);
		gi->handle(irq, cpu, level, gi->opaque);
	}

	return VMM_OK;
}

static vmm_irq_return_t vmm_devemu_handle_h2g_irq(int hirq, void *dev)
{
	struct vmm_devemu_host2guest_irq *irq = dev;

	if (irq) {
		vmm_host_irq_disable(irq->host_irq);
		vmm_devemu_emulate_irq(irq->guest, irq->guest_irq, 1);
	}

	return VMM_IRQ_HANDLED;
}

int vmm_devemu_complete_host2guest_irq(struct vmm_guest *guest, u32 irq)
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
			if (irq == eg->h2g_irq[i].guest_irq) {
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

int vmm_devemu_register_irq_handler(struct vmm_guest *guest, u32 irq,
				    const char *name, 
				    void (*handle) (u32 irq, int cpu, int level, void *opaque),
				    void *opaque)
{
	bool found;
	struct dlist *l;
	struct vmm_devemu_guest_irq *gi;
	struct vmm_devemu_guest_context *eg;

	/* Sanity checks */
	if (!guest || !handle) {
		return VMM_EFAIL;
	}

	eg = (struct vmm_devemu_guest_context *)guest->aspace.devemu_priv;

	/* Sanity checks */
	if (eg->g_irq_count <= irq) {
		return VMM_EINVALID;
	}

	/* Check if handler is not already registered */
	gi = NULL;
	found = FALSE;
	list_for_each(l, &eg->g_irq[irq]) {
		gi = list_entry(l, struct vmm_devemu_guest_irq, head);
		if (gi->handle == handle && gi->opaque == opaque) {
			found = TRUE;
			break;
		}
	}
	if (found) {
		return VMM_EEXIST;
	}

	/* Alloc guest irq */
	gi = vmm_zalloc(sizeof(struct vmm_devemu_guest_irq));
	if (!gi) {
		return VMM_ENOMEM;
	}

	/* Initialize guest irq */
	INIT_LIST_HEAD(&gi->head);
	gi->name = name;
	gi->handle = handle;
	gi->opaque = opaque;

	/* Add guest irq to list */
	list_add_tail(&gi->head, &eg->g_irq[irq]);

	return VMM_OK;
}

int vmm_devemu_unregister_irq_handler(struct vmm_guest *guest, u32 irq,
				      void (*handle) (u32 irq, int cpu, int level, void *opaque),
				      void *opaque)
{
	bool found;
	struct dlist *l;
	struct vmm_devemu_guest_irq *gi;
	struct vmm_devemu_guest_context *eg;

	/* Sanity checks */
	if (!guest || !handle) {
		return VMM_EFAIL;
	}

	eg = (struct vmm_devemu_guest_context *)guest->aspace.devemu_priv;

	/* Check if handler is not already unregistered */
	gi = NULL;
	found = FALSE;
	list_for_each(l, &eg->g_irq[irq]) {
		gi = list_entry(l, struct vmm_devemu_guest_irq, head);
		if (gi->handle == handle && gi->opaque == opaque) {
			found = TRUE;
			break;
		}
	}
	if (!found) {
		return VMM_ENOTAVAIL;
	}

	/* Remove from list and free guest irq */
	list_del(&gi->head);
	vmm_free(gi);

	return VMM_OK;
}

u32 vmm_devemu_count_irqs(struct vmm_guest *guest)
{
	struct vmm_devemu_guest_context *eg;

	if (!guest) {
		return 0;
	}

	eg = (struct vmm_devemu_guest_context *)guest->aspace.devemu_priv;

	return (eg) ? eg->g_irq_count : 0;
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
			ev->rd_mem_victim = 0;
			ev->wr_mem_victim = 0;
			ev->rd_io_victim = 0;
			ev->wr_io_victim = 0;
			for (ite = 0; 
			     ite < CONFIG_VGPA2REG_CACHE_SIZE; 
			     ite++) {
				ev->rd_mem_gstart[ite] = 0;
				ev->rd_mem_gend[ite] = 0;
				ev->rd_mem_reg[ite] = NULL;
				ev->wr_mem_gstart[ite] = 0;
				ev->wr_mem_gend[ite] = 0;
				ev->wr_mem_reg[ite] = NULL;
				ev->rd_io_gstart[ite] = 0;
				ev->rd_io_gend[ite] = 0;
				ev->rd_io_reg[ite] = NULL;
				ev->wr_io_gstart[ite] = 0;
				ev->wr_io_gend[ite] = 0;
				ev->wr_io_reg[ite] = NULL;
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
	if (!edev || !edev->emu->reset) {
		return VMM_EFAIL;
	}

	return edev->emu->reset(edev);
}

int vmm_devemu_probe_region(struct vmm_guest *guest, struct vmm_region *reg)
{
	int rc;
	bool found;
	struct dlist *l1;
	struct vmm_emudev *einst;
	struct vmm_emulator *emu;
	const struct vmm_devtree_nodeid *match;

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
		match = vmm_devtree_match_node(emu->match_table, reg->node);
		if (match) {
			found = TRUE;
			einst = vmm_zalloc(sizeof(struct vmm_emudev));
			if (einst == NULL) {
				/* FIXME: There is more cleanup to do */
				vmm_mutex_unlock(&dectrl.emu_lock);
				return VMM_EFAIL;
			}
			INIT_SPIN_LOCK(&einst->lock);
			einst->node = reg->node;
			einst->emu = emu;
			einst->priv = NULL;
			reg->devemu_priv = einst;
#if defined(CONFIG_VERBOSE_MODE)
			vmm_printf("Probe edevice %s/%s\n",
				   guest->node->name, reg->node->name);
#endif
			if ((rc = emu->probe(guest, einst, match))) {
				vmm_printf("%s: %s/%s probe error %d\n", 
				__func__, guest->node->name, reg->node->name, rc);
				vmm_free(einst);
				reg->devemu_priv = NULL;
				vmm_mutex_unlock(&dectrl.emu_lock);
				return rc;
			}
			if ((rc = emu->reset(einst))) {
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

		if ((rc = einst->emu->remove(einst))) {
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

	eg = vmm_zalloc(sizeof(struct vmm_devemu_guest_context));
	if (!eg) {
		rc = VMM_EFAIL;
		goto devemu_init_context_done;
	}

	eg->g_irq = NULL;
	eg->g_irq_count = 0;
	attr = vmm_devtree_attrval(guest->aspace.node, 
				   VMM_DEVTREE_GUESTIRQCNT_ATTR_NAME);
	if (attr) {
		eg->g_irq_count = *((u32 *)attr);
		eg->g_irq = vmm_zalloc(sizeof(struct dlist) * eg->g_irq_count);
		if (!eg->g_irq) {
			rc = VMM_ENOMEM;
			goto devemu_init_context_free;
		}
		for (ite = 0; ite < eg->g_irq_count; ite++) {
			INIT_LIST_HEAD(&eg->g_irq[ite]);
		}
	} else {
		rc = VMM_EFAIL;
		goto devemu_init_context_free;
	}

	eg->h2g_irq = NULL;
	eg->h2g_irq_count = 0;
	attr = vmm_devtree_attrval(guest->aspace.node, 
				   VMM_DEVTREE_H2GIRQMAP_ATTR_NAME);
	if (attr) {
		eg->h2g_irq_count = vmm_devtree_attrlen(guest->aspace.node, 
					VMM_DEVTREE_H2GIRQMAP_ATTR_NAME) >> 3;
		if (!(eg->h2g_irq_count)) {
			rc = VMM_EFAIL;
			goto devemu_init_context_free_g;
		}

		eg->h2g_irq = 
			vmm_zalloc(sizeof(struct vmm_devemu_host2guest_irq) * 
							(eg->h2g_irq_count));
		if (!eg->h2g_irq) {
			rc = VMM_EFAIL;
			goto devemu_init_context_free_g;
		}

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
			ev = vmm_zalloc(sizeof(struct vmm_devemu_vcpu_context));
			ev->rd_mem_victim = 0;
			ev->wr_mem_victim = 0;
			ev->rd_io_victim = 0;
			ev->wr_io_victim = 0;
			for (ite = 0; 
			     ite < CONFIG_VGPA2REG_CACHE_SIZE; 
			     ite++) {
				ev->rd_mem_gstart[ite] = 0;
				ev->rd_mem_gend[ite] = 0;
				ev->rd_mem_reg[ite] = NULL;
				ev->wr_mem_gstart[ite] = 0;
				ev->wr_mem_gend[ite] = 0;
				ev->wr_mem_reg[ite] = NULL;
				ev->rd_io_gstart[ite] = 0;
				ev->rd_io_gend[ite] = 0;
				ev->rd_io_reg[ite] = NULL;
				ev->wr_io_gstart[ite] = 0;
				ev->wr_io_gend[ite] = 0;
				ev->wr_io_reg[ite] = NULL;
			}
			vcpu->devemu_priv = ev;
		}
	}

	guest->aspace.devemu_priv = eg;

	goto devemu_init_context_done;

devemu_init_context_free_h2g:
	vmm_free(eg->h2g_irq);
devemu_init_context_free_g:
	vmm_free(eg->g_irq);
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
		if (vcpu->devemu_priv) {
			vmm_free(vcpu->devemu_priv);
			vcpu->devemu_priv = NULL;
		}
	}

	eg = guest->aspace.devemu_priv;
	guest->aspace.devemu_priv = NULL;

	if (eg->g_irq) {
		vmm_free(eg->g_irq);
		eg->g_irq = NULL;
		eg->g_irq_count = 0;
	}

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
		eg->h2g_irq_count = 0;
	}

	vmm_free(eg);

	return rc;
}

int __init vmm_devemu_init(void)
{
	memset(&dectrl, 0, sizeof(dectrl));

	INIT_MUTEX(&dectrl.emu_lock);
	INIT_LIST_HEAD(&dectrl.emu_list);

	return VMM_OK;
}
