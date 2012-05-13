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
 * @file vmm_manager.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for hypervisor manager
 */

#include <arch_vcpu.h>
#include <arch_guest.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_guest_aspace.h>
#include <vmm_vcpu_irq.h>
#include <vmm_scheduler.h>
#include <vmm_manager.h>

/** Control structure for Scheduler */
struct vmm_manager_ctrl {
	vmm_spinlock_t lock;
	u32 vcpu_count;
	u32 guest_count;
	struct vmm_vcpu vcpu_array[CONFIG_MAX_VCPU_COUNT];
	bool vcpu_avail_array[CONFIG_MAX_VCPU_COUNT];
	struct vmm_guest guest_array[CONFIG_MAX_GUEST_COUNT];
	bool guest_avail_array[CONFIG_MAX_GUEST_COUNT];
	struct dlist orphan_vcpu_list;
	struct dlist guest_list;
};

static struct vmm_manager_ctrl mngr;

u32 vmm_manager_max_vcpu_count(void)
{
	return CONFIG_MAX_VCPU_COUNT;
}

u32 vmm_manager_vcpu_count(void)
{
	return mngr.vcpu_count;
}

struct vmm_vcpu * vmm_manager_vcpu(u32 vcpu_id)
{
	if (vcpu_id < CONFIG_MAX_VCPU_COUNT) {
		if (!mngr.vcpu_avail_array[vcpu_id]) {
			return &mngr.vcpu_array[vcpu_id];
		}
	}
	return NULL;
}

static int vmm_manager_vcpu_state_change(struct vmm_vcpu *vcpu, u32 new_state)
{
	int rc = VMM_EFAIL;
	irq_flags_t flags;

	if(!vcpu) {
		return rc;
	}

	vmm_spin_lock_irqsave(&vcpu->lock, flags);
	switch(new_state) {
	case VMM_VCPU_STATE_RESET:
		if ((vcpu->state != VMM_VCPU_STATE_RESET) &&
		    (vcpu->state != VMM_VCPU_STATE_UNKNOWN)) {
			rc = vmm_scheduler_notify_state_change(vcpu, new_state);
			if (rc) {
				break;
			}
			vcpu->state = VMM_VCPU_STATE_RESET;
			vcpu->reset_count++;
			if ((rc = arch_vcpu_init(vcpu))) {
				break;
			}
			if ((rc = vmm_vcpu_irq_init(vcpu))) {
				break;
			}
		}
		break;
	case VMM_VCPU_STATE_READY:
		if ((vcpu->state == VMM_VCPU_STATE_RESET) ||
		    (vcpu->state == VMM_VCPU_STATE_PAUSED)) {
			rc = vmm_scheduler_notify_state_change(vcpu, new_state);
			vcpu->state = VMM_VCPU_STATE_READY;
		}
		break;
	case VMM_VCPU_STATE_PAUSED:
		if ((vcpu->state == VMM_VCPU_STATE_READY) ||
		    (vcpu->state == VMM_VCPU_STATE_RUNNING)) {
			rc = vmm_scheduler_notify_state_change(vcpu, new_state);
			vcpu->state = VMM_VCPU_STATE_PAUSED;
		}
		break;
	case VMM_VCPU_STATE_HALTED:
		if ((vcpu->state == VMM_VCPU_STATE_READY) ||
		    (vcpu->state == VMM_VCPU_STATE_RUNNING)) {
			rc = vmm_scheduler_notify_state_change(vcpu, new_state);
			vcpu->state = VMM_VCPU_STATE_HALTED;
		}
		break;
	};
	vmm_spin_unlock_irqrestore(&vcpu->lock, flags);
	return rc;
 }

int vmm_manager_vcpu_reset(struct vmm_vcpu * vcpu)
{
	return vmm_manager_vcpu_state_change(vcpu, VMM_VCPU_STATE_RESET);
}

int vmm_manager_vcpu_kick(struct vmm_vcpu * vcpu)
{
	return vmm_manager_vcpu_state_change(vcpu, VMM_VCPU_STATE_READY);
}

int vmm_manager_vcpu_pause(struct vmm_vcpu * vcpu)
{
	return vmm_manager_vcpu_state_change(vcpu, VMM_VCPU_STATE_PAUSED);
}

int vmm_manager_vcpu_resume(struct vmm_vcpu * vcpu)
{
	return vmm_manager_vcpu_state_change(vcpu, VMM_VCPU_STATE_READY);
}

