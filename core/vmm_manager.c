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
#include <vmm_timer.h>
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

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)		vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

/** Control structure for Scheduler */
struct vmm_manager_ctrl {
	vmm_spinlock_t lock;
	u32 vcpu_count;
	u32 guest_count;
	struct vmm_cpumask vcpu_affinity_mask[CONFIG_MAX_VCPU_COUNT];
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

static void manager_vcpu_ipi_reset(void *vcpu_ptr, 
				   void *dummy1, void *dummy2)
{
	vmm_scheduler_state_change(vcpu_ptr, VMM_VCPU_STATE_RESET);
}

int vmm_manager_vcpu_iterate(int (*iter)(struct vmm_vcpu *, void *), 
			     void *priv)
{
	int rc, v;
	struct vmm_vcpu *vcpu;
	irq_flags_t flags;

	/* If no iteration callback then return */
	if (!iter) {
		return VMM_EINVALID;
	}

	/* Iterate over each used VCPU instance */
	rc = VMM_OK;
	for (v = 0; v < CONFIG_MAX_VCPU_COUNT; v++) {		
		vmm_spin_lock_irqsave_lite(&mngr.lock, flags);
		if (mngr.vcpu_avail_array[v]) {
			vmm_spin_unlock_irqrestore_lite(&mngr.lock, flags);
			continue;
		}
		vcpu = &mngr.vcpu_array[v];
		vmm_spin_unlock_irqrestore_lite(&mngr.lock, flags);

		rc = iter(vcpu, priv);
		if (rc) {
			break;
		}
	}

	return rc;
}

int vmm_manager_vcpu_stats(struct vmm_vcpu *vcpu,
			   u32 *state,
			   u8  *priority,
			   u32 *hcpu,
			   u32 *reset_count,
			   u64 *last_reset_nsecs,
			   u64 *ready_nsecs,
			   u64 *running_nsecs,
			   u64 *paused_nsecs,
			   u64 *halted_nsecs)
{
	irq_flags_t flags;
	u64 current_tstamp;
	u32 current_state;

	if (!vcpu) {
		return VMM_EFAIL;
	}

	/* Current timestamp */
	current_tstamp = vmm_timer_timestamp();

	/* Acquire scheduling lock */
	vmm_write_lock_irqsave_lite(&vcpu->sched_lock, flags);

	current_state = arch_atomic_read(&vcpu->state);

	/* Retrive current state and current hcpu */
	if (state) {
		*state = current_state;
	}
	if (priority) {
		*priority = vcpu->priority;
	}
	if (hcpu) {
		*hcpu = vcpu->hcpu;
	}

	/* Syncup statistics based on current timestamp */
	switch (current_state) {
	case VMM_VCPU_STATE_READY:
		vcpu->state_ready_nsecs += 
				current_tstamp - vcpu->state_tstamp;
		vcpu->state_tstamp = current_tstamp;
		break;
	case VMM_VCPU_STATE_RUNNING:
		vcpu->state_running_nsecs += 
				current_tstamp - vcpu->state_tstamp;
		vcpu->state_tstamp = current_tstamp;
		break;
	case VMM_VCPU_STATE_PAUSED:
		vcpu->state_paused_nsecs += 
				current_tstamp - vcpu->state_tstamp;
		vcpu->state_tstamp = current_tstamp;
		break;
	case VMM_VCPU_STATE_HALTED:
		vcpu->state_halted_nsecs += 
				current_tstamp - vcpu->state_tstamp;
		vcpu->state_tstamp = current_tstamp;
		break;
	default:
		break; 
	}

	/* Retrive statistics */
	if (reset_count) {
		*reset_count = vcpu->reset_count;
	}
	if (last_reset_nsecs) {
		*last_reset_nsecs = current_tstamp - vcpu->reset_tstamp;
	}
	if (ready_nsecs) {
		*ready_nsecs = vcpu->state_ready_nsecs;
	}
	if (running_nsecs) {
		*running_nsecs = vcpu->state_running_nsecs;
	}
	if (paused_nsecs) {
		*paused_nsecs = vcpu->state_paused_nsecs;
	}
	if (halted_nsecs) {
		*halted_nsecs = vcpu->state_halted_nsecs;
	}

	/* Release scheduling lock */
	vmm_write_unlock_irqrestore_lite(&vcpu->sched_lock, flags);

	return VMM_OK;
}

u32 vmm_manager_vcpu_get_state(struct vmm_vcpu *vcpu)
{
	if (!vcpu) {
		return VMM_VCPU_STATE_UNKNOWN;
	}

	return (u32)arch_atomic_read(&vcpu->state);
}

int vmm_manager_vcpu_set_state(struct vmm_vcpu *vcpu, u32 new_state)
{
	u32 vhcpu;
	irq_flags_t flags;

	if (!vcpu) {
		return VMM_EFAIL;
	}

	/* If new_state == VMM_VCPU_STATE_RESET then 
	 * we use sync IPI for proper working of VCPU reset.
	 * 
	 * For all other states we can directly call 
	 * scheduler state change
	 */

	if (new_state == VMM_VCPU_STATE_RESET) {
		vmm_read_lock_irqsave_lite(&vcpu->sched_lock, flags);
		vhcpu = vcpu->hcpu;
		vmm_read_unlock_irqrestore_lite(&vcpu->sched_lock, flags);
		return vmm_smp_ipi_sync_call(vmm_cpumask_of(vhcpu), 1000,
					     manager_vcpu_ipi_reset,
					     vcpu, NULL, NULL);
	}

	return vmm_scheduler_state_change(vcpu, new_state);
}

int vmm_manager_vcpu_get_hcpu(struct vmm_vcpu *vcpu, u32 *hcpu)
{
	irq_flags_t flags;

	if ((vcpu == NULL) || (hcpu == NULL)) {
		return VMM_EFAIL;
	}

	vmm_read_lock_irqsave_lite(&vcpu->sched_lock, flags);
	*hcpu = vcpu->hcpu;
	vmm_read_unlock_irqrestore_lite(&vcpu->sched_lock, flags);

	return VMM_OK;
}

static void manager_vcpu_movto_hcpu(void *vcpu_ptr, 
				    void *new_hcpu, void *dummy)
{
	int rc;
	irq_flags_t flags;
	u32 hcpu = (u32)(virtual_addr_t)new_hcpu;
	struct vmm_vcpu *vcpu = vcpu_ptr;

	rc = vmm_scheduler_state_change(vcpu, VMM_VCPU_STATE_PAUSED);
	if (rc) {
		DPRINTF("%s: Failed to pause VCPU=%s on CPU%d (%d)\n", 
			__func__, vcpu->name, vmm_smp_processor_id(), rc);
		return;
	}

	vmm_write_lock_irqsave_lite(&vcpu->sched_lock, flags);
	vcpu->hcpu = hcpu;
	vmm_write_unlock_irqrestore_lite(&vcpu->sched_lock, flags);

	rc = vmm_scheduler_state_change(vcpu, VMM_VCPU_STATE_READY);
	if (rc) {
		DPRINTF("%s: Failed to resume VCPU=%s on CPU%d (%d)\n",
			__func__, vcpu->name, vmm_smp_processor_id(), rc);
		return;
	}
}

int vmm_manager_vcpu_set_hcpu(struct vmm_vcpu *vcpu, u32 hcpu)
{
	u32 old_hcpu;
	irq_flags_t flags;
	u32 state;

	if (!vcpu) {
		return VMM_EFAIL;
	}

	/* Lock VCPU scheduling */
	vmm_write_lock_irqsave_lite(&vcpu->sched_lock, flags);

	/* Current hcpu */
	old_hcpu = vcpu->hcpu;

	/* If hcpu not changing then do nothing */
	if (old_hcpu == hcpu) {
		vmm_write_unlock_irqrestore_lite(&vcpu->sched_lock, flags);
		return VMM_OK;
	}

	/* Match affinity with new hcpu */
	if (!vmm_cpumask_test_cpu(hcpu, vcpu->cpu_affinity)) {
		vmm_write_unlock_irqrestore_lite(&vcpu->sched_lock, flags);
		return VMM_EINVALID;
	}

	state = arch_atomic_read(&vcpu->state);

	/* Check if we don't need to migrate VCPU to new hcpu */
	if ((state != VMM_VCPU_STATE_READY) &&
	    (state != VMM_VCPU_STATE_RUNNING)) {
		vcpu->hcpu = hcpu;
		vmm_write_unlock_irqrestore_lite(&vcpu->sched_lock, flags);
		return VMM_OK;
	}

	/* Unlock VCPU scheduling */
	vmm_write_unlock_irqrestore_lite(&vcpu->sched_lock, flags);

	/* Try to migrate running/ready VCPU to new host CPU */
	vmm_smp_ipi_async_call(vmm_cpumask_of(old_hcpu),
			       manager_vcpu_movto_hcpu,
			       vcpu, (void *)(virtual_addr_t)hcpu,
			       NULL);

	return VMM_OK;
}

const struct vmm_cpumask *vmm_manager_vcpu_get_affinity(struct vmm_vcpu *vcpu)
{
	irq_flags_t flags;
	const struct vmm_cpumask *cpu_mask = NULL;

	if (!vcpu) {
		return NULL;
	}

	vmm_read_lock_irqsave_lite(&vcpu->sched_lock, flags);
	cpu_mask = vcpu->cpu_affinity;
	vmm_read_unlock_irqrestore_lite(&vcpu->sched_lock, flags);

	return cpu_mask;
}

int vmm_manager_vcpu_set_affinity(struct vmm_vcpu *vcpu, 
				  const struct vmm_cpumask *cpu_mask)
{
	irq_flags_t flags;

	if (!vcpu || !cpu_mask) {
		return VMM_EFAIL;
	}

	/* Lock load balancing */
	vmm_write_lock_irqsave_lite(&vcpu->sched_lock, flags);

	/* Match new affinity with current hcpu */
	if (!vmm_cpumask_test_cpu(vcpu->hcpu, cpu_mask)) {
		vmm_write_unlock_irqrestore_lite(&vcpu->sched_lock, flags);
		return VMM_EINVALID;
	}

	/* Update affinity */
	vcpu->cpu_affinity = cpu_mask;

	/* Unlock load balancing */
	vmm_write_unlock_irqrestore_lite(&vcpu->sched_lock, flags);

	return VMM_OK;
}

struct vmm_vcpu *vmm_manager_vcpu_orphan_create(const char *name,
					    virtual_addr_t start_pc,
					    virtual_size_t stack_sz,
					    u8 priority,
					    u64 time_slice_nsecs)
{
	u32 vnum;
	struct vmm_vcpu *vcpu = NULL;
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
	vcpu->is_poweroff = FALSE;
	vcpu->guest = NULL;

	/* Setup start program counter and stack */
	vcpu->start_pc = start_pc;
	vcpu->stack_va = (virtual_addr_t)vmm_malloc(stack_sz);
	if (!vcpu->stack_va) {
		vcpu = NULL;
		goto release_lock;
	}
	vcpu->stack_sz = stack_sz;

	/* Intialize scheduling context */
	INIT_RW_LOCK(&vcpu->sched_lock);
	vcpu->hcpu = vmm_smp_processor_id();
	vcpu->cpu_affinity = vmm_cpumask_of(vcpu->hcpu);
	arch_atomic_write(&vcpu->state, VMM_VCPU_STATE_UNKNOWN);
	vcpu->state_tstamp = vmm_timer_timestamp();
	vcpu->state_ready_nsecs = 0;
	vcpu->state_running_nsecs = 0;
	vcpu->state_paused_nsecs = 0;
	vcpu->state_halted_nsecs = 0;
	vcpu->reset_count = 0;
	vcpu->reset_tstamp = 0;
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
	if (vmm_manager_vcpu_set_state(vcpu, 
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
	if ((rc = vmm_manager_vcpu_set_state(vcpu, 
					VMM_VCPU_STATE_RESET))) {
		return rc;
	}

	/* Acquire lock */
	vmm_spin_lock_irqsave_lite(&mngr.lock, flags);

	/* Decrement vcpu count */
	mngr.vcpu_count--;

	/* Remove VCPU from orphan list */
	list_del(&vcpu->head);

	/* Notify scheduler about VCPU state change */
	if ((rc = vmm_manager_vcpu_set_state(vcpu, 
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

struct vmm_guest *vmm_manager_guest_find(const char *guest_name)
{
	u32 g;
	irq_flags_t flags;
	struct vmm_guest *ret;

	if (!guest_name) {
		return NULL;
	}

	/* Acquire manager lock */
	vmm_spin_lock_irqsave_lite(&mngr.lock, flags);

	/* Iterate over each used VCPU instance */
	ret = NULL;
	for (g = 0; g < CONFIG_MAX_GUEST_COUNT; g++) {		
		if (!mngr.guest_avail_array[g]) {
			if (!strcmp(mngr.guest_array[g].name, guest_name)) {
				ret = &mngr.guest_array[g];
				break;
			}
		}
	}

	/* Release manager lock */
	vmm_spin_unlock_irqrestore_lite(&mngr.lock, flags);

	return ret;	
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
	irq_flags_t flags;
	struct vmm_vcpu *vcpu = NULL;

	if (!guest) {
		return NULL;
	}

	vmm_read_lock_irqsave_lite(&guest->vcpu_lock, flags);

	list_for_each_entry(vcpu, &guest->vcpu_list, head) {
		if (vcpu->subid == subid) {
			found = TRUE;
			break;
		}
	}

	vmm_read_unlock_irqrestore_lite(&guest->vcpu_lock, flags);

	if (!found) {
		vcpu = NULL;
	}

	return vcpu;
}

int vmm_manager_guest_vcpu_iterate(struct vmm_guest *guest,
				   int (*iter)(struct vmm_vcpu *, void *), 
				   void *priv)
{
	int rc;
	irq_flags_t flags;
	struct vmm_vcpu *vcpu;

	if (!guest || !iter) {
		return VMM_EFAIL;
	}

	vmm_read_lock_irqsave_lite(&guest->vcpu_lock, flags);

	list_for_each_entry(vcpu, &guest->vcpu_list, head) {
		rc = iter(vcpu, priv);
		if (rc) {
			break;
		}
	}

	vmm_read_unlock_irqrestore_lite(&guest->vcpu_lock, flags);

	return rc;
}

static int manager_guest_reset_iter(struct vmm_vcpu *vcpu, void *priv)
{
	return vmm_manager_vcpu_reset(vcpu);
}

int vmm_manager_guest_reset(struct vmm_guest *guest)
{
	int rc;

	if (!guest) {
		return VMM_EFAIL;
	}

	rc = vmm_manager_guest_vcpu_iterate(guest, 
				manager_guest_reset_iter, NULL);
	if (rc) {
		return rc;
	}

	if (!(rc = vmm_guest_aspace_reset(guest))) {
		guest->reset_count++;
		rc = arch_guest_init(guest);
	}

	return rc;
}

static int manager_guest_kick_iter(struct vmm_vcpu *vcpu, void *priv)
{
	/* Do not kick VCPU with poweroff flag set 
	 * when Guest is kicked.
	 */
	if (vcpu->is_poweroff) {
		return VMM_OK;
	}
	return vmm_manager_vcpu_kick(vcpu);
}

int vmm_manager_guest_kick(struct vmm_guest *guest)
{
	return vmm_manager_guest_vcpu_iterate(guest, 
					manager_guest_kick_iter, NULL);
}

static int manager_guest_pause_iter(struct vmm_vcpu *vcpu, void *priv)
{
	return vmm_manager_vcpu_pause(vcpu);
}

int vmm_manager_guest_pause(struct vmm_guest *guest)
{
	return vmm_manager_guest_vcpu_iterate(guest, 
					manager_guest_pause_iter, NULL);
}

static int manager_guest_resume_iter(struct vmm_vcpu *vcpu, void *priv)
{
	return vmm_manager_vcpu_resume(vcpu);
}

int vmm_manager_guest_resume(struct vmm_guest *guest)
{
	return vmm_manager_guest_vcpu_iterate(guest, 
					manager_guest_resume_iter, NULL);
}

static int manager_guest_halt_iter(struct vmm_vcpu *vcpu, void *priv)
{
	return vmm_manager_vcpu_halt(vcpu);
}

int vmm_manager_guest_halt(struct vmm_guest *guest)
{
	return vmm_manager_guest_vcpu_iterate(guest,
					manager_guest_halt_iter, NULL);
}

struct vmm_guest *vmm_manager_guest_create(struct vmm_devtree_node *gnode)
{
	u32 val, vnum, gnum;
	const char *str;
	irq_flags_t flags, flags1;
	struct vmm_devtree_node *vsnode;
	struct vmm_devtree_node *vnode;
	struct vmm_guest *guest = NULL;
	struct vmm_vcpu *vcpu = NULL;

	/* Sanity checks */
	if (!gnode) {
		return NULL;
	}
	if (vmm_devtree_read_string(gnode,
				VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME, &str)) {
		return NULL;
	}
	if (strcmp(str, VMM_DEVTREE_DEVICE_TYPE_VAL_GUEST) != 0) {
		return NULL;
	}

	/* Acquire manager lock */
	vmm_spin_lock_irqsave_lite(&mngr.lock, flags);

	/* Ensure guest node uniqueness */
	list_for_each_entry(guest, &mngr.guest_list, head) {
		if ((guest->node == gnode) ||
		    (strcmp(guest->name, gnode->name) == 0)) {
			vmm_spin_unlock_irqrestore_lite(&mngr.lock, flags);
			vmm_printf("%s: Duplicate Guest %s detected\n", 
					__func__, gnode->name);
			return NULL;
		}
	}

	/* Find next available guest instance */
	for (gnum = 0; gnum < CONFIG_MAX_GUEST_COUNT; gnum++) {
		if (mngr.guest_avail_array[gnum]) {
			guest = &mngr.guest_array[gnum];
			break;
		}
	}

	if (!guest) {
		vmm_spin_unlock_irqrestore_lite(&mngr.lock, flags);
		vmm_printf("%s: No available Guest instance found\n", 
			   __func__);
		return NULL;
	}

	/* Add guest instance to guest list */
	list_add_tail(&guest->head, &mngr.guest_list);

	/* Mark this guest instance as not available */
	mngr.guest_avail_array[guest->id] = FALSE;

	/* Increment guest count */
	mngr.guest_count++;

	/* Initialize guest instance */
	strlcpy(guest->name, gnode->name, sizeof(guest->name));
	guest->node = gnode;
#ifdef CONFIG_CPU_BE
	guest->is_big_endian = TRUE;
#else
	guest->is_big_endian = FALSE;
#endif
	guest->reset_count = 0;
	INIT_RW_LOCK(&guest->vcpu_lock);
	guest->vcpu_count = 0;
	INIT_LIST_HEAD(&guest->vcpu_list);
	memset(&guest->aspace, 0, sizeof(guest->aspace));
	guest->aspace.initialized = FALSE;
	INIT_RW_LOCK(&guest->aspace.reg_list_lock);
	INIT_LIST_HEAD(&guest->aspace.reg_list);
	guest->arch_priv = NULL;

	/* Determine guest endianness from guest node */
	if (vmm_devtree_read_string(gnode,
			VMM_DEVTREE_ENDIANNESS_ATTR_NAME, &str) == VMM_OK) {
		if (!strcmp(str, VMM_DEVTREE_ENDIANNESS_VAL_LITTLE)) {
			guest->is_big_endian = FALSE;
		} else if (!strcmp(str, VMM_DEVTREE_ENDIANNESS_VAL_BIG)) {
			guest->is_big_endian = TRUE;
		}
	}

	vsnode = vmm_devtree_getchild(gnode, VMM_DEVTREE_VCPUS_NODE_NAME);
	if (!vsnode) {
		vmm_printf("%s: vcpus node not found for Guest %s\n",
			   __func__, gnode->name);
		goto fail_release_lock;
	}

	list_for_each_entry(vnode, &vsnode->child_list, head) {
		/* Sanity checks */
		if (CONFIG_MAX_VCPU_COUNT <= mngr.vcpu_count) {
			vmm_printf("%s: No more free VCPUs\n"
				   "for Guest %s VCPU %s\n",
				   __func__, gnode->name, vnode->name);
			goto fail_release_lock;
		}
		if (vmm_devtree_read_string(vnode,
				VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME, &str)) {
			vmm_printf("%s: No device_type attribute\n"
				   "for Guest %s VCPU %s\n",
				   __func__, gnode->name, vnode->name);
			goto fail_release_lock;
		}
		if (strcmp(str, VMM_DEVTREE_DEVICE_TYPE_VAL_VCPU) != 0) {
			vmm_printf("%s: Invalid device_type attribute\n"
				   "for Guest %s VCPU %s\n",
				   __func__, gnode->name, vnode->name);
			goto fail_release_lock;
		}

		/* Find next available VCPU instance */
		vcpu = NULL;
		for (vnum = 0; vnum < CONFIG_MAX_VCPU_COUNT; vnum++) {
			if (mngr.vcpu_avail_array[vnum]) {
				vcpu = &mngr.vcpu_array[vnum];
				break;
			}
		}
		if (!vcpu) {
			vmm_printf("%s: No available VCPU instance found \n"
				   "for Guest %s VCPU %s\n",
				   __func__, gnode->name, vnode->name);
			goto fail_release_lock;
		}

		/* Mark this VCPU instance as not available */
		mngr.vcpu_avail_array[vcpu->id] = FALSE;

		/* Initialize general info */
		vcpu->subid = guest->vcpu_count;
		strlcpy(vcpu->name, gnode->name, sizeof(vcpu->name));
		strlcat(vcpu->name, VMM_DEVTREE_PATH_SEPARATOR_STRING,
			sizeof(vcpu->name));
		if (strlcat(vcpu->name, vnode->name, sizeof(vcpu->name)) >=
		    sizeof(vcpu->name)) {
			vmm_printf("%s: name concatination failed "
				   "for Guest %s VCPU %s\n",
				   __func__, gnode->name, vnode->name);
			mngr.vcpu_avail_array[vcpu->id] = TRUE;
			goto fail_release_lock;
		}
		vcpu->node = vnode;
		vcpu->is_normal = TRUE;
		vcpu->is_poweroff = FALSE;
		vcpu->guest = guest;

		/* Setup start program counter and stack */
		vmm_devtree_read_virtaddr(vnode,
			VMM_DEVTREE_START_PC_ATTR_NAME, &vcpu->start_pc);
		vcpu->stack_va = 
			(virtual_addr_t)vmm_malloc(CONFIG_IRQ_STACK_SIZE);
		if (!vcpu->stack_va) {
			vmm_printf("%s: stack alloc failed "
				   "for VCPU %s\n", __func__, vcpu->name);
			mngr.vcpu_avail_array[vcpu->id] = TRUE;
			goto fail_release_lock;
		}
		vcpu->stack_sz = CONFIG_IRQ_STACK_SIZE;

		/* Initialize scheduling context */
		INIT_RW_LOCK(&vcpu->sched_lock);
		vcpu->hcpu = vmm_smp_processor_id();
		vcpu->cpu_affinity = vmm_cpumask_of(vcpu->hcpu);
		arch_atomic_write(&vcpu->state, VMM_VCPU_STATE_UNKNOWN);
		vcpu->state_tstamp = vmm_timer_timestamp();
		vcpu->state_ready_nsecs = 0;
		vcpu->state_running_nsecs = 0;
		vcpu->state_paused_nsecs = 0;
		vcpu->state_halted_nsecs = 0;
		vcpu->reset_count = 0;
		vcpu->reset_tstamp = 0;
		vcpu->preempt_count = 0;
		if (vmm_devtree_read_u32(vnode,
			VMM_DEVTREE_PRIORITY_ATTR_NAME, &val)) {
			vcpu->priority = VMM_VCPU_DEF_PRIORITY;
		} else {
			vcpu->priority = val;
		}
		if (VMM_VCPU_MAX_PRIORITY < vcpu->priority) {
			vcpu->priority = VMM_VCPU_MAX_PRIORITY;
		}
		if (vmm_devtree_read_u64(vnode,
			VMM_DEVTREE_TIME_SLICE_ATTR_NAME, &vcpu->time_slice)) {
			vcpu->time_slice = VMM_VCPU_DEF_TIME_SLICE;
		}
		vcpu->sched_priv = NULL;

		/* Initialize architecture specific context */
		vcpu->arch_priv = NULL;
		if (arch_vcpu_init(vcpu)) {
			vmm_free((void *)vcpu->stack_va);
			vmm_printf("%s: arch_vcpu_init() failed "
				   "for VCPU %s\n", __func__, vcpu->name);
			mngr.vcpu_avail_array[vcpu->id] = TRUE;
			goto fail_release_lock;
		}

		/* Initialize virtual IRQ context */
		if (vmm_vcpu_irq_init(vcpu)) {
			arch_vcpu_deinit(vcpu);
			vmm_free((void *)vcpu->stack_va);
			vmm_printf("%s: vmm_vcpu_irq_init() failed "
				   "for VCPU %s\n", __func__, vcpu->name);
			mngr.vcpu_avail_array[vcpu->id] = TRUE;
			goto fail_release_lock;
		}

		/* Initialize waitqueue context */
		INIT_LIST_HEAD(&vcpu->wq_head);
		vcpu->wq_priv = NULL;

		/* Initialize device emulation context */
		vcpu->devemu_priv = NULL;

		/* Notify scheduler about new VCPU */
		if (vmm_manager_vcpu_set_state(vcpu, VMM_VCPU_STATE_RESET)) {
			vmm_vcpu_irq_deinit(vcpu);
			arch_vcpu_deinit(vcpu);
			vmm_free((void *)vcpu->stack_va);
			vmm_printf("%s: Setting RESET state failed "
				   "for VCPU %s\n", __func__, vcpu->name);
			mngr.vcpu_avail_array[vcpu->id] = TRUE;
			goto fail_release_lock;
		}

		/* Setup VCPU affinity mask */
		if (vmm_devtree_getattr(vnode,
				VMM_DEVTREE_VCPU_AFFINITY_ATTR_NAME)) {
			int index;
			u32 cpu;
			struct vmm_cpumask *affinity_mask =
					&mngr.vcpu_affinity_mask[vcpu->id];

			/* Start with empty affinity mask */
			*affinity_mask = VMM_CPU_MASK_NONE;

			/* Set all assigned CPU in the mask */
			index = 0;
			while (vmm_devtree_read_u32_atindex(vnode,
				VMM_DEVTREE_VCPU_AFFINITY_ATTR_NAME,
				&cpu, index) == VMM_OK) {
				if ((cpu < CONFIG_CPU_COUNT) &&
				    vmm_cpu_online(cpu)) {
					vmm_cpumask_set_cpu(cpu,
							    affinity_mask);
				} else {
					vmm_manager_vcpu_set_state(vcpu,
						VMM_VCPU_STATE_UNKNOWN);
					vmm_vcpu_irq_deinit(vcpu);
					arch_vcpu_deinit(vcpu);
					vmm_free((void *)vcpu->stack_va);
					vmm_printf(
						"%s: CPU%d is out of bound"
						" (%d <) or not online for"
						" %s\n", __func__, cpu,
						CONFIG_CPU_COUNT, vcpu->name);
					mngr.vcpu_avail_array[vcpu->id] = TRUE;
					goto fail_release_lock;
				}
				index++;
			}

			/* Set hcpu as the first CPU in the mask */
			vcpu->hcpu = vmm_cpumask_first(affinity_mask);
			if (vcpu->hcpu > CONFIG_CPU_COUNT) {
				vmm_manager_vcpu_set_state(vcpu,
						VMM_VCPU_STATE_UNKNOWN);
				vmm_vcpu_irq_deinit(vcpu);
				arch_vcpu_deinit(vcpu);
				vmm_free((void *)vcpu->stack_va);
				vmm_printf(
					"%s: Can't find a valid CPU for"
					" vcpu %s\n", __func__, vcpu->name);
				mngr.vcpu_avail_array[vcpu->id] = TRUE;
				goto fail_release_lock;
			}

			/* Set the affinity mask */
			vmm_manager_vcpu_set_affinity(vcpu, affinity_mask);
		} else {
			vmm_manager_vcpu_set_affinity(vcpu, cpu_online_mask);
		}

		/* Get poweroff flag from device tree */
		if (vmm_devtree_getattr(vnode,
				VMM_DEVTREE_VCPU_POWEROFF_ATTR_NAME)) {
			vcpu->is_poweroff = TRUE;
		}

		/* Increment VCPU count */
		mngr.vcpu_count++;

		/* Add VCPU to Guest child list */
		vmm_write_lock_irqsave_lite(&guest->vcpu_lock, flags1);
		guest->vcpu_count++;
		list_add_tail(&vcpu->head, &guest->vcpu_list);
		vmm_write_unlock_irqrestore_lite(&guest->vcpu_lock, flags1);
	}

	/* Fail if no VCPU is associated to the guest */
	if (list_empty(&guest->vcpu_list)) {
		goto fail_release_lock;
	}

	/* Release manager lock */
	vmm_spin_unlock_irqrestore_lite(&mngr.lock, flags);

	/* Initialize guest address space */
	if (vmm_guest_aspace_init(guest)) {
		goto fail_destroy_guest;
	}

	/* Reset guest address space */
	if (vmm_guest_aspace_reset(guest)) {
		goto fail_destroy_guest;
	}

	/* Initialize arch guest context */
	if (arch_guest_init(guest)) {
		goto fail_destroy_guest;
	}

	return guest;

fail_release_lock:
	vmm_spin_unlock_irqrestore_lite(&mngr.lock, flags);
fail_destroy_guest:
	vmm_manager_guest_destroy(guest);

	return NULL;
}

int vmm_manager_guest_destroy(struct vmm_guest *guest)
{
	int rc;
	irq_flags_t flags, flags1;
	struct vmm_vcpu *vcpu;

	/* Sanity Check */
	if (!guest) {
		return VMM_EFAIL;
	}

	/* For sanity reset guest (ignore reture value) */
	vmm_manager_guest_reset(guest);

	/* Deinit arch guest context */
	if ((rc = arch_guest_deinit(guest))) {
		goto release_lock;
	}

	/* Deinit the guest aspace */
	if ((rc = vmm_guest_aspace_deinit(guest))) {
		goto release_lock;
	}

	/* Acquire manager lock */
	vmm_spin_lock_irqsave_lite(&mngr.lock, flags);

	/* Acquire Guest VCPU lock */
	vmm_write_lock_irqsave_lite(&guest->vcpu_lock, flags1);

	/* Destroy each VCPU of guest */
	while (!list_empty(&guest->vcpu_list)) {
		vcpu = list_first_entry(&guest->vcpu_list,
					struct vmm_vcpu, head);
		list_del(&vcpu->head);

		/* Decrement vcpu count */
		mngr.vcpu_count--;

		/* Release Guest VCPU lock */
		vmm_write_unlock_irqrestore_lite(&guest->vcpu_lock, flags1);

		/* Notify scheduler about VCPU state change */
		if ((rc = vmm_manager_vcpu_set_state(vcpu, 
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

		/* Acquire Guest VCPU lock */
		vmm_write_lock_irqsave_lite(&guest->vcpu_lock, flags1);
	}

	/* Release Guest VCPU lock */
	vmm_write_unlock_irqrestore_lite(&guest->vcpu_lock, flags1);

	/* Reset guest instance members */
	guest->node = NULL;
	guest->name[0] = '\0';
	INIT_LIST_HEAD(&guest->vcpu_list);

	/* Decrement guest count */
	mngr.guest_count--;

	/* Mark this guest instance as available */
	mngr.guest_avail_array[guest->id] = TRUE;

	/* Remove from guest list */
	list_del(&guest->head);
	INIT_LIST_HEAD(&guest->head);

release_lock:
	/* Release manager lock */
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
		mngr.guest_array[gnum].id = gnum;
		mngr.guest_array[gnum].node = NULL;
		INIT_RW_LOCK(&mngr.guest_array[gnum].vcpu_lock);
		mngr.guest_array[gnum].vcpu_count = 0;
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
		arch_atomic_write(&mngr.vcpu_array[vnum].state, VMM_VCPU_STATE_UNKNOWN);
		mngr.vcpu_array[vnum].state_tstamp = 0;
		mngr.vcpu_array[vnum].state_ready_nsecs = 0;
		mngr.vcpu_array[vnum].state_running_nsecs = 0;
		mngr.vcpu_array[vnum].state_paused_nsecs = 0;
		mngr.vcpu_array[vnum].state_halted_nsecs = 0;
		mngr.vcpu_array[vnum].reset_count = 0;
		mngr.vcpu_array[vnum].reset_tstamp = 0;
		INIT_RW_LOCK(&mngr.vcpu_array[vnum].sched_lock);
		mngr.vcpu_avail_array[vnum] = TRUE;
	}

	return VMM_OK;
}
