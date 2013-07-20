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
#include <vmm_loadbal.h>
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

static void vmm_manager_ipi_state_change(void *vcpu, 
					 void *new_state, void *dummy)
{
	vmm_scheduler_state_change(vcpu, (u32)new_state);
}

static int vmm_manager_vcpu_state_change(struct vmm_vcpu *vcpu, u32 new_state)
{
	u32 hcpu;
	irq_flags_t flags;

	vmm_spin_lock_irqsave_lite(&vcpu->loadbal_lock, flags);

	if (new_state == VMM_VCPU_STATE_READY) {
		vcpu->hcpu = vmm_loadbal_get_next_hcpu(vcpu);
	}

	hcpu = vcpu->hcpu;

	vmm_spin_unlock_irqrestore_lite(&vcpu->loadbal_lock, flags);

	vmm_smp_ipi_async_call(vmm_cpumask_of(hcpu),
			       vmm_manager_ipi_state_change,
			       vcpu, (void *)new_state, NULL);

	return VMM_OK;
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
		vmm_spin_lock_irqsave_lite(&vcpu->sched_lock, flags);
		if (vcpu->state != VMM_VCPU_STATE_RUNNING) {
			arch_vcpu_regs_dump(vcpu);
			rc = VMM_OK;
		}
		vmm_spin_unlock_irqrestore_lite(&vcpu->sched_lock, flags);
	}
	return rc;
}

int vmm_manager_vcpu_dumpstat(struct vmm_vcpu *vcpu)
{
	int rc = VMM_EFAIL;
	irq_flags_t flags;
	if (vcpu) {
		vmm_spin_lock_irqsave_lite(&vcpu->sched_lock, flags);
		if (vcpu->state != VMM_VCPU_STATE_RUNNING) {
			arch_vcpu_stat_dump(vcpu);
			rc = VMM_OK;
		}
		vmm_spin_unlock_irqrestore_lite(&vcpu->sched_lock, flags);
	}
	return rc;
}

u32 vmm_manager_vcpu_state(struct vmm_vcpu *vcpu)
{
	u32 state = VMM_VCPU_STATE_UNKNOWN;
	irq_flags_t flags;
	if (vcpu) {
		vmm_spin_lock_irqsave_lite(&vcpu->sched_lock, flags);
		state = vcpu->state;
		vmm_spin_unlock_irqrestore_lite(&vcpu->sched_lock, flags);
	}
	return state;
}