int vmm_manager_vcpu_halt(struct vmm_vcpu * vcpu)
{
	return vmm_manager_vcpu_state_change(vcpu, VMM_VCPU_STATE_HALTED);
}

int vmm_manager_vcpu_dumpreg(struct vmm_vcpu * vcpu)
{
	int rc = VMM_EFAIL;
	irq_flags_t flags;
	if (vcpu) {
		vmm_spin_lock_irqsave(&vcpu->lock, flags);
		if (vcpu->state != VMM_VCPU_STATE_RUNNING) {
			arch_vcpu_regs_dump(vcpu);
			rc = VMM_OK;
		}
		vmm_spin_unlock_irqrestore(&vcpu->lock, flags);
	}
	return rc;
}

int vmm_manager_vcpu_dumpstat(struct vmm_vcpu * vcpu)
{
	int rc = VMM_EFAIL;
	irq_flags_t flags;
	if (vcpu) {
		vmm_spin_lock_irqsave(&vcpu->lock, flags);
		if (vcpu->state != VMM_VCPU_STATE_RUNNING) {
			arch_vcpu_stat_dump(vcpu);
			rc = VMM_OK;
		}
		vmm_spin_unlock_irqrestore(&vcpu->lock, flags);
	}
	return rc;
}

struct vmm_vcpu * vmm_manager_vcpu_orphan_create(const char *name,
					    virtual_addr_t start_pc,
					    virtual_addr_t start_sp,
					    u8 priority,
					    u64 time_slice_nsecs)
{
	int found, vnum;
	struct vmm_vcpu * vcpu;
	irq_flags_t flags;

	/* Sanity checks */
	if (name == NULL || start_pc == 0 || time_slice_nsecs == 0) {
		return NULL;
	}
	if (VMM_VCPU_MAX_PRIORITY < priority) {
		priority = VMM_VCPU_MAX_PRIORITY;
	}

	/* Acquire lock */
	vmm_spin_lock_irqsave(&mngr.lock, flags);

	/* Find the next available vcpu */
	found = 0;
	for (vnum = 0; vnum < CONFIG_MAX_VCPU_COUNT; vnum++) {
		if (mngr.vcpu_avail_array[vnum]) {
			found = 1;
			break;
		}
	}
	if (found) {
		vcpu = &mngr.vcpu_array[vnum];
		mngr.vcpu_avail_array[vnum] = FALSE;
	} else {
		vmm_spin_unlock_irqrestore(&mngr.lock, flags);
		return NULL;
	}

	/* Update vcpu attributes */
	INIT_SPIN_LOCK(&vcpu->lock);
	INIT_LIST_HEAD(&vcpu->head);
	vcpu->subid = 0;
	vmm_strcpy(vcpu->name, name);
	vcpu->node = NULL;
	vcpu->is_normal = FALSE;
	vcpu->state = VMM_VCPU_STATE_UNKNOWN;
	vcpu->reset_count = 0;
	vcpu->preempt_count = 0;
	vcpu->priority = priority;
	vcpu->time_slice = time_slice_nsecs;
	vcpu->start_pc = start_pc;
	vcpu->start_sp = start_sp;
	vcpu->guest = NULL;
	vcpu->arch_priv = NULL;

	/* Initialize VCPU */
	if (arch_vcpu_init(vcpu)) {
		mngr.vcpu_avail_array[vnum] = TRUE;
		vmm_spin_unlock_irqrestore(&mngr.lock, flags);
		return NULL;
	}

	/* Notify scheduler about new VCPU */
	vcpu->sched_priv = NULL;
	if (vmm_scheduler_notify_state_change(vcpu, 
					VMM_VCPU_STATE_RESET)) {
		mngr.vcpu_avail_array[vnum] = TRUE;
		vmm_spin_unlock_irqrestore(&mngr.lock, flags);
		return NULL;
	}
	vcpu->state = VMM_VCPU_STATE_RESET;

	/* Set wait queue context to NULL */
	INIT_LIST_HEAD(&vcpu->wq_head);
	vcpu->wq_priv = NULL;

	/* Set device emulation context to NULL */
	vcpu->devemu_priv = NULL;

	/* Add VCPU to orphan list */
	list_add_tail(&mngr.orphan_vcpu_list, &vcpu->head);

	/* Increment vcpu count */
	mngr.vcpu_count++;

	/* Release lock */
	vmm_spin_unlock_irqrestore(&mngr.lock, flags);

	return vcpu;
}

