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

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_compiler.h>
#include <vmm_guest_aspace.h>
#include <vmm_vcpu_irq.h>
#include <vmm_scheduler.h>
#include <vmm_waitqueue.h>
#include <vmm_manager.h>
#include <vmm_stdio.h>
#include <vmm_smp.h>
#include <arch_vcpu.h>
#include <arch_guest.h>
#include <libs/stringlib.h>

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

struct vmm_vcpu *vmm_manager_vcpu(u32 vcpu_id)
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
			rc = vmm_scheduler_state_change(vcpu, new_state);
			if (rc) {
				break;
			}
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
			rc = vmm_scheduler_state_change(vcpu, new_state);
			if (rc) {
				break;
			}
		}
		break;
	case VMM_VCPU_STATE_PAUSED:
		if ((vcpu->state == VMM_VCPU_STATE_READY) ||
		    (vcpu->state == VMM_VCPU_STATE_RUNNING)) {
			rc = vmm_scheduler_state_change(vcpu, new_state);
			if (rc) {
				break;
			}
		}
		break;
	case VMM_VCPU_STATE_HALTED:
		if ((vcpu->state == VMM_VCPU_STATE_READY) ||
		    (vcpu->state == VMM_VCPU_STATE_RUNNING)) {
			rc = vmm_scheduler_state_change(vcpu, new_state);
			if (rc) {
				break;
			}
		}
		break;
	}
	vmm_spin_unlock_irqrestore(&vcpu->lock, flags);

	return rc;
 }

int vmm_manager_vcpu_reset(struct vmm_vcpu *vcpu)
{
	return vmm_manager_vcpu_state_change(vcpu, VMM_VCPU_STATE_RESET);
}

int vmm_manager_vcpu_kick(struct vmm_vcpu *vcpu)
{
	return vmm_manager_vcpu_state_change(vcpu, VMM_VCPU_STATE_READY);
}

int vmm_manager_vcpu_pause(struct vmm_vcpu *vcpu)
{
	return vmm_manager_vcpu_state_change(vcpu, VMM_VCPU_STATE_PAUSED);
}

int vmm_manager_vcpu_resume(struct vmm_vcpu *vcpu)
{
	return vmm_manager_vcpu_state_change(vcpu, VMM_VCPU_STATE_READY);
}

int vmm_manager_vcpu_halt(struct vmm_vcpu *vcpu)
{
	return vmm_manager_vcpu_state_change(vcpu, VMM_VCPU_STATE_HALTED);
}

int vmm_manager_vcpu_dumpreg(struct vmm_vcpu *vcpu)
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

int vmm_manager_vcpu_dumpstat(struct vmm_vcpu *vcpu)
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

struct vmm_vcpu *vmm_manager_vcpu_orphan_create(const char *name,
					    virtual_addr_t start_pc,
					    virtual_size_t stack_sz,
					    u8 priority,
					    u64 time_slice_nsecs)
{
	struct vmm_vcpu *vcpu = NULL;
	int vnum;
	irq_flags_t flags;

	/* Sanity checks */
	if (name == NULL || start_pc == 0 || time_slice_nsecs == 0) {
		return vcpu;
	}
	if (VMM_VCPU_MAX_PRIORITY < priority) {
		priority = VMM_VCPU_MAX_PRIORITY;
	}

	/* Acquire lock */
	vmm_spin_lock_irqsave(&mngr.lock, flags);

	/* Find the next available vcpu */
	for (vnum = 0; vnum < CONFIG_MAX_VCPU_COUNT; vnum++) {
		if (mngr.vcpu_avail_array[vnum]) {
			vcpu = &mngr.vcpu_array[vnum];
			break;
		}
	}

	if (!vcpu) {
		goto release_lock;
	}

	/* Alloc stack pages */
	vcpu->stack_va = (virtual_addr_t)vmm_malloc(stack_sz);
	if (!vcpu->stack_va) {
		vcpu = NULL;
		goto release_lock;
	}
	vcpu->stack_sz = stack_sz;

	/* Update vcpu attributes */
	INIT_SPIN_LOCK(&vcpu->lock);
	INIT_LIST_HEAD(&vcpu->head);
	vcpu->subid = 0;
	strcpy(vcpu->name, name);
	vcpu->node = NULL;
	vcpu->is_normal = FALSE;
	vcpu->state = VMM_VCPU_STATE_UNKNOWN;
	vcpu->reset_count = 0;
	vcpu->preempt_count = 0;
	vcpu->priority = priority;
	vcpu->time_slice = time_slice_nsecs;
	vcpu->start_pc = start_pc;
	vcpu->guest = NULL;
	vcpu->arch_priv = NULL;

	/* Initialize VCPU */
	if (arch_vcpu_init(vcpu)) {
		vcpu = NULL;
		goto release_lock;
	}

#ifdef CONFIG_SMP
	/* Set hcpu to current CPU */
	vcpu->hcpu = vmm_smp_processor_id();
#endif

	/* Notify scheduler about new VCPU */
	vcpu->sched_priv = NULL;
	if (vmm_scheduler_state_change(vcpu, 
					VMM_VCPU_STATE_RESET)) {
		vcpu = NULL;
		goto release_lock;
	}

	/* Set wait queue context to NULL */
	INIT_LIST_HEAD(&vcpu->wq_head);
	vcpu->wq_priv = NULL;

	/* Set device emulation context to NULL */
	vcpu->devemu_priv = NULL;

	/* Add VCPU to orphan list */
	list_add_tail(&vcpu->head, &mngr.orphan_vcpu_list);

	/* Increment vcpu count */
	mngr.vcpu_count++;

	mngr.vcpu_avail_array[vnum] = FALSE;

release_lock:
	/* Release lock */
	vmm_spin_unlock_irqrestore(&mngr.lock, flags);

	return vcpu;
}

