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
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for hypervisor manager
 */

#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_heap.h>
#include <vmm_cpu.h>
#include <vmm_devtree.h>
#include <vmm_guest_aspace.h>
#include <vmm_vcpu_irq.h>
#include <vmm_manager.h>

vmm_manager_ctrl_t mngr;

u32 vmm_manager_max_vcpu_count(void)
{
	return mngr.max_vcpu_count;
}

u32 vmm_manager_vcpu_count(void)
{
	return mngr.vcpu_count;
}

vmm_vcpu_t * vmm_manager_vcpu(u32 vcpu_id)
{
	if (vcpu_id < mngr.vcpu_count) {
		return &mngr.vcpu_array[vcpu_id];
	}
	return NULL;
}

int vmm_manager_vcpu_reset(vmm_vcpu_t * vcpu)
{
	int rc = VMM_EFAIL;
	irq_flags_t flags;
	if (vcpu) {
		flags = vmm_spin_lock_irqsave(&vcpu->lock);
		if ((vcpu->state != VMM_VCPU_STATE_RESET) &&
		    (vcpu->state != VMM_VCPU_STATE_UNKNOWN)) {
			vcpu->state = VMM_VCPU_STATE_RESET;
			vcpu->reset_count++;
			if ((rc = vmm_vcpu_regs_init(vcpu))) {
				return rc;
			}
			if ((rc = vmm_vcpu_irq_init(vcpu))) {
				return rc;
			}
		}
		vmm_spin_unlock_irqrestore(&vcpu->lock, flags);
	}
	return rc;
}

int vmm_manager_vcpu_kick(vmm_vcpu_t * vcpu)
{
	int rc = VMM_EFAIL;
	irq_flags_t flags;
	if (vcpu) {
		flags = vmm_spin_lock_irqsave(&vcpu->lock);
		if (vcpu->state == VMM_VCPU_STATE_RESET) {
			vcpu->state = VMM_VCPU_STATE_READY;
			rc = VMM_OK;
		}
		vmm_spin_unlock_irqrestore(&vcpu->lock, flags);
	}
	return rc;
}

int vmm_manager_vcpu_pause(vmm_vcpu_t * vcpu)
{
	int rc = VMM_EFAIL;
	irq_flags_t flags;
	if (vcpu) {
		flags = vmm_spin_lock_irqsave(&vcpu->lock);
		if ((vcpu->state == VMM_VCPU_STATE_READY) ||
		    (vcpu->state == VMM_VCPU_STATE_RUNNING)) {
			vcpu->state = VMM_VCPU_STATE_PAUSED;
			rc = VMM_OK;
		}
		vmm_spin_unlock_irqrestore(&vcpu->lock, flags);
	}
	return rc;
}

int vmm_manager_vcpu_resume(vmm_vcpu_t * vcpu)
{
	int rc = VMM_EFAIL;
	irq_flags_t flags;
	if (vcpu) {
		flags = vmm_spin_lock_irqsave(&vcpu->lock);
		if (vcpu->state == VMM_VCPU_STATE_PAUSED) {
			vcpu->state = VMM_VCPU_STATE_READY;
			rc = VMM_OK;
		}
		vmm_spin_unlock_irqrestore(&vcpu->lock, flags);
	}
	return rc;
}

int vmm_manager_vcpu_halt(vmm_vcpu_t * vcpu)
{
	int rc = VMM_EFAIL;
	irq_flags_t flags;
	if (vcpu) {
		flags = vmm_spin_lock_irqsave(&vcpu->lock);
		if ((vcpu->state == VMM_VCPU_STATE_READY) ||
		    (vcpu->state == VMM_VCPU_STATE_RUNNING)) {
			vcpu->state = VMM_VCPU_STATE_HALTED;
			rc = VMM_OK;
		}
		vmm_spin_unlock_irqrestore(&vcpu->lock, flags);
	}
	return rc;
}

int vmm_manager_vcpu_dumpreg(vmm_vcpu_t * vcpu)
{
	int rc = VMM_EFAIL;
	irq_flags_t flags;
	if (vcpu) {
		flags = vmm_spin_lock_irqsave(&vcpu->lock);
		if (vcpu->state != VMM_VCPU_STATE_RUNNING) {
			vmm_vcpu_regs_dump(vcpu);
			rc = VMM_OK;
		}
		vmm_spin_unlock_irqrestore(&vcpu->lock, flags);
	}
	return rc;
}