int vmm_manager_vcpu_orphan_destroy(struct vmm_vcpu * vcpu)
{
	int rc = VMM_OK;
	irq_flags_t flags;

	/* Sanity checks */
	if (!vcpu) {
		return VMM_EFAIL;
	}
	if (vcpu->is_normal || vcpu->wq_priv) {
		return VMM_EFAIL;
	}

	/* Reset the VCPU */
	if ((rc = vmm_manager_vcpu_state_change(vcpu, VMM_VCPU_STATE_RESET))) {
		return rc;
	}

	/* Acquire lock */
	vmm_spin_lock_irqsave(&mngr.lock, flags);

	/* Decrement vcpu count */
	mngr.vcpu_count--;

	/* Remove VCPU from orphan list */
	list_del(&vcpu->head);

	/* Notify scheduler about VCPU state change */
	if ((rc = vmm_scheduler_notify_state_change(vcpu, 
					VMM_VCPU_STATE_UNKNOWN))) {
		vmm_spin_unlock_irqrestore(&mngr.lock, flags);
		return rc;
	}
	vcpu->sched_priv = NULL;

	/* Change VCPU state */
	vcpu->state = VMM_VCPU_STATE_UNKNOWN;

	/* Deinit VCPU */
	if ((rc = arch_vcpu_deinit(vcpu))) {
		vmm_spin_unlock_irqrestore(&mngr.lock, flags);
		return rc;
	}

	/* Mark VCPU as available */
	mngr.vcpu_avail_array[vcpu->id] = TRUE;

	/* Release lock */
	vmm_spin_unlock_irqrestore(&mngr.lock, flags);

	return VMM_OK;
}

u32 vmm_manager_max_guest_count(void)
{
	return CONFIG_MAX_GUEST_COUNT;
}

u32 vmm_manager_guest_count(void)
{
	return mngr.guest_count;
}

struct vmm_guest * vmm_manager_guest(u32 guest_id)
{
	if (guest_id < CONFIG_MAX_GUEST_COUNT) {
		if (!mngr.guest_avail_array[guest_id]) {
			return &mngr.guest_array[guest_id];
		}
	}
	return NULL;
}

u32 vmm_manager_guest_vcpu_count(struct vmm_guest *guest)
{
	if (!guest) {
		return 0;
	}

	return guest->vcpu_count;
}

struct vmm_vcpu * vmm_manager_guest_vcpu(struct vmm_guest *guest, u32 subid)
{
	bool found = FALSE;
	struct vmm_vcpu *vcpu = NULL;
	struct dlist *l;

	if (!guest) {
		return NULL;
	}

	list_for_each(l, &guest->vcpu_list) {
		vcpu = list_entry(l, struct vmm_vcpu, head);
		if (vcpu->subid == subid) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return NULL;
	}

	return vcpu;
}

int vmm_manager_guest_reset(struct vmm_guest * guest)
{
	int rc = VMM_EFAIL;
	struct dlist *lentry;
	struct vmm_vcpu *vcpu;
	if (guest) {
		rc = VMM_OK;
		list_for_each(lentry, &guest->vcpu_list) {
			vcpu = list_entry(lentry, struct vmm_vcpu, head);
			if ((rc = vmm_manager_vcpu_reset(vcpu))) {
				return rc;
			}
		}
		if ((rc = vmm_guest_aspace_reset(guest))) {
			return rc;
		}
		guest->reset_count++;
		if ((rc = arch_guest_init(guest))) {
			return rc;
		}
	}
	return rc;
}

int vmm_manager_guest_kick(struct vmm_guest * guest)
{
	int rc = VMM_EFAIL;
	struct dlist *lentry;
	struct vmm_vcpu *vcpu;
	if (guest) {
		rc = VMM_OK;
		list_for_each(lentry, &guest->vcpu_list) {
			vcpu = list_entry(lentry, struct vmm_vcpu, head);
			if ((rc = vmm_manager_vcpu_kick(vcpu))) {
				break;
			}
		}
	}
	return rc;
}

