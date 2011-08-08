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
 * @file vmm_scheduler.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for hypervisor scheduler
 */

#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_heap.h>
#include <vmm_cpu.h>
#include <vmm_mterm.h>
#include <vmm_devtree.h>
#include <vmm_guest_aspace.h>
#include <vmm_vcpu_irq.h>
#include <vmm_scheduler.h>

vmm_scheduler_ctrl_t sched;

void vmm_scheduler_next(vmm_user_regs_t * regs)
{
	s32 next;
	vmm_vcpu_t *cur_vcpu, *nxt_vcpu;

	/* Determine current vcpu */
	cur_vcpu = (-1 < sched.vcpu_current) ?
		    &sched.vcpu_array[sched.vcpu_current] : NULL;

	/* Determine the next ready vcpu to schedule */
	next = (cur_vcpu) ? cur_vcpu->num : -1;
	next = (next + 1) % (sched.vcpu_count);
	nxt_vcpu = &sched.vcpu_array[next];
	while ((nxt_vcpu->state != VMM_VCPU_STATE_READY) &&
		(next != sched.vcpu_current)) {
		next = (next + 1) % (sched.vcpu_count);
		nxt_vcpu = &sched.vcpu_array[next];
	}

	/* Do context switch between current and next vcpus */
	if (!cur_vcpu || (cur_vcpu->num != nxt_vcpu->num)) {
		if (cur_vcpu && (cur_vcpu->state & VMM_VCPU_STATE_SAVEABLE)) {
			if (cur_vcpu->state == VMM_VCPU_STATE_RUNNING) {
				cur_vcpu->state = VMM_VCPU_STATE_READY;
			}
			vmm_vcpu_regs_switch(cur_vcpu, nxt_vcpu, regs);
		} else {
			vmm_vcpu_regs_switch(NULL, nxt_vcpu, regs);
		}
	}

	if (nxt_vcpu) {
		nxt_vcpu->tick_pending = nxt_vcpu->tick_count;
		nxt_vcpu->state = VMM_VCPU_STATE_RUNNING;
		sched.vcpu_current = nxt_vcpu->num;
	}
}

void vmm_scheduler_tick(vmm_user_regs_t * regs, u32 ticks)
{
	vmm_vcpu_t * vcpu = (-1 < sched.vcpu_current) ? 
				&sched.vcpu_array[sched.vcpu_current] : NULL;
	if (!vcpu) {
		vmm_scheduler_next(regs);
		return;
	} 
	if (!vcpu->preempt_count) {
		if (!vcpu->tick_pending) {
			vmm_scheduler_next(regs);
		} else {
			vcpu->tick_pending-=ticks;
			if (vcpu->tick_func && !vcpu->preempt_count) {
				vcpu->tick_func(regs, vcpu->tick_pending);
			}
		}
	}
}

void vmm_scheduler_irq_process(vmm_user_regs_t * regs)
{
	vmm_vcpu_t * vcpu = NULL;

	/* Determine current vcpu */
	vcpu = (-1 < sched.vcpu_current) ? 
		&sched.vcpu_array[sched.vcpu_current] : NULL;
	if (!vcpu) {
		return;
	}

	/* Schedule next vcpu if state of 
	 * current vcpu is not RUNNING */
	if (vcpu->state != VMM_VCPU_STATE_RUNNING) {
		vmm_scheduler_next(regs);
		return;
	}

	/* VCPU irq processing */
	vmm_vcpu_irq_process(regs);

}

vmm_vcpu_t * vmm_scheduler_current_vcpu(void)
{
	irq_flags_t flags;
	vmm_vcpu_t * vcpu = NULL;
	flags = vmm_spin_lock_irqsave(&sched.lock);
	if (sched.vcpu_current != -1) {
		vcpu = &sched.vcpu_array[sched.vcpu_current];
	}
	vmm_spin_unlock_irqrestore(&sched.lock, flags);
	return vcpu;
}

vmm_guest_t * vmm_scheduler_current_guest(void)
{
	vmm_vcpu_t *vcpu = vmm_scheduler_current_vcpu();
	if (vcpu) {
		return vcpu->guest;
	}
	return NULL;
}

void vmm_scheduler_preempt_disable(void)
{
	irq_flags_t flags;
	vmm_vcpu_t * vcpu = vmm_scheduler_current_vcpu();
	if (vcpu) {
		flags = vmm_cpu_irq_save();
		vcpu->preempt_count++;
		vmm_cpu_irq_restore(flags);
	}
}

void vmm_scheduler_preempt_enable(void)
{
	irq_flags_t flags;
	vmm_vcpu_t * vcpu = vmm_scheduler_current_vcpu();
	if (vcpu && vcpu->preempt_count) {
		flags = vmm_cpu_irq_save();
		vcpu->preempt_count--;
		vmm_cpu_irq_restore(flags);
	}
}