vmm_vcpu_t * vmm_manager_vcpu_orphan_create(const char *name,
					    virtual_addr_t start_pc,
					    virtual_addr_t start_sp,
					    u64 time_slice_nsecs)
{
	vmm_vcpu_t * vcpu;
	irq_flags_t flags;

	/* Sanity checks */
	if (name == NULL || start_pc == 0 || time_slice_nsecs == 0) {
		return NULL;
	}

	/* Acquire lock */
	flags = vmm_spin_lock_irqsave(&mngr.lock);

	/* Find the next available vcpu */
	vcpu = &mngr.vcpu_array[mngr.vcpu_count];

	/* Add it to orphan list */
	list_add_tail(&mngr.orphan_vcpu_list, &vcpu->head);

	/* Update vcpu attributes */
	INIT_SPIN_LOCK(&vcpu->lock);
	vcpu->subid = 0;
	vmm_strcpy(vcpu->name, name);
	vcpu->node = NULL;
	vcpu->state = VMM_VCPU_STATE_READY;
	vcpu->reset_count = 0;
	vcpu->preempt_count = 0;
	vcpu->time_slice = time_slice_nsecs;
	vcpu->start_pc = start_pc;
	vcpu->start_sp = start_sp;
	vcpu->guest = NULL;
	vcpu->uregs = vmm_malloc(sizeof(vmm_user_regs_t));
	vcpu->sregs = NULL;
	vcpu->irqs = NULL;

	/* Sanity Check */
	if (!vcpu->uregs) {
		return NULL;
	}

	/* Initialize registers */
	if (vmm_vcpu_regs_init(vcpu)) {
		vcpu = NULL;
	}

	/* Increment vcpu count */
	if (vcpu) {
		mngr.vcpu_count++;
	}

	/* Release lock */
	vmm_spin_unlock_irqrestore(&mngr.lock, flags);

	return vcpu;
}

int vmm_manager_vcpu_orphan_destroy(vmm_vcpu_t * vcpu)
{
	/* FIXME: TBD */
	return VMM_OK;
}

u32 vmm_manager_max_guest_count(void)
{
	return mngr.max_guest_count;
}

u32 vmm_manager_guest_count(void)
{
	return mngr.guest_count;
}

vmm_guest_t * vmm_manager_guest(u32 guest_id)
{
	if (guest_id < mngr.guest_count) {
		return &mngr.guest_array[guest_id];
	}
	return NULL;
}

u32 vmm_manager_guest_vcpu_count(vmm_guest_t *guest)
{
	if (!guest) {
		return 0;
	}

	return guest->vcpu_count;
}