int vmm_manager_guest_pause(struct vmm_guest * guest)
{
	int rc = VMM_EFAIL;
	struct dlist *lentry;
	struct vmm_vcpu *vcpu;
	if (guest) {
		rc = VMM_OK;
		list_for_each(lentry, &guest->vcpu_list) {
			vcpu = list_entry(lentry, struct vmm_vcpu, head);
			if ((rc = vmm_manager_vcpu_pause(vcpu))) {
				break;
			}
		}
	}
	return rc;
}

int vmm_manager_guest_resume(struct vmm_guest * guest)
{
	int rc = VMM_EFAIL;
	struct dlist *lentry;
	struct vmm_vcpu *vcpu;
	if (guest) {
		rc = VMM_OK;
		list_for_each(lentry, &guest->vcpu_list) {
			vcpu = list_entry(lentry, struct vmm_vcpu, head);
			if ((rc = vmm_manager_vcpu_resume(vcpu))) {
				break;
			}
		}
	}
	return rc;
}

int vmm_manager_guest_halt(struct vmm_guest * guest)
{
	int rc = VMM_EFAIL;
	struct dlist *lentry;
	struct vmm_vcpu *vcpu;
	if (guest) {
		rc = VMM_OK;
		list_for_each(lentry, &guest->vcpu_list) {
			vcpu = list_entry(lentry, struct vmm_vcpu, head);
			if ((rc = vmm_manager_vcpu_halt(vcpu))) {
				break;
			}
		}
	}
	return rc;
}

int vmm_manager_guest_dumpreg(struct vmm_guest * guest)
{
	int rc = VMM_EFAIL;
	struct dlist *lentry;
	struct vmm_vcpu *vcpu;
	if (guest) {
		rc = VMM_OK;
		list_for_each(lentry, &guest->vcpu_list) {
			vcpu = list_entry(lentry, struct vmm_vcpu, head);
			if ((rc = vmm_manager_vcpu_dumpreg(vcpu))) {
				break;
			}
		}
	}
	return rc;
}

struct vmm_guest * vmm_manager_guest_create(struct vmm_devtree_node * gnode)
{
	int vnum, gnum, found;
	const char *attrval;
	irq_flags_t flags;
	struct dlist *l1;
	struct vmm_devtree_node *vsnode;
	struct vmm_devtree_node *vnode;
	struct vmm_guest * guest = NULL;
	struct vmm_vcpu * vcpu = NULL;

	/* Sanity checks */
	if (!gnode) {
		return NULL;
	}
	attrval = vmm_devtree_attrval(gnode,
				      VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME);
	if (!attrval) {
		return NULL;
	}
	if (vmm_strcmp(attrval, VMM_DEVTREE_DEVICE_TYPE_VAL_GUEST) != 0) {
		return NULL;
	}

	/* Acquire lock */
	vmm_spin_lock_irqsave(&mngr.lock, flags);

	/* Ensure guest node uniqueness */
	list_for_each(l1, &mngr.guest_list) {
		guest = list_entry(l1, struct vmm_guest, head);
		if ((guest->node == gnode) ||
		    (vmm_strcmp(guest->node->name, gnode->name) == 0)) {
			vmm_spin_unlock_irqrestore(&mngr.lock, flags);
			return NULL;
		}
	}
	guest = NULL;

	/* Find next available guest instance */
	found = 0;
	for (gnum = 0; gnum < CONFIG_MAX_GUEST_COUNT; gnum++) {
		if (mngr.guest_avail_array[gnum]) {
			found = 1;
			break;
		}
	}
	if (found) {
		guest = &mngr.guest_array[gnum];
		mngr.guest_avail_array[gnum] = FALSE;
	} else {
		vmm_spin_unlock_irqrestore(&mngr.lock, flags);
		return NULL;
	}

	/* Initialize guest instance */
	INIT_SPIN_LOCK(&guest->lock);
	list_add_tail(&mngr.guest_list, &guest->head);
	guest->node = gnode;
	guest->reset_count = 0;
	guest->vcpu_count = 0;
	INIT_LIST_HEAD(&guest->vcpu_list);
	guest->arch_priv = NULL;