int vmm_manager_vcpu_orphan_destroy(struct vmm_vcpu *vcpu)
{
	int rc = VMM_EFAIL;
	irq_flags_t flags;

	/* Sanity checks */
	if (!vcpu) {
		return rc;
	}
	if (vcpu->is_normal) {
		return rc;
	}

	/* Force VCPU out of waitqueue */
	vmm_waitqueue_forced_remove(vcpu);

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
	if ((rc = vmm_scheduler_state_change(vcpu, 
					VMM_VCPU_STATE_UNKNOWN))) {
		goto release_lock;
	}
	vcpu->sched_priv = NULL;

	/* Deinit VCPU */
	if ((rc = arch_vcpu_deinit(vcpu))) {
		goto release_lock;
	}

	/* Free stack pages */
	if (vcpu->stack_va) {
		vmm_free((void *)vcpu->stack_va);
	}

	/* Mark VCPU as available */
	mngr.vcpu_avail_array[vcpu->id] = TRUE;

release_lock:
	/* Release lock */
	vmm_spin_unlock_irqrestore(&mngr.lock, flags);

	return rc;
}

u32 vmm_manager_max_guest_count(void)
{
	return CONFIG_MAX_GUEST_COUNT;
}

u32 vmm_manager_guest_count(void)
{
	return mngr.guest_count;
}

struct vmm_guest *vmm_manager_guest(u32 guest_id)
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

struct vmm_vcpu *vmm_manager_guest_vcpu(struct vmm_guest *guest, u32 subid)
{
	bool found = FALSE;
	struct dlist *lentry;
	struct vmm_vcpu *vcpu = NULL;

	if (guest) {
		list_for_each(lentry, &guest->vcpu_list) {
			vcpu = list_entry(lentry, struct vmm_vcpu, head);
			if (vcpu->subid == subid) {
				found = TRUE;
				break;
			}
		}

		if (!found) {
			vcpu = NULL;
		}
	}

	return vcpu;
}

int vmm_manager_guest_reset(struct vmm_guest *guest)
{
	int rc = VMM_EFAIL;
	struct dlist *lentry;
	struct vmm_vcpu *vcpu;

	if (guest) {
		list_for_each(lentry, &guest->vcpu_list) {
			vcpu = list_entry(lentry, struct vmm_vcpu, head);
			if ((rc = vmm_manager_vcpu_reset(vcpu))) {
				return rc;
			}
		}
		if (!(rc = vmm_guest_aspace_reset(guest))) {
			guest->reset_count++;
			rc = arch_guest_init(guest);
		}
	}
	return rc;
}

int vmm_manager_guest_kick(struct vmm_guest *guest)
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

int vmm_manager_guest_pause(struct vmm_guest *guest)
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