vmm_vcpu_t * vmm_manager_guest_vcpu(vmm_guest_t *guest, u32 subid)
{
	bool found = FALSE;
	vmm_vcpu_t *vcpu = NULL;
	struct dlist *l;

	if (!guest) {
		return NULL;
	}

	list_for_each(l, &guest->vcpu_list) {
		vcpu = list_entry(l, vmm_vcpu_t, head);
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

int vmm_manager_guest_reset(vmm_guest_t * guest)
{
	int rc = VMM_EFAIL;
	struct dlist *lentry;
	vmm_vcpu_t *vcpu;
	if (guest) {
		rc = VMM_OK;
		list_for_each(lentry, &guest->vcpu_list) {
			vcpu = list_entry(lentry, vmm_vcpu_t, head);
			if ((rc = vmm_manager_vcpu_reset(vcpu))) {
				break;
			}
		}
		if (!rc) {
			rc = vmm_guest_aspace_reset(guest);
		}
	}
	return rc;
}

int vmm_manager_guest_kick(vmm_guest_t * guest)
{
	int rc = VMM_EFAIL;
	struct dlist *lentry;
	vmm_vcpu_t *vcpu;
	if (guest) {
		rc = VMM_OK;
		list_for_each(lentry, &guest->vcpu_list) {
			vcpu = list_entry(lentry, vmm_vcpu_t, head);
			if ((rc = vmm_manager_vcpu_kick(vcpu))) {
				break;
			}
		}
	}
	return rc;
}

int vmm_manager_guest_pause(vmm_guest_t * guest)
{
	int rc = VMM_EFAIL;
	struct dlist *lentry;
	vmm_vcpu_t *vcpu;
	if (guest) {
		rc = VMM_OK;
		list_for_each(lentry, &guest->vcpu_list) {
			vcpu = list_entry(lentry, vmm_vcpu_t, head);
			if ((rc = vmm_manager_vcpu_pause(vcpu))) {
				break;
			}
		}
	}
	return rc;
}

int vmm_manager_guest_resume(vmm_guest_t * guest)
{
	int rc = VMM_EFAIL;
	struct dlist *lentry;
	vmm_vcpu_t *vcpu;
	if (guest) {
		rc = VMM_OK;
		list_for_each(lentry, &guest->vcpu_list) {
			vcpu = list_entry(lentry, vmm_vcpu_t, head);
			if ((rc = vmm_manager_vcpu_resume(vcpu))) {
				break;
			}
		}
	}
	return rc;
}

int vmm_manager_guest_halt(vmm_guest_t * guest)
{
	int rc = VMM_EFAIL;
	struct dlist *lentry;
	vmm_vcpu_t *vcpu;
	if (guest) {
		rc = VMM_OK;
		list_for_each(lentry, &guest->vcpu_list) {
			vcpu = list_entry(lentry, vmm_vcpu_t, head);
			if ((rc = vmm_manager_vcpu_halt(vcpu))) {
				break;
			}
		}
	}
	return rc;
}

int vmm_manager_guest_dumpreg(vmm_guest_t * guest)
{
	int rc = VMM_EFAIL;
	struct dlist *lentry;
	vmm_vcpu_t *vcpu;
	if (guest) {
		rc = VMM_OK;
		list_for_each(lentry, &guest->vcpu_list) {
			vcpu = list_entry(lentry, vmm_vcpu_t, head);
			if ((rc = vmm_manager_vcpu_dumpreg(vcpu))) {
				break;
			}
		}
	}
	return rc;
}

vmm_guest_t * vmm_manager_guest_create(vmm_devtree_node_t * gnode)
{
	const char *attrval;
	struct dlist *l1;
	vmm_devtree_node_t *vsnode;
	vmm_devtree_node_t *vnode;
	vmm_guest_t * guest = NULL;
	vmm_vcpu_t * vcpu = NULL;

	/* Sanity checks */
	if (!gnode) {
		return NULL;
	}
	if (mngr.guest_count > mngr.max_guest_count) {
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

	/* Initialize guest instance */
	guest = &mngr.guest_array[mngr.guest_count];
	list_add_tail(&mngr.guest_list, &guest->head);
	INIT_SPIN_LOCK(&guest->lock);
	guest->node = gnode;
	guest->vcpu_count = 0;
	INIT_LIST_HEAD(&guest->vcpu_list);

	/* Initialize guest address space */
	if (vmm_guest_aspace_init(guest)) {
		return NULL;
	}

	vsnode = vmm_devtree_getchildnode(gnode,
					  VMM_DEVTREE_VCPUS_NODE_NAME);
	if (!vsnode) {
		return guest;
	}
	list_for_each(l1, &vsnode->child_list) {
		vnode = list_entry(l1, vmm_devtree_node_t, head);

		/* Sanity checks */
		if (mngr.max_vcpu_count <= mngr.vcpu_count) {
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

		/* Initialize vcpu instance */
		vcpu = &mngr.vcpu_array[mngr.vcpu_count];
		list_add_tail(&guest->vcpu_list, &vcpu->head);
		INIT_SPIN_LOCK(&vcpu->lock);
		vcpu->subid = guest->vcpu_count;
		vmm_strcpy(vcpu->name, gnode->name);
		vmm_strcat(vcpu->name,
			   VMM_DEVTREE_PATH_SEPRATOR_STRING);
		vmm_strcat(vcpu->name, vnode->name);
		vcpu->node = vnode;
		vcpu->state = VMM_VCPU_STATE_RESET;
		vcpu->reset_count = 0;
		vcpu->preempt_count = 0;
		attrval = vmm_devtree_attrval(vnode,
					     VMM_DEVTREE_TIME_SLICE_ATTR_NAME);
		if (attrval) {
			vcpu->time_slice = *((u32 *) attrval);
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
		vcpu->uregs = vmm_malloc(sizeof(vmm_user_regs_t));
		vcpu->sregs = vmm_malloc(sizeof(vmm_super_regs_t));
		vcpu->irqs = vmm_malloc(sizeof(vmm_vcpu_irqs_t));
		if (!vcpu->uregs || !vcpu->sregs || !vcpu->irqs) {
			break;
		}
		if (vmm_vcpu_regs_init(vcpu)) {
			continue;
		}
		if (vmm_vcpu_irq_init(vcpu)) {
			continue;
		}

		/* Increment vcpu count */
		mngr.vcpu_count++;
		guest->vcpu_count++;
	}

	/* Initialize guest address space */
	if (vmm_guest_aspace_probe(guest)) {
		/* FIXME: Free vcpus alotted to this guest */
		return NULL;
	}

	/* Increment guest count */
	mngr.guest_count++;

	return guest;
}

int vmm_manager_guest_destroy(vmm_guest_t * guest)
{
	/* FIXME: TBD */
	return VMM_OK;
}

int vmm_manager_init(void)
{
	u32 vnum, gnum;
	const char *attrval;
	vmm_devtree_node_t *vnode;

	/* Reset the manager control structure */
	vmm_memset(&mngr, 0, sizeof(mngr));

	/* Intialize guest & vcpu managment parameters */
	INIT_SPIN_LOCK(&mngr.lock);
	mngr.max_vcpu_count = 0;
	mngr.max_guest_count = 0;
	mngr.vcpu_count = 0;
	mngr.guest_count = 0;
	mngr.vcpu_array = NULL;
	mngr.guest_array = NULL;
	INIT_LIST_HEAD(&mngr.orphan_vcpu_list);
	INIT_LIST_HEAD(&mngr.guest_list);

	/* Get VMM information node */
	vnode = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPRATOR_STRING
				    VMM_DEVTREE_VMMINFO_NODE_NAME);
	if (!vnode) {
		return VMM_EFAIL;
	}

	/* Get max vcpu count */
	attrval = vmm_devtree_attrval(vnode,
				      VMM_DEVTREE_MAX_VCPU_COUNT_ATTR_NAME);
	if (!attrval) {
		return VMM_EFAIL;
	}
	mngr.max_vcpu_count = *((u32 *) attrval);

	/* Get max guest count */
	attrval = vmm_devtree_attrval(vnode,
				      VMM_DEVTREE_MAX_GUEST_COUNT_ATTR_NAME);
	if (!attrval) {
		return VMM_EFAIL;
	}
	mngr.max_guest_count = *((u32 *) attrval);

	/* Allocate memory for guest instances */
	mngr.guest_array =
	    vmm_malloc(sizeof(vmm_guest_t) * mngr.max_guest_count);

	/* Initialze memory for guest instances */
	for (gnum = 0; gnum < mngr.max_guest_count; gnum++) {
		vmm_memset(&mngr.guest_array[gnum], 0, sizeof(vmm_guest_t));
		INIT_LIST_HEAD(&mngr.guest_array[gnum].head);
		INIT_SPIN_LOCK(&mngr.guest_array[gnum].lock);
		mngr.guest_array[gnum].id = gnum;
		mngr.guest_array[gnum].node = NULL;
		INIT_LIST_HEAD(&mngr.guest_array[gnum].vcpu_list);
	}

	/* Allocate memory for vcpu instances */
	mngr.vcpu_array =
	    vmm_malloc(sizeof(vmm_vcpu_t) * mngr.max_vcpu_count);

	/* Initialze memory for vcpu instances */
	for (vnum = 0; vnum < mngr.max_vcpu_count; vnum++) {
		vmm_memset(&mngr.vcpu_array[vnum], 0, sizeof(vmm_vcpu_t));
		INIT_LIST_HEAD(&mngr.vcpu_array[vnum].head);
		INIT_SPIN_LOCK(&mngr.vcpu_array[vnum].lock);
		mngr.vcpu_array[vnum].id = vnum;
		vmm_strcpy(mngr.vcpu_array[vnum].name, "");
		mngr.vcpu_array[vnum].node = NULL;
		mngr.vcpu_array[vnum].state = VMM_VCPU_STATE_UNKNOWN;
	}

	return VMM_OK;
}