	vsnode = vmm_devtree_getchild(gnode, VMM_DEVTREE_VCPUS_NODE_NAME);
	if (!vsnode) {
		return guest;
	}
	list_for_each(l1, &vsnode->child_list) {
		vnode = list_entry(l1, struct vmm_devtree_node, head);

		/* Sanity checks */
		if (CONFIG_MAX_VCPU_COUNT <= mngr.vcpu_count) {
			break;
		}
		attrval = vmm_devtree_attrval(vnode,
					      VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME);
		if (!attrval) {
			continue;
		}
		if (vmm_strcmp(attrval,
			       VMM_DEVTREE_DEVICE_TYPE_VAL_VCPU) != 0) {
			continue;
		}

		/* Find next available vcpu instance */
		found = 0;
		for (vnum = 0; vnum < CONFIG_MAX_VCPU_COUNT; vnum++) {
			if (mngr.vcpu_avail_array[vnum]) {
				found = 1;
				break;
			}
		}
		if (found) {
			vcpu = &mngr.vcpu_array[vnum];
			mngr.vcpu_avail_array[vnum] = FALSE;
		} else {
			break;
		}

		/* Initialize vcpu instance */
		INIT_SPIN_LOCK(&vcpu->lock);
		vcpu->subid = guest->vcpu_count;
		vmm_strcpy(vcpu->name, gnode->name);
		vmm_strcat(vcpu->name,
			   VMM_DEVTREE_PATH_SEPARATOR_STRING);
		vmm_strcat(vcpu->name, vnode->name);
		vcpu->node = vnode;
		vcpu->is_normal = TRUE;
		vcpu->state = VMM_VCPU_STATE_UNKNOWN;
		vcpu->reset_count = 0;
		vcpu->preempt_count = 0;
		attrval = vmm_devtree_attrval(vnode,
					     VMM_DEVTREE_PRIORITY_ATTR_NAME);
		if (attrval) {
			vcpu->priority = *((u32 *) attrval);
			if (VMM_VCPU_MAX_PRIORITY < vcpu->priority) {
				vcpu->priority = VMM_VCPU_MAX_PRIORITY;
			}
		} else {
			vcpu->priority = VMM_VCPU_DEF_PRIORITY;
		}
		attrval = vmm_devtree_attrval(vnode,
					     VMM_DEVTREE_TIME_SLICE_ATTR_NAME);
		if (attrval) {
			vcpu->time_slice = *((u32 *) attrval);
		} else {
			vcpu->time_slice = VMM_VCPU_DEF_TIME_SLICE;
		}
		attrval = vmm_devtree_attrval(vnode,
					      VMM_DEVTREE_START_PC_ATTR_NAME);
		if (attrval) {
			vcpu->start_pc = *((virtual_addr_t *) attrval);
		}
		attrval = vmm_devtree_attrval(vnode,
					      VMM_DEVTREE_START_SP_ATTR_NAME);
		if (attrval) {
			vcpu->start_sp = *((virtual_addr_t *) attrval);
		} else {
			vcpu->start_sp = 0x0;
		}
		vcpu->guest = guest;
		vcpu->arch_priv = NULL;
		if (arch_vcpu_init(vcpu)) {
			continue;
		}
		if (vmm_vcpu_irq_init(vcpu)) {
			continue;
		}

		/* Notify scheduler about new VCPU */
		vcpu->sched_priv = NULL;
		if (vmm_scheduler_notify_state_change(vcpu, 
						VMM_VCPU_STATE_RESET)) {
			mngr.vcpu_avail_array[vnum] = TRUE;
			break;
		}
		vcpu->state = VMM_VCPU_STATE_RESET;

		/* Set wait queue context to NULL */
		INIT_LIST_HEAD(&vcpu->wq_head);
		vcpu->wq_priv = NULL;

		/* Set device emulation context to NULL */
		vcpu->devemu_priv = NULL;

		/* Add VCPU to Guest child list */
		list_add_tail(&guest->vcpu_list, &vcpu->head);

		/* Increment vcpu count */
		mngr.vcpu_count++;
		guest->vcpu_count++;
	}

	/* Initialize guest address space */
	if (vmm_guest_aspace_init(guest)) {
		vmm_spin_unlock_irqrestore(&mngr.lock, flags);
		return NULL;
	}

	/* Reset guest address space */
	if (vmm_guest_aspace_reset(guest)) {
		vmm_spin_unlock_irqrestore(&mngr.lock, flags);
		return NULL;
	}

	/* Initialize arch guest context */
	if (arch_guest_init(guest)) {
		vmm_spin_unlock_irqrestore(&mngr.lock, flags);
		return NULL;
	}

	/* Increment guest count */
	mngr.guest_count++;

	/* Release lock */
	vmm_spin_unlock_irqrestore(&mngr.lock, flags);