u32 vmm_scheduler_vcpu_count(void)
{
	return sched.vcpu_count;
}

vmm_vcpu_t * vmm_scheduler_vcpu(s32 vcpu_no)
{
	if (-1 < vcpu_no && vcpu_no < sched.vcpu_count) {
		return &sched.vcpu_array[vcpu_no];
	}
	return NULL;
}

int vmm_scheduler_vcpu_reset(vmm_vcpu_t * vcpu)
{
	int rc = VMM_EFAIL;
	irq_flags_t flags;
	if (vcpu && vcpu->guest) {
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

int vmm_scheduler_vcpu_kick(vmm_vcpu_t * vcpu)
{
	int rc = VMM_EFAIL;
	irq_flags_t flags;
	if (vcpu && vcpu->guest) {
		flags = vmm_spin_lock_irqsave(&vcpu->lock);
		if (vcpu->state == VMM_VCPU_STATE_RESET) {
			vcpu->state = VMM_VCPU_STATE_READY;
			vcpu->tick_pending = vcpu->tick_count;
			rc = VMM_OK;
		}
		vmm_spin_unlock_irqrestore(&vcpu->lock, flags);
	}
	return rc;
}

int vmm_scheduler_vcpu_pause(vmm_vcpu_t * vcpu)
{
	int rc = VMM_EFAIL;
	irq_flags_t flags;
	if (vcpu && vcpu->guest) {
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

int vmm_scheduler_vcpu_resume(vmm_vcpu_t * vcpu)
{
	int rc = VMM_EFAIL;
	irq_flags_t flags;
	if (vcpu && vcpu->guest) {
		flags = vmm_spin_lock_irqsave(&vcpu->lock);
		if (vcpu->state == VMM_VCPU_STATE_PAUSED) {
			vcpu->state = VMM_VCPU_STATE_READY;
			rc = VMM_OK;
		}
		vmm_spin_unlock_irqrestore(&vcpu->lock, flags);
	}
	return rc;
}

int vmm_scheduler_vcpu_halt(vmm_vcpu_t * vcpu)
{
	int rc = VMM_EFAIL;
	irq_flags_t flags;
	if (vcpu && vcpu->guest) {
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

int vmm_scheduler_vcpu_dumpreg(vmm_vcpu_t * vcpu)
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

vmm_vcpu_t * vmm_scheduler_vcpu_orphan_create(const char *name,
					      virtual_addr_t start_pc,
					      u32 tick_count,
					      vmm_vcpu_tick_t tick_func)
{
	vmm_vcpu_t * vcpu;
	irq_flags_t flags;

	/* Sanity checks */
	if (name == NULL || start_pc == 0 || tick_count == 0) {
		return NULL;
	}

	/* Acquire lock */
	flags = vmm_spin_lock_irqsave(&sched.lock);

	/* Find the next available vcpu */
	vcpu = &sched.vcpu_array[sched.vcpu_count];

	/* Add it to orphan list */
	list_add_tail(&sched.orphan_vcpu_list, &vcpu->head);

	/* Update vcpu attributes */
	INIT_SPIN_LOCK(&vcpu->lock);
	vcpu->index = 0;
	vmm_strcpy(vcpu->name, name);
	vcpu->node = NULL;
	vcpu->state = VMM_VCPU_STATE_READY;
	vcpu->reset_count = 0;
	vcpu->preempt_count = 0;
	vcpu->tick_pending = 0;
	vcpu->tick_count = tick_count;
	vcpu->tick_func = tick_func;
	vcpu->start_pc = start_pc;
	vcpu->bootpg_addr = 0;
	vcpu->bootpg_size = 0;
	vcpu->guest = NULL;

	/* Initialize registers */
	if (vmm_vcpu_regs_init(vcpu)) {
		vcpu = NULL;
	}

	/* Increment vcpu count */
	if (vcpu) {
		sched.vcpu_count++;
	}

	/* Release lock */
	vmm_spin_unlock_irqrestore(&sched.lock, flags);

	return vcpu;
}

int vmm_scheduler_vcpu_orphan_destroy(vmm_vcpu_t * vcpu)
{
	/* FIXME: TBD */
	return VMM_OK;
}

u32 vmm_scheduler_guest_count(void)
{
	return sched.guest_count;
}

vmm_guest_t * vmm_scheduler_guest(s32 guest_no)
{
	if (-1 < guest_no && guest_no < sched.guest_count) {
		return &sched.guest_array[guest_no];
	}
	return NULL;
}

u32 vmm_scheduler_guest_vcpu_count(vmm_guest_t *guest)
{
	if (!guest) {
		return 0;
	}

	return guest->vcpu_count;
}

vmm_vcpu_t * vmm_scheduler_guest_vcpu(vmm_guest_t *guest, int index)
{
	bool found = FALSE;
	vmm_vcpu_t *vcpu = NULL;
	struct dlist *lentry;

	if (!guest || (index < 0)) {
		return NULL;
	}

	list_for_each(lentry, &guest->vcpu_list) {
		vcpu = list_entry(lentry, vmm_vcpu_t, head);
		if (vcpu->index == index) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return NULL;
	}

	return vcpu;
}

int vmm_scheduler_guest_reset(vmm_guest_t * guest)
{
	int rc = VMM_EFAIL;
	struct dlist *lentry;
	vmm_vcpu_t *vcpu;
	if (guest) {
		rc = VMM_OK;
		list_for_each(lentry, &guest->vcpu_list) {
			vcpu = list_entry(lentry, vmm_vcpu_t, head);
			if ((rc = vmm_scheduler_vcpu_reset(vcpu))) {
				break;
			}
		}
		if (!rc) {
			rc = vmm_guest_aspace_reset(guest);
		}
	}
	return rc;
}

int vmm_scheduler_guest_kick(vmm_guest_t * guest)
{
	int rc = VMM_EFAIL;
	struct dlist *lentry;
	vmm_vcpu_t *vcpu;
	if (guest) {
		rc = VMM_OK;
		list_for_each(lentry, &guest->vcpu_list) {
			vcpu = list_entry(lentry, vmm_vcpu_t, head);
			if ((rc = vmm_scheduler_vcpu_kick(vcpu))) {
				break;
			}
		}
	}
	return rc;
}

int vmm_scheduler_guest_pause(vmm_guest_t * guest)
{
	int rc = VMM_EFAIL;
	struct dlist *lentry;
	vmm_vcpu_t *vcpu;
	if (guest) {
		rc = VMM_OK;
		list_for_each(lentry, &guest->vcpu_list) {
			vcpu = list_entry(lentry, vmm_vcpu_t, head);
			if ((rc = vmm_scheduler_vcpu_pause(vcpu))) {
				break;
			}
		}
	}
	return rc;
}

int vmm_scheduler_guest_resume(vmm_guest_t * guest)
{
	int rc = VMM_EFAIL;
	struct dlist *lentry;
	vmm_vcpu_t *vcpu;
	if (guest) {
		rc = VMM_OK;
		list_for_each(lentry, &guest->vcpu_list) {
			vcpu = list_entry(lentry, vmm_vcpu_t, head);
			if ((rc = vmm_scheduler_vcpu_resume(vcpu))) {
				break;
			}
		}
	}
	return rc;
}

int vmm_scheduler_guest_halt(vmm_guest_t * guest)
{
	int rc = VMM_EFAIL;
	struct dlist *lentry;
	vmm_vcpu_t *vcpu;
	if (guest) {
		rc = VMM_OK;
		list_for_each(lentry, &guest->vcpu_list) {
			vcpu = list_entry(lentry, vmm_vcpu_t, head);
			if ((rc = vmm_scheduler_vcpu_halt(vcpu))) {
				break;
			}
		}
	}
	return rc;
}

int vmm_scheduler_guest_dumpreg(vmm_guest_t * guest)
{
	int rc = VMM_EFAIL;
	struct dlist *lentry;
	vmm_vcpu_t *vcpu;
	if (guest) {
		rc = VMM_OK;
		list_for_each(lentry, &guest->vcpu_list) {
			vcpu = list_entry(lentry, vmm_vcpu_t, head);
			if ((rc = vmm_scheduler_vcpu_dumpreg(vcpu))) {
				break;
			}
		}
	}
	return rc;
}

vmm_guest_t * vmm_scheduler_guest_create(vmm_devtree_node_t * gnode)
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
	if (sched.guest_count > sched.max_guest_count) {
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
	guest = &sched.guest_array[sched.guest_count];
	list_add_tail(&sched.guest_list, &guest->head);
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
		if (sched.max_vcpu_count <= sched.vcpu_count) {
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
		vcpu = &sched.vcpu_array[sched.vcpu_count];
		list_add_tail(&guest->vcpu_list, &vcpu->head);
		INIT_SPIN_LOCK(&vcpu->lock);
		vcpu->index = guest->vcpu_count;
		vmm_strcpy(vcpu->name, gnode->name);
		vmm_strcat(vcpu->name,
			   VMM_DEVTREE_PATH_SEPRATOR_STRING);
		vmm_strcat(vcpu->name, vnode->name);
		vcpu->node = vnode;
		vcpu->state = VMM_VCPU_STATE_RESET;
		vcpu->reset_count = 0;
		vcpu->preempt_count = 0;
		vcpu->tick_pending = 0;
		attrval = vmm_devtree_attrval(vnode,
					      VMM_DEVTREE_TICK_COUNT_ATTR_NAME);
		if (attrval) {
			vcpu->tick_count =
			    *((virtual_addr_t *) attrval);
		}
		attrval = vmm_devtree_attrval(vnode,
					      VMM_DEVTREE_START_PC_ATTR_NAME);
		if (attrval) {
			vcpu->start_pc = *((virtual_addr_t *) attrval);
		}
		attrval = vmm_devtree_attrval(vnode,
					      VMM_DEVTREE_BOOTPG_ADDR_ATTR_NAME);
		if (attrval) {
			vcpu->bootpg_addr =
			    *((physical_addr_t *) attrval);
		}
		attrval = vmm_devtree_attrval(vnode,
					      VMM_DEVTREE_BOOTPG_SIZE_ATTR_NAME);
		if (attrval) {
			vcpu->bootpg_size =
			    *((physical_addr_t *) attrval);
		}
		vcpu->guest = guest;
		if (vmm_vcpu_regs_init(vcpu)) {
			continue;
		}
		if (vmm_vcpu_irq_init(vcpu)) {
			continue;
		}

		/* Increment vcpu count */
		sched.vcpu_count++;
		guest->vcpu_count++;
	}

	/* Initialize guest address space */
	if (vmm_guest_aspace_probe(guest)) {
		/* FIXME: Free vcpus alotted to this guest */
		return NULL;
	}

	/* Increment guest count */
	sched.guest_count++;

	return guest;
}

int vmm_scheduler_guest_destroy(vmm_guest_t * guest)
{
	/* FIXME: TBD */
	return VMM_OK;
}

int vmm_scheduler_init(void)
{
	int rc;
	u32 vnum, gnum;
	const char *attrval;
	vmm_devtree_node_t *vnode;

	/* Reset the scheduler control structure */
	vmm_memset(&sched, 0, sizeof(sched));

	/* Initialize scheduling parameters */
	sched.vcpu_current = -1;

	/* Intialize guest & vcpu managment parameters */
	INIT_SPIN_LOCK(&sched.lock);
	sched.max_vcpu_count = 0;
	sched.max_guest_count = 0;
	sched.vcpu_count = 0;
	sched.guest_count = 0;
	sched.vcpu_array = NULL;
	sched.guest_array = NULL;
	INIT_LIST_HEAD(&sched.orphan_vcpu_list);
	INIT_LIST_HEAD(&sched.guest_list);

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
	sched.max_vcpu_count = *((u32 *) attrval);

	/* Get max guest count */
	attrval = vmm_devtree_attrval(vnode,
				      VMM_DEVTREE_MAX_GUEST_COUNT_ATTR_NAME);
	if (!attrval) {
		return VMM_EFAIL;
	}
	sched.max_guest_count = *((u32 *) attrval);

	/* Allocate memory for guest instances */
	sched.guest_array =
	    vmm_malloc(sizeof(vmm_guest_t) * sched.max_guest_count);

	/* Initialze memory for guest instances */
	for (gnum = 0; gnum < sched.max_guest_count; gnum++) {
		vmm_memset(&sched.guest_array[gnum], 0, sizeof(vmm_guest_t));
		INIT_LIST_HEAD(&sched.guest_array[gnum].head);
		INIT_SPIN_LOCK(&sched.guest_array[gnum].lock);
		sched.guest_array[gnum].num = gnum;
		sched.guest_array[gnum].node = NULL;
		INIT_LIST_HEAD(&sched.guest_array[gnum].vcpu_list);
	}

	/* Allocate memory for vcpu instances */
	sched.vcpu_array =
	    vmm_malloc(sizeof(vmm_vcpu_t) * sched.max_vcpu_count);

	/* Initialze memory for vcpu instances */
	for (vnum = 0; vnum < sched.max_vcpu_count; vnum++) {
		vmm_memset(&sched.vcpu_array[vnum], 0, sizeof(vmm_vcpu_t));
		INIT_LIST_HEAD(&sched.vcpu_array[vnum].head);
		INIT_SPIN_LOCK(&sched.vcpu_array[vnum].lock);
		sched.vcpu_array[vnum].num = vnum;
		vmm_strcpy(sched.vcpu_array[vnum].name, "");
		sched.vcpu_array[vnum].node = NULL;
		sched.vcpu_array[vnum].state = VMM_VCPU_STATE_UNKNOWN;
	}

	/* Register ticker per Host CPU */
	vmm_strcpy(sched.tk.name, "sched");
	sched.tk.enabled = TRUE;
	sched.tk.hndl = &vmm_scheduler_tick;
	if ((rc = vmm_timer_register_ticker(&sched.tk))) {
		return rc;
	}

	return VMM_OK;
}