int vmm_manager_guest_resume(struct vmm_guest *guest)
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

int vmm_manager_guest_halt(struct vmm_guest *guest)
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

int vmm_manager_guest_dumpreg(struct vmm_guest *guest)
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

struct vmm_guest *vmm_manager_guest_create(struct vmm_devtree_node *gnode)
{
	int vnum, gnum;
	const char *attrval;
	irq_flags_t flags;
	struct dlist *lentry;
	struct vmm_devtree_node *vsnode;
	struct vmm_devtree_node *vnode;
	struct vmm_guest *guest = NULL;
	struct vmm_vcpu *vcpu = NULL;

	/* Sanity checks */
	if (!gnode) {
		return guest;
	}
	attrval = vmm_devtree_attrval(gnode,
				      VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME);
	if (!attrval) {
		return guest;
	}
	if (strcmp(attrval, VMM_DEVTREE_DEVICE_TYPE_VAL_GUEST) != 0) {
		return guest;
	}

	/* Acquire lock */
	vmm_spin_lock_irqsave(&mngr.lock, flags);

	/* Ensure guest node uniqueness */
	list_for_each(lentry, &mngr.guest_list) {
		guest = list_entry(lentry, struct vmm_guest, head);
		if ((guest->node == gnode) ||
		    (strcmp(guest->node->name, gnode->name) == 0)) {
			vmm_spin_unlock_irqrestore(&mngr.lock, flags);
			vmm_printf("%s: Duplicate guest \"%s\" detected\n", 
					__func__, gnode->name);
			return NULL;
		}
	}
	guest = NULL;

	/* Find next available guest instance */
	for (gnum = 0; gnum < CONFIG_MAX_GUEST_COUNT; gnum++) {
		if (mngr.guest_avail_array[gnum]) {
			guest = &mngr.guest_array[gnum];
			mngr.guest_avail_array[gnum] = FALSE;
			break;
		}
	}

	if (!guest) {
		vmm_spin_unlock_irqrestore(&mngr.lock, flags);
		vmm_printf("%s: No available guest instance found\n", __func__);
		return guest;
	}

	/* Initialize guest instance */
	INIT_SPIN_LOCK(&guest->lock);
	list_add_tail(&guest->head, &mngr.guest_list);
	guest->node = gnode;
	guest->reset_count = 0;
	guest->vcpu_count = 0;
	INIT_LIST_HEAD(&guest->vcpu_list);
	guest->arch_priv = NULL;

	vsnode = vmm_devtree_getchild(gnode, VMM_DEVTREE_VCPUS_NODE_NAME);
	if (!vsnode) {
		vmm_printf("%s: %s/vcpus node not found\n", __func__, gnode->name);
		goto guest_create_error;
	}
	list_for_each(lentry, &vsnode->child_list) {
		vnode = list_entry(lentry, struct vmm_devtree_node, head);

		/* Sanity checks */
		if (CONFIG_MAX_VCPU_COUNT <= mngr.vcpu_count) {
			break;
		}
		attrval = vmm_devtree_attrval(vnode,
					      VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME);
		if (!attrval) {
			continue;
		}
		if (strcmp(attrval, VMM_DEVTREE_DEVICE_TYPE_VAL_VCPU) != 0) {
			continue;
		}

		/* Find next available vcpu instance */
		for (vnum = 0; vnum < CONFIG_MAX_VCPU_COUNT; vnum++) {
			if (mngr.vcpu_avail_array[vnum]) {
				vcpu = &mngr.vcpu_array[vnum];
				mngr.vcpu_avail_array[vnum] = FALSE;
				break;
			}
		}

		if (!vcpu) {
			break;
		}

		/* Initialize vcpu instance */
		INIT_SPIN_LOCK(&vcpu->lock);
		vcpu->subid = guest->vcpu_count;
		strcpy(vcpu->name, gnode->name);
		strcat(vcpu->name, VMM_DEVTREE_PATH_SEPARATOR_STRING);
		strcat(vcpu->name, vnode->name);
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

		/* Alloc stack pages */
		vcpu->stack_va = 
			(virtual_addr_t)vmm_malloc(CONFIG_IRQ_STACK_SIZE);
		if (!vcpu->stack_va) {
			continue;
		}
		vcpu->stack_sz = CONFIG_IRQ_STACK_SIZE;

		/* Architecture specific VCPU initialization */
		vcpu->guest = guest;
		vcpu->arch_priv = NULL;
		if (arch_vcpu_init(vcpu)) {
			continue;
		}

		/* Initialize VCPU IRQs */
		if (vmm_vcpu_irq_init(vcpu)) {
			continue;
		}

#ifdef CONFIG_SMP
		/* Set hcpu to current CPU */
		vcpu->hcpu = vmm_smp_processor_id();
#endif

		/* Notify scheduler about new VCPU */
		vcpu->sched_priv = NULL;
		if (vmm_scheduler_state_change(vcpu, 
						VMM_VCPU_STATE_RESET)) {
			mngr.vcpu_avail_array[vnum] = TRUE;
			break;
		}

		/* Set wait queue context to NULL */
		INIT_LIST_HEAD(&vcpu->wq_head);
		vcpu->wq_priv = NULL;

		/* Set device emulation context to NULL */
		vcpu->devemu_priv = NULL;

		/* Add VCPU to Guest child list */
		list_add_tail(&vcpu->head, &guest->vcpu_list);

		/* Increment vcpu count */
		mngr.vcpu_count++;
		guest->vcpu_count++;
	}