	return guest;
}

int vmm_manager_guest_destroy(struct vmm_guest * guest)
{
	int rc;
	irq_flags_t flags;
	struct dlist * l;
	struct vmm_vcpu * vcpu;

	/* Sanity Check */
	if (!guest) {
		return VMM_EFAIL;
	}

	/* For sanity reset guest (ignore reture value) */
	vmm_manager_guest_reset(guest);

	/* Acquire lock */
	vmm_spin_lock_irqsave(&mngr.lock, flags);

	/* Decrement guest count */
	mngr.guest_count--;

	/* Remove from guest list */
	list_del(&guest->head);

	/* Deinit arch guest context */
	if ((rc = arch_guest_deinit(guest))) {
		vmm_spin_unlock_irqrestore(&mngr.lock, flags);
		return rc;
	}

	/* Deinit the guest aspace */
	if ((rc = vmm_guest_aspace_deinit(guest))) {
		vmm_spin_unlock_irqrestore(&mngr.lock, flags);
		return rc;
	}

	/* Destroy each VCPU of guest */
	while (!list_empty(&guest->vcpu_list)) {
		l = list_pop(&guest->vcpu_list);
		vcpu = list_entry(l, struct vmm_vcpu, head);

		/* Decrement vcpu count */
		mngr.vcpu_count--;

		/* Notify scheduler about VCPU state change */
		if ((rc = vmm_scheduler_notify_state_change(vcpu, 
					VMM_VCPU_STATE_UNKNOWN))) {
			vmm_spin_unlock_irqrestore(&mngr.lock, flags);
			return rc;
		}
		vcpu->sched_priv = NULL;

		/* Change VCPU state */
		vcpu->state = VMM_VCPU_STATE_UNKNOWN;

		/* Deinit VCPU */
		if ((rc = arch_vcpu_deinit(vcpu))) {
			vmm_spin_unlock_irqrestore(&mngr.lock, flags);
			return rc;
		}

		/* Mark VCPU as available */
		mngr.vcpu_avail_array[vcpu->id] = TRUE;
	}

	/* Reset guest instance members */
	INIT_LIST_HEAD(&mngr.guest_array[guest->id].head);
	mngr.guest_array[guest->id].node = NULL;
	INIT_LIST_HEAD(&mngr.guest_array[guest->id].vcpu_list);
	mngr.guest_avail_array[guest->id] = TRUE;

	/* Release lock */
	vmm_spin_unlock_irqrestore(&mngr.lock, flags);

	return VMM_OK;
}

int __init vmm_manager_init(void)
{
	u32 vnum, gnum;

	/* Reset the manager control structure */
	vmm_memset(&mngr, 0, sizeof(mngr));

	/* Intialize guest & vcpu managment parameters */
	INIT_SPIN_LOCK(&mngr.lock);
	mngr.vcpu_count = 0;
	mngr.guest_count = 0;
	INIT_LIST_HEAD(&mngr.orphan_vcpu_list);
	INIT_LIST_HEAD(&mngr.guest_list);

	/* Initialze memory for guest instances */
	for (gnum = 0; gnum < CONFIG_MAX_GUEST_COUNT; gnum++) {
		INIT_LIST_HEAD(&mngr.guest_array[gnum].head);
		INIT_SPIN_LOCK(&mngr.guest_array[gnum].lock);
		mngr.guest_array[gnum].id = gnum;
		mngr.guest_array[gnum].node = NULL;
		INIT_LIST_HEAD(&mngr.guest_array[gnum].vcpu_list);
		mngr.guest_avail_array[gnum] = TRUE;
	}

	/* Initialze memory for vcpu instances */
	for (vnum = 0; vnum < CONFIG_MAX_VCPU_COUNT; vnum++) {
		INIT_LIST_HEAD(&mngr.vcpu_array[vnum].head);
		INIT_SPIN_LOCK(&mngr.vcpu_array[vnum].lock);
		mngr.vcpu_array[vnum].id = vnum;
		vmm_strcpy(mngr.vcpu_array[vnum].name, "");
		mngr.vcpu_array[vnum].node = NULL;
		mngr.vcpu_array[vnum].is_normal = FALSE;
		mngr.vcpu_array[vnum].state = VMM_VCPU_STATE_UNKNOWN;
		mngr.vcpu_avail_array[vnum] = TRUE;
	}

	return VMM_OK;
}