int vmm_manager_vcpu_set_affinity(struct vmm_vcpu *vcpu, 
				  const struct vmm_cpumask *cpu_mask)
{
	vcpu->cpu_affinity = cpu_mask;

	return VMM_OK;
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
	vmm_spin_lock_irqsave_lite(&mngr.lock, flags);

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

	INIT_LIST_HEAD(&vcpu->head);

	/* Update general info */
	vcpu->subid = 0;
	if (strlcpy(vcpu->name, name, sizeof(vcpu->name)) >=
	    sizeof(vcpu->name)) {
		vcpu = NULL;
		goto release_lock;
	}
	vcpu->node = NULL;
	vcpu->is_normal = FALSE;
	vcpu->guest = NULL;

	/* Setup start program counter and stack */
	vcpu->start_pc = start_pc;
	vcpu->stack_va = (virtual_addr_t)vmm_malloc(stack_sz);
	if (!vcpu->stack_va) {
		vcpu = NULL;
		goto release_lock;
	}
	vcpu->stack_sz = stack_sz;

	/* Initialize load balancing context */
	INIT_SPIN_LOCK(&vcpu->loadbal_lock);
	vcpu->hcpu = vmm_loadbal_get_new_hcpu(vcpu);
	vmm_manager_vcpu_set_affinity(vcpu,
				      vmm_cpumask_of(vcpu->hcpu));

	/* Intialize scheduling context */
	INIT_SPIN_LOCK(&vcpu->sched_lock);
	vcpu->state = VMM_VCPU_STATE_UNKNOWN;
	vcpu->reset_count = 0;
	vcpu->preempt_count = 0;
	vcpu->priority = priority;
	vcpu->time_slice = time_slice_nsecs;
	vcpu->sched_priv = NULL;

	/* Initialize architecture specific context */
	vcpu->arch_priv = NULL;
	if (arch_vcpu_init(vcpu)) {
		vmm_free((void *)vcpu->stack_va);
		vcpu = NULL;
		goto release_lock;
	}

	/* Initialize waitqueue context */
	INIT_LIST_HEAD(&vcpu->wq_head);
	vcpu->wq_priv = NULL;

	/* Initialize device emulation context */
	vcpu->devemu_priv = NULL;

	/* Notify scheduler about new VCPU */
	if (vmm_manager_vcpu_state_change(vcpu, 
					VMM_VCPU_STATE_RESET)) {
		arch_vcpu_deinit(vcpu);
		vmm_free((void *)vcpu->stack_va);
		vcpu = NULL;
		goto release_lock;
	}

	/* Update VCPU affinity */
	vmm_manager_vcpu_set_affinity(vcpu, cpu_online_mask);

	/* Add VCPU to orphan list */
	list_add_tail(&vcpu->head, &mngr.orphan_vcpu_list);

	/* Increment vcpu count */
	mngr.vcpu_count++;

	/* Mark this VCPU as not available */
	mngr.vcpu_avail_array[vcpu->id] = FALSE;

release_lock:
	/* Release lock */
	vmm_spin_unlock_irqrestore_lite(&mngr.lock, flags);

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
	vmm_spin_lock_irqsave_lite(&mngr.lock, flags);

	/* Decrement vcpu count */
	mngr.vcpu_count--;

	/* Remove VCPU from orphan list */
	list_del(&vcpu->head);

	/* Notify scheduler about VCPU state change */
	if ((rc = vmm_manager_vcpu_state_change(vcpu, 
					VMM_VCPU_STATE_UNKNOWN))) {
		goto release_lock;
	}

	/* Deinit architecture specific context */
	if ((rc = arch_vcpu_deinit(vcpu))) {
		goto release_lock;
	}

	/* Cleanup scheduling context */
	vcpu->sched_priv = NULL;

	/* Free stack pages */
	if (vcpu->stack_va) {
		vmm_free((void *)vcpu->stack_va);
	}

	/* Mark this VCPU as available */
	mngr.vcpu_avail_array[vcpu->id] = TRUE;

release_lock:
	/* Release lock */
	vmm_spin_unlock_irqrestore_lite(&mngr.lock, flags);

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
	struct vmm_guest *guest;
	struct vmm_vcpu *vcpu;

	/* Sanity checks */
	if (!gnode) {
		return NULL;
	}
	attrval = vmm_devtree_attrval(gnode,
				      VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME);
	if (!attrval) {
		return NULL;
	}
	if (strcmp(attrval, VMM_DEVTREE_DEVICE_TYPE_VAL_GUEST) != 0) {
		return NULL;
	}

	/* Acquire lock */
	vmm_spin_lock_irqsave_lite(&mngr.lock, flags);

	/* Ensure guest node uniqueness */
	list_for_each(lentry, &mngr.guest_list) {
		guest = list_entry(lentry, struct vmm_guest, head);
		if ((guest->node == gnode) ||
		    (strcmp(guest->node->name, gnode->name) == 0)) {
			vmm_spin_unlock_irqrestore_lite(&mngr.lock, flags);
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
			break;
		}
	}

	if (!guest) {
		vmm_spin_unlock_irqrestore_lite(&mngr.lock, flags);
		vmm_printf("%s: No available guest instance found\n", __func__);
		return NULL;
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

		vcpu = NULL;

		/* Find next available VCPU instance */
		for (vnum = 0; vnum < CONFIG_MAX_VCPU_COUNT; vnum++) {
			if (mngr.vcpu_avail_array[vnum]) {
				vcpu = &mngr.vcpu_array[vnum];
				break;
			}
		}

		if (!vcpu) {
			break;
		}

		/* Initialize general info */
		vcpu->subid = guest->vcpu_count;
		strlcpy(vcpu->name, gnode->name, sizeof(vcpu->name));
		strlcat(vcpu->name, VMM_DEVTREE_PATH_SEPARATOR_STRING,
			sizeof(vcpu->name));
		if (strlcat(vcpu->name, vnode->name, sizeof(vcpu->name)) >=
		    sizeof(vcpu->name)) {
			continue;
		}
		vcpu->node = vnode;
		vcpu->is_normal = TRUE;
		vcpu->guest = guest;

		/* Setup start program counter and stack */
		attrval = vmm_devtree_attrval(vnode,
					      VMM_DEVTREE_START_PC_ATTR_NAME);
		if (attrval) {
			vcpu->start_pc = *((virtual_addr_t *) attrval);
		}
		vcpu->stack_va = 
			(virtual_addr_t)vmm_malloc(CONFIG_IRQ_STACK_SIZE);
		if (!vcpu->stack_va) {
			continue;
		}
		vcpu->stack_sz = CONFIG_IRQ_STACK_SIZE;

		/* Initialize load balancing context */
		INIT_SPIN_LOCK(&vcpu->loadbal_lock);
		vcpu->hcpu = vmm_loadbal_get_new_hcpu(vcpu);
		vmm_manager_vcpu_set_affinity(vcpu, vmm_cpumask_of(vcpu->hcpu));

		/* Initialize scheduling context */
		INIT_SPIN_LOCK(&vcpu->sched_lock);
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
		vcpu->sched_priv = NULL;

		/* Initialize architecture specific context */
		vcpu->arch_priv = NULL;
		if (arch_vcpu_init(vcpu)) {
			vmm_free((void *)vcpu->stack_va);
			continue;
		}

		/* Initialize virtual IRQ context */
		if (vmm_vcpu_irq_init(vcpu)) {
			arch_vcpu_deinit(vcpu);
			vmm_free((void *)vcpu->stack_va);
			continue;
		}

		/* Initialize waitqueue context */
		INIT_LIST_HEAD(&vcpu->wq_head);
		vcpu->wq_priv = NULL;

		/* Initialize device emulation context */
		vcpu->devemu_priv = NULL;

		/* Notify scheduler about new VCPU */
		if (vmm_manager_vcpu_state_change(vcpu, 
						VMM_VCPU_STATE_RESET)) {
			vmm_vcpu_irq_deinit(vcpu);
			arch_vcpu_deinit(vcpu);
			vmm_free((void *)vcpu->stack_va);
			continue;
		}

		/* Correct affinity */
		vmm_manager_vcpu_set_affinity(vcpu, cpu_online_mask);

		/* Add VCPU to Guest child list */
		list_add_tail(&vcpu->head, &guest->vcpu_list);

		/* Mark this VCPU instance as not available */
		mngr.vcpu_avail_array[vcpu->id] = FALSE;

		/* Increment VCPU count */
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

	mngr.guest_avail_array[guest->id] = FALSE;

	/* Increment guest count */
	mngr.guest_count++;

	/* Release lock */
	vmm_spin_unlock_irqrestore_lite(&mngr.lock, flags);

	return guest;

guest_create_error:
	vmm_spin_unlock_irqrestore_lite(&mngr.lock, flags);
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
	vmm_spin_lock_irqsave_lite(&mngr.lock, flags);

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
		if ((rc = vmm_manager_vcpu_state_change(vcpu, 
					VMM_VCPU_STATE_UNKNOWN))) {
			goto release_lock;
		}

		/* Deinit Virtual IRQ context */
		if ((rc = vmm_vcpu_irq_deinit(vcpu))) {
			goto release_lock;
		}

		/* Deinit architecture specific context */
		if ((rc = arch_vcpu_deinit(vcpu))) {
			goto release_lock;
		}

		/* Cleanup scheduling context */
		vcpu->sched_priv = NULL;

		/* Free stack pages */
		if (vcpu->stack_va) {
			vmm_free((void *)vcpu->stack_va);
		}

		/* Mark this VCPU as available */
		mngr.vcpu_avail_array[vcpu->id] = TRUE;
	}

	/* Reset guest instance members */
	INIT_LIST_HEAD(&mngr.guest_array[guest->id].head);
	mngr.guest_array[guest->id].node = NULL;
	INIT_LIST_HEAD(&mngr.guest_array[guest->id].vcpu_list);
	mngr.guest_avail_array[guest->id] = TRUE;

release_lock:
	/* Release lock */
	vmm_spin_unlock_irqrestore_lite(&mngr.lock, flags);

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
		mngr.vcpu_array[vnum].id = vnum;
		mngr.vcpu_array[vnum].name[0] = 0;
		mngr.vcpu_array[vnum].node = NULL;
		mngr.vcpu_array[vnum].is_normal = FALSE;
		INIT_SPIN_LOCK(&mngr.vcpu_array[vnum].loadbal_lock);
		mngr.vcpu_array[vnum].state = VMM_VCPU_STATE_UNKNOWN;
		INIT_SPIN_LOCK(&mngr.vcpu_array[vnum].sched_lock);
		mngr.vcpu_avail_array[vnum] = TRUE;
	}

	return VMM_OK;
}