	/* Initialize guest address space */
	if (vmm_guest_aspace_init(guest)) {
		goto guest_create_error;
	}

	/* Reset guest address space */
	if (vmm_guest_aspace_reset(guest)) {
		goto guest_create_error;
	}

	/* Initialize arch guest context */
	if (arch_guest_init(guest)) {
		goto guest_create_error;
	}

	/* Increment guest count */
	mngr.guest_count++;

	/* Release lock */
	vmm_spin_unlock_irqrestore(&mngr.lock, flags);

	return guest;

guest_create_error:
	vmm_spin_unlock_irqrestore(&mngr.lock, flags);
	vmm_manager_guest_destroy(guest);

	return NULL;
}

int vmm_manager_guest_destroy(struct vmm_guest *guest)
{
	int rc;
	irq_flags_t flags;
	struct dlist *l;
	struct vmm_vcpu *vcpu;

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
		goto release_lock;
	}

	/* Deinit the guest aspace */
	if ((rc = vmm_guest_aspace_deinit(guest))) {
		goto release_lock;
	}

	/* Destroy each VCPU of guest */
	while (!list_empty(&guest->vcpu_list)) {
		l = list_pop(&guest->vcpu_list);
		vcpu = list_entry(l, struct vmm_vcpu, head);

		/* Decrement vcpu count */
		mngr.vcpu_count--;

		/* Notify scheduler about VCPU state change */
		if ((rc = vmm_scheduler_state_change(vcpu, 
					VMM_VCPU_STATE_UNKNOWN))) {
			goto release_lock;
		}
		vcpu->sched_priv = NULL;

		/* Deinit VCPU */
		if ((rc = arch_vcpu_deinit(vcpu))) {
			goto release_lock;
		}
		if ((rc = vmm_vcpu_irq_deinit(vcpu))) {
			goto release_lock;
		}

		/* Free stack pages */
		if (vcpu->stack_va) {
			vmm_free((void *)vcpu->stack_va);
		}

		/* Mark VCPU as available */
		mngr.vcpu_avail_array[vcpu->id] = TRUE;
	}

	/* Reset guest instance members */
	INIT_LIST_HEAD(&mngr.guest_array[guest->id].head);
	mngr.guest_array[guest->id].node = NULL;
	INIT_LIST_HEAD(&mngr.guest_array[guest->id].vcpu_list);
	mngr.guest_avail_array[guest->id] = TRUE;

release_lock:
	/* Release lock */
	vmm_spin_unlock_irqrestore(&mngr.lock, flags);

	return rc;
}

int __init vmm_manager_init(void)
{
	u32 vnum, gnum;

	/* Reset the manager control structure */
	memset(&mngr, 0, sizeof(mngr));

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
		strcpy(mngr.vcpu_array[vnum].name, "");
		mngr.vcpu_array[vnum].node = NULL;
		mngr.vcpu_array[vnum].is_normal = FALSE;
		mngr.vcpu_array[vnum].state = VMM_VCPU_STATE_UNKNOWN;
		mngr.vcpu_avail_array[vnum] = TRUE;
	}

	return VMM_OK;
}
