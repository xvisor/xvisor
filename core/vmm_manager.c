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
#include <vmm_compiler.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_timer.h>
#include <vmm_guest_aspace.h>
#include <vmm_vcpu_irq.h>
#include <vmm_scheduler.h>
#include <vmm_waitqueue.h>
#include <vmm_workqueue.h>
#include <vmm_manager.h>
#include <arch_vcpu.h>
#include <arch_guest.h>
#include <libs/stringlib.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)		vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

/** Control structure for manager */
struct vmm_manager_ctrl {
	/* Guest & VCPU management */
	struct vmm_mutex lock;
	u32 vcpu_count;
	u32 guest_count;
	struct vmm_cpumask vcpu_affinity_mask[CONFIG_MAX_VCPU_COUNT];
	struct vmm_vcpu vcpu_array[CONFIG_MAX_VCPU_COUNT];
	bool vcpu_avail_array[CONFIG_MAX_VCPU_COUNT];
	struct vmm_guest guest_array[CONFIG_MAX_GUEST_COUNT];
	bool guest_avail_array[CONFIG_MAX_GUEST_COUNT];
	struct dlist orphan_vcpu_list;
	struct dlist guest_list;
	/* Work structs to process guest request */
	struct vmm_work guest_work_array[CONFIG_MAX_GUEST_COUNT];
};

static struct vmm_manager_ctrl mngr;

void vmm_manager_lock(void)
{
	vmm_mutex_lock(&mngr.lock);
}

void vmm_manager_unlock(void)
{
	vmm_mutex_unlock(&mngr.lock);
}

u32 vmm_manager_max_vcpu_count(void)
{
	return CONFIG_MAX_VCPU_COUNT;
}

u32 vmm_manager_vcpu_count(void)
{
	u32 ret;

	vmm_manager_lock();
	ret = mngr.vcpu_count;
	vmm_manager_unlock();

	return ret;
}

struct vmm_vcpu *vmm_manager_vcpu(u32 vcpu_id)
{
	struct vmm_vcpu *ret = NULL;

	if (vcpu_id < CONFIG_MAX_VCPU_COUNT) {
		vmm_manager_lock();
		if (!mngr.vcpu_avail_array[vcpu_id]) {
			ret = &mngr.vcpu_array[vcpu_id];
		}
		vmm_manager_unlock();
	}

	return ret;
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

	/* If no iteration callback then return */
	if (!iter) {
		return VMM_EINVALID;
	}

	/* Acquire manager lock */
	vmm_manager_lock();

	/* Iterate over each used VCPU instance */
	rc = VMM_OK;
	for (v = 0; v < CONFIG_MAX_VCPU_COUNT; v++) {
		if (mngr.vcpu_avail_array[v]) {
			continue;
		}
		vcpu = &mngr.vcpu_array[v];

		rc = iter(vcpu, priv);
		if (rc) {
			break;
		}
	}

	/* Release manager lock */
	vmm_manager_unlock();

	return rc;
}

/* Note: Must be called with manager lock held */
static u32 __vmm_manager_good_hcpu(u8 priority,
				   const struct vmm_cpumask *affinity)
{
	struct vmm_vcpu *vcpu;
	u32 count[CONFIG_CPU_COUNT];
	u32 v, c, min, hcpu = vmm_cpumask_first(affinity);

	if (!vmm_timer_started() ||
	    (vmm_cpumask_weight(affinity) < 1)) {
		return vmm_smp_processor_id();
	}

	for (c = 0; c < CONFIG_CPU_COUNT; c++) {
		count[c] = 0;
	}

	for (v = 0; v < CONFIG_MAX_VCPU_COUNT; v++) {
		if (mngr.vcpu_avail_array[v]) {
			continue;
		}
		vcpu = &mngr.vcpu_array[v];

		if ((vcpu->priority != priority) ||
		    !vmm_cpumask_test_cpu(vcpu->hcpu, affinity)) {
			continue;
		}

		count[vcpu->hcpu]++;
	}

	min = count[hcpu];
	for (c = 0; c < CONFIG_CPU_COUNT; c++) {
		if (!vmm_cpumask_test_cpu(c, affinity)) {
			continue;
		}
		if (count[c] < min) {
			min = count[c];
			hcpu = c;
		}
	}

	return hcpu;
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
	return vmm_scheduler_get_hcpu(vcpu, hcpu);
}

bool vmm_manager_vcpu_check_current_hcpu(struct vmm_vcpu *vcpu)
{
	return vmm_scheduler_check_current_hcpu(vcpu);
}

int vmm_manager_vcpu_set_hcpu(struct vmm_vcpu *vcpu, u32 hcpu)
{
	return vmm_scheduler_set_hcpu(vcpu, hcpu);
}

int vmm_manager_vcpu_hcpu_resched(struct vmm_vcpu *vcpu)
{
	int rc;
	irq_flags_t flags;

	if (!vcpu) {
		return VMM_EINVALID;
	}

	vmm_read_lock_irqsave_lite(&vcpu->sched_lock, flags);
	rc = vmm_scheduler_force_resched(vcpu->hcpu);
	vmm_read_unlock_irqrestore_lite(&vcpu->sched_lock, flags);

	return rc;
}

static void manager_vcpu_hcpu_func(void *fptr, void *vptr, void *data)
{
	void (*func)(struct vmm_vcpu *, void *) = fptr;
	struct vmm_vcpu *vcpu = vptr;

	if (func && vcpu) {
		func(vcpu, data);
	}
}

int vmm_manager_vcpu_hcpu_func(struct vmm_vcpu *vcpu,
			       u32 state_mask,
			       void (*func)(struct vmm_vcpu *, void *),
			       void *data)
{
	irq_flags_t flags;
	const struct vmm_cpumask *cpu_mask = NULL;

	if (!vcpu || !func) {
		return VMM_EINVALID;
	}

	vmm_read_lock_irqsave_lite(&vcpu->sched_lock, flags);
	if (arch_atomic_read(&vcpu->state) & state_mask) {
		cpu_mask = vmm_cpumask_of(vcpu->hcpu);
	}
	vmm_read_unlock_irqrestore_lite(&vcpu->sched_lock, flags);

	if (cpu_mask) {
		vmm_smp_ipi_async_call(cpu_mask, manager_vcpu_hcpu_func,
					func, vcpu, data);
	}

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
	int rc;
	bool locked;
	u32 new_hcpu;
	irq_flags_t flags;
	struct vmm_cpumask and_mask;

	if (!vcpu || !cpu_mask) {
		return VMM_EFAIL;
	}

	/* Lock load balancing */
	vmm_write_lock_irqsave_lite(&vcpu->sched_lock, flags);

	/* New affinity must overlap current affinity */
	vmm_cpumask_and(&and_mask, vcpu->cpu_affinity, cpu_mask);
	if (!vmm_cpumask_weight(&and_mask)) {
		vmm_write_unlock_irqrestore_lite(&vcpu->sched_lock, flags);
		return VMM_EINVALID;
	}

	/* Make sure current hcpu is set in both current and new affinity */
	if (!vmm_cpumask_test_cpu(vcpu->hcpu, &and_mask)) {
		vmm_write_unlock_irqrestore_lite(&vcpu->sched_lock, flags);

		/* Acquire manager lock */
		/* NOTE: We only touch manager lock if timer subsystem
		 * has started on current host CPU. This check helps
		 * create boot-time orphan VCPUs.
		 */
		if (vmm_timer_started()) {
			locked = TRUE;
			vmm_manager_lock();
		} else {
			locked = FALSE;
		}

		/* Find good host CPU */
		new_hcpu = __vmm_manager_good_hcpu(vcpu->priority, &and_mask);

		/* Change host CPU */
		rc = vmm_manager_vcpu_set_hcpu(vcpu, new_hcpu);

		/* Release manager lock */
		if (locked) {
			vmm_manager_unlock();
		}

		/* If set_hcpu failed then return failure */
		if (rc) {
			return rc;
		}

		vmm_write_lock_irqsave_lite(&vcpu->sched_lock, flags);
	}

	/* Update affinity */
	memcpy(&mngr.vcpu_affinity_mask[vcpu->id],
		cpu_mask, sizeof(*cpu_mask));
	vcpu->cpu_affinity = &mngr.vcpu_affinity_mask[vcpu->id];

	/* Unlock load balancing */
	vmm_write_unlock_irqrestore_lite(&vcpu->sched_lock, flags);

	return VMM_OK;
}

int vmm_manager_vcpu_resource_add(struct vmm_vcpu *vcpu,
				  struct vmm_vcpu_resource *res)
{
	irq_flags_t flags;

	if (!vcpu || !res || !res->name || !res->cleanup) {
		return VMM_EINVALID;
	}

	INIT_LIST_HEAD(&res->head);
	vmm_spin_lock_irqsave_lite(&vcpu->res_lock, flags);
	list_add_tail(&res->head, &vcpu->res_head);
	vmm_spin_unlock_irqrestore_lite(&vcpu->res_lock, flags);

	return VMM_OK;
}

int vmm_manager_vcpu_resource_remove(struct vmm_vcpu *vcpu,
				     struct vmm_vcpu_resource *res)
{
	irq_flags_t flags;

	if (!vcpu || !res) {
		return VMM_EINVALID;
	}

	vmm_spin_lock_irqsave_lite(&vcpu->res_lock, flags);
	list_del(&res->head);
	vmm_spin_unlock_irqrestore_lite(&vcpu->res_lock, flags);

	return VMM_OK;
}

static void vmm_manager_vcpu_resource_flush(struct vmm_vcpu *vcpu)
{
	irq_flags_t flags;
	struct vmm_vcpu_resource *res;

	if (!vcpu) {
		return;
	}

	vmm_spin_lock_irqsave_lite(&vcpu->res_lock, flags);

	while (!list_empty(&vcpu->res_head)) {
		res = list_entry(list_pop_tail(&vcpu->res_head),
				 struct vmm_vcpu_resource, head);

		vmm_spin_unlock_irqrestore_lite(&vcpu->res_lock, flags);
		if (res->cleanup)
			res->cleanup(vcpu, res);
		vmm_spin_lock_irqsave_lite(&vcpu->res_lock, flags);
	}

	vmm_spin_unlock_irqrestore_lite(&vcpu->res_lock, flags);
}

struct vmm_vcpu *vmm_manager_vcpu_orphan_create(const char *name,
					virtual_addr_t start_pc,
					virtual_size_t stack_sz,
					u8 priority,
					u64 time_slice_nsecs,
					u64 deadline,
					u64 periodicity,
					const struct vmm_cpumask *affinity)
{
	bool locked;
	u32 vnum, hcpu;
	struct vmm_vcpu *vcpu = NULL;
	const struct vmm_cpumask *aff =
			(affinity) ? affinity : cpu_online_mask;

	/* Sanity checks */
	if (name == NULL || start_pc == 0 || time_slice_nsecs == 0) {
		return NULL;
	}
	if (VMM_VCPU_MAX_PRIORITY < priority) {
		return NULL;
	}
	if (priority < VMM_VCPU_MIN_PRIORITY) {
		return NULL;
	}

	/* Acquire manager lock */
	/* NOTE: We only touch manager lock if timer subsystem
	 * has started on current host CPU. This check helps
	 * create boot-time orphan VCPUs.
	 */
	if (vmm_timer_started()) {
		locked = TRUE;
		vmm_manager_lock();
	} else {
		locked = FALSE;
	}

	/* Find good host CPU */
	hcpu = __vmm_manager_good_hcpu(priority, aff);

	/* Find the next available vcpu */
	for (vnum = 0; vnum < CONFIG_MAX_VCPU_COUNT; vnum++) {
		if (!mngr.vcpu_avail_array[vnum]) {
			continue;
		}
		vcpu = &mngr.vcpu_array[vnum];

		/* Update priority */
		vcpu->priority = priority;

		/* Update host CPU and affinity */
		vcpu->hcpu = hcpu;
		memcpy(&mngr.vcpu_affinity_mask[vcpu->id],
			aff, sizeof(*aff));
		vcpu->cpu_affinity = &mngr.vcpu_affinity_mask[vcpu->id];

		mngr.vcpu_avail_array[vcpu->id] = FALSE;
		break;
	}
	if (!vcpu) {
		goto fail;
	}

	INIT_LIST_HEAD(&vcpu->head);

	/* Update general info and state */
	vcpu->subid = 0;
	if (strlcpy(vcpu->name, name, sizeof(vcpu->name)) >=
	    sizeof(vcpu->name)) {
		goto fail_avail;
	}
	vcpu->node = NULL;
	vcpu->is_normal = FALSE;
	vcpu->is_poweroff = FALSE;
	vcpu->guest = NULL;
	arch_atomic_write(&vcpu->state, VMM_VCPU_STATE_UNKNOWN);

	/* Add VCPU to orphan list */
	list_add_tail(&vcpu->head, &mngr.orphan_vcpu_list);

	/* Increment vcpu count */
	mngr.vcpu_count++;

	/* Release manager lock */
	if (locked) {
		vmm_manager_unlock();
	}

	/* Setup start program counter and stack */
	vcpu->start_pc = start_pc;
	vcpu->stack_va = (virtual_addr_t)vmm_malloc(stack_sz);
	if (!vcpu->stack_va) {
		goto fail_list_del;
	}
	vcpu->stack_sz = stack_sz;

	/* Intialize dynamic scheduling context */
	INIT_RW_LOCK(&vcpu->sched_lock);
	vcpu->state_tstamp = vmm_timer_timestamp();
	vcpu->state_ready_nsecs = 0;
	vcpu->state_running_nsecs = 0;
	vcpu->state_paused_nsecs = 0;
	vcpu->state_halted_nsecs = 0;
	vcpu->reset_count = 0;
	vcpu->reset_tstamp = 0;
	vcpu->preempt_count = 0;
	vcpu->resumed = FALSE;
	vcpu->sched_priv = NULL;

	/* Intialize static scheduling context */
	vcpu->time_slice = time_slice_nsecs;
	vcpu->deadline = deadline;
	if (vcpu->deadline < vcpu->time_slice) {
		vcpu->deadline = vcpu->time_slice;
	}
	vcpu->periodicity = periodicity;
	if (vcpu->periodicity < vcpu->deadline) {
		vcpu->periodicity = vcpu->deadline;
	}

	/* Initialize architecture specific context */
	vcpu->arch_priv = NULL;
	if (arch_vcpu_init(vcpu)) {
		goto fail_free_stack;
	}

	/* Initialize resource list */
	INIT_SPIN_LOCK(&vcpu->res_lock);
	INIT_LIST_HEAD(&vcpu->res_head);

	/* Initialize waitqueue context */
	INIT_LIST_HEAD(&vcpu->wq_head);
	vcpu->wq_priv = NULL;

	/* Notify scheduler about new VCPU */
	if (vmm_manager_vcpu_set_state(vcpu,
					VMM_VCPU_STATE_RESET)) {
		goto fail_vcpu_deinit;
	}

	return vcpu;

fail_vcpu_deinit:
	arch_vcpu_deinit(vcpu);
fail_free_stack:
	vmm_free((void *)vcpu->stack_va);
fail_list_del:
	if (vmm_timer_started()) {
		vmm_manager_lock();
		locked = TRUE;
	} else {
		locked = FALSE;
	}
	mngr.vcpu_count--;
	list_del(&vcpu->head);
fail_avail:
	mngr.vcpu_avail_array[vcpu->id] = TRUE;
fail:
	if (locked) {
		vmm_manager_unlock();
	}
	return NULL;
}

int vmm_manager_vcpu_orphan_destroy(struct vmm_vcpu *vcpu)
{
	int rc = VMM_EFAIL;

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

	/* Flush all resources acquired by this VCPU */
	vmm_manager_vcpu_resource_flush(vcpu);

	/* Set VCPU to unknown state (This will clean scheduling context) */
	if ((rc = vmm_manager_vcpu_set_state(vcpu,
					VMM_VCPU_STATE_UNKNOWN))) {
		return rc;
	}
	vcpu->sched_priv = NULL;

	/* Deinit architecture specific context */
	if ((rc = arch_vcpu_deinit(vcpu))) {
		return rc;
	}

	/* Free stack pages */
	if (vcpu->stack_va) {
		vmm_free((void *)vcpu->stack_va);
	}

	/* Acquire manager lock */
	vmm_manager_lock();

	/* Decrement vcpu count */
	mngr.vcpu_count--;

	/* Remove VCPU from orphan list */
	list_del(&vcpu->head);

	/* Mark this VCPU as available */
	mngr.vcpu_avail_array[vcpu->id] = TRUE;

	/* Release manager lock */
	vmm_manager_unlock();

	return VMM_OK;
}

u32 vmm_manager_max_guest_count(void)
{
	return CONFIG_MAX_GUEST_COUNT;
}

u32 vmm_manager_guest_count(void)
{
	u32 ret;

	vmm_manager_lock();
	ret = mngr.guest_count;
	vmm_manager_unlock();

	return ret;
}

struct vmm_guest *vmm_manager_guest(u32 guest_id)
{
	struct vmm_guest *ret = NULL;

	if (guest_id < CONFIG_MAX_GUEST_COUNT) {
		vmm_manager_lock();
		if (!mngr.guest_avail_array[guest_id]) {
			ret = &mngr.guest_array[guest_id];
		}
		vmm_manager_unlock();
	}

	return ret;
}

struct vmm_guest *vmm_manager_guest_find(const char *guest_name)
{
	u32 g;
	struct vmm_guest *ret;

	if (!guest_name) {
		return NULL;
	}

	/* Acquire manager lock */
	vmm_manager_lock();

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
	vmm_manager_unlock();

	return ret;
}

int vmm_manager_guest_iterate(int (*iter)(struct vmm_guest *, void *),
			      void *priv)
{
	int rc, g;
	struct vmm_guest *guest;

	/* If no iteration callback then return */
	if (!iter) {
		return VMM_EINVALID;
	}

	/* Acquire manager lock */
	vmm_manager_lock();

	/* Iterate over each used VCPU instance */
	rc = VMM_OK;
	for (g = 0; g < CONFIG_MAX_GUEST_COUNT; g++) {
		if (mngr.guest_avail_array[g]) {
			continue;
		}
		guest = &mngr.guest_array[g];

		rc = iter(guest, priv);
		if (rc) {
			break;
		}
	}

	/* Release manager lock */
	vmm_manager_unlock();

	return rc;
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

struct vmm_vcpu *vmm_manager_guest_next_vcpu(const struct vmm_guest *guest,
					     struct vmm_vcpu *current)
{
	irq_flags_t flags;
	struct vmm_vcpu *ret = NULL;
	struct vmm_guest *g = (struct vmm_guest *)guest;

	if (!g) {
		return NULL;
	}

	vmm_read_lock_irqsave_lite(&g->vcpu_lock, flags);

	if (!current) {
		if (!list_empty(&g->vcpu_list)) {
			ret = list_first_entry(&g->vcpu_list,
						struct vmm_vcpu, head);
		}
	} else if (!list_is_last(&current->head, &g->vcpu_list)) {
		ret = list_first_entry(&current->head,
					struct vmm_vcpu, head);
	}

	vmm_read_unlock_irqrestore_lite(&g->vcpu_lock, flags);

	return ret;
}

int vmm_manager_guest_vcpu_iterate(struct vmm_guest *guest,
				   int (*iter)(struct vmm_vcpu *, void *),
				   void *priv)
{
	int rc = VMM_OK;
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

	guest->reset_count++;
	guest->reset_tstamp = vmm_timer_timestamp();

	rc = vmm_manager_guest_vcpu_iterate(guest,
				manager_guest_reset_iter, NULL);
	if (rc) {
		return rc;
	}

	if (!(rc = arch_guest_init(guest))) {
		rc = vmm_guest_aspace_reset(guest);
	}

	return rc;
}

u64 vmm_manager_guest_reset_timestamp(struct vmm_guest *guest)
{
	return (guest) ? guest->reset_tstamp : 0;
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

static bool manager_have_req(struct vmm_guest *guest)
{
	bool ret = FALSE;
	irq_flags_t flags;

	vmm_spin_lock_irqsave_lite(&guest->req_lock, flags);
	if (!list_empty(&guest->req_list)) {
		ret = TRUE;
	}
	vmm_spin_unlock_irqrestore_lite(&guest->req_lock, flags);

	return ret;
}

static void manager_enqueue_req(struct vmm_guest *guest,
				struct vmm_guest_request *req)
{
	irq_flags_t flags;

	vmm_spin_lock_irqsave_lite(&guest->req_lock, flags);
	list_add_tail(&req->head, &guest->req_list);
	vmm_spin_unlock_irqrestore_lite(&guest->req_lock, flags);

	vmm_workqueue_schedule_work(NULL, &mngr.guest_work_array[guest->id]);
}

static struct vmm_guest_request *manager_dequeue_req(struct vmm_guest *guest)
{
	irq_flags_t flags;
	struct vmm_guest_request *req = NULL;

	vmm_spin_lock_irqsave_lite(&guest->req_lock, flags);
	if (!list_empty(&guest->req_list)) {
		req = list_entry(list_pop(&guest->req_list),
				 struct vmm_guest_request, head);
	}
	vmm_spin_unlock_irqrestore_lite(&guest->req_lock, flags);

	return req;
}

static void manager_flush_req(struct vmm_guest *guest)
{
	irq_flags_t flags;
	struct vmm_guest_request *req;

	vmm_spin_lock_irqsave_lite(&guest->req_lock, flags);
	while (!list_empty(&guest->req_list)) {
		req = list_entry(list_pop(&guest->req_list),
				 struct vmm_guest_request, head);
		vmm_free(req);
	}
	vmm_spin_unlock_irqrestore_lite(&guest->req_lock, flags);
}

static void manager_req_work(struct vmm_work *work)
{
	u32 id;
	void *start, *end, *ptr;
	struct vmm_guest *guest;
	struct vmm_guest_request *req;

	/* Determine guest pointer from work pointer */
	ptr = work;
	start = &mngr.guest_work_array[0];
	end = &mngr.guest_work_array[CONFIG_MAX_GUEST_COUNT - 1];
	if (ptr < start || end <= ptr) {
		return;
	}
	id = ptr - start;
	id = id / sizeof(*work);
	guest = vmm_manager_guest(id);
	if (!guest) {
		return;
	}

	/* Process one request if available */
	if ((req = manager_dequeue_req(guest))) {
		req->func(guest, req->data);
		vmm_free(req);

		/* Reschedule work if we more request */
		if (manager_have_req(guest)) {
			vmm_workqueue_schedule_work(NULL,
					&mngr.guest_work_array[guest->id]);
		}
	}
}

int vmm_manager_guest_request(struct vmm_guest *guest,
			      void (*req_func)(struct vmm_guest *, void *),
			      void *req_data)
{
	struct vmm_guest_request *req;

	if (!guest || !req_func) {
		return VMM_EINVALID;
	}

	req = vmm_zalloc(sizeof(*req));
	if (!req) {
		return VMM_ENOMEM;
	}

	INIT_LIST_HEAD(&req->head);
	req->func = req_func;
	req->data = req_data;

	manager_enqueue_req(guest, req);

	return VMM_OK;
}

static void manager_reboot_request(struct vmm_guest *guest, void *data)
{
	vmm_manager_guest_reset(guest);
	vmm_manager_guest_kick(guest);
}

int vmm_manager_guest_reboot_request(struct vmm_guest *guest)
{
	struct vmm_vcpu *cvcpu;

	if (!guest) {
		return VMM_EINVALID;
	}

	/* If current VCPU belongs to the Guest then
	 * pause the VCPU so that we don't return back
	 * to the VCPU after submitting request.
	 */
	cvcpu = vmm_scheduler_current_vcpu();
	if (cvcpu &&
	    (cvcpu->guest == guest) &&
	    vmm_scheduler_normal_context()) {
		vmm_manager_vcpu_pause(cvcpu);
	}

	return vmm_manager_guest_request(guest,
				manager_reboot_request, NULL);
}

static void manager_shutdown_request(struct vmm_guest *guest, void *data)
{
	vmm_manager_guest_reset(guest);
}

int vmm_manager_guest_shutdown_request(struct vmm_guest *guest)
{
	struct vmm_vcpu *cvcpu;

	if (!guest) {
		return VMM_EINVALID;
	}

	/* If current VCPU belongs to the Guest then
	 * pause the VCPU so that we don't return back
	 * to the VCPU after submitting request.
	 */
	cvcpu = vmm_scheduler_current_vcpu();
	if (cvcpu &&
	    (cvcpu->guest == guest) &&
	    vmm_scheduler_normal_context()) {
		vmm_manager_vcpu_pause(cvcpu);
	}

	return vmm_manager_guest_request(guest,
				manager_shutdown_request, NULL);
}

struct vmm_guest *vmm_manager_guest_create(struct vmm_devtree_node *gnode)
{
	u32 val, vnum, gnum;
	const char *str;
	irq_flags_t flags;
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
	vmm_manager_lock();

	/* Ensure guest node uniqueness */
	list_for_each_entry(guest, &mngr.guest_list, head) {
		if ((guest->node == gnode) ||
		    (strcmp(guest->name, gnode->name) == 0)) {
			vmm_manager_unlock();
			vmm_printf("%s: Duplicate Guest %s detected\n",
					__func__, gnode->name);
			return NULL;
		}
	}

	/* Find next available guest instance */
	for (gnum = 0; gnum < CONFIG_MAX_GUEST_COUNT; gnum++) {
		if (mngr.guest_avail_array[gnum]) {
			guest = &mngr.guest_array[gnum];
			mngr.guest_avail_array[guest->id] = FALSE;
			break;
		}
	}
	if (!guest) {
		vmm_manager_unlock();
		vmm_printf("%s: No available Guest instance found\n",
			   __func__);
		return NULL;
	}

	/* Add guest instance to guest list */
	list_add_tail(&guest->head, &mngr.guest_list);

	/* Increment guest count */
	mngr.guest_count++;

	/* Initialize guest instance */
	strlcpy(guest->name, gnode->name, sizeof(guest->name));
	vmm_devtree_ref_node(gnode);
	guest->node = gnode;
#ifdef CONFIG_CPU_BE
	guest->is_big_endian = TRUE;
#else
	guest->is_big_endian = FALSE;
#endif
	guest->reset_count = 0;
	guest->reset_tstamp = vmm_timer_timestamp();
	INIT_SPIN_LOCK(&guest->req_lock);
	INIT_LIST_HEAD(&guest->req_list);
	INIT_RW_LOCK(&guest->vcpu_lock);
	guest->vcpu_count = 0;
	INIT_LIST_HEAD(&guest->vcpu_list);
	memset(&guest->aspace, 0, sizeof(guest->aspace));
	guest->aspace.initialized = FALSE;
	INIT_RW_LOCK(&guest->aspace.reg_iotree_lock);
	INIT_LIST_HEAD(&guest->aspace.reg_ioprobe_list);
	guest->aspace.reg_iotree = RB_ROOT;
	INIT_RW_LOCK(&guest->aspace.reg_memtree_lock);
	INIT_LIST_HEAD(&guest->aspace.reg_memprobe_list);
	guest->aspace.reg_memtree = RB_ROOT;
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

	/* Release manager lock */
	vmm_manager_unlock();

	vsnode = vmm_devtree_getchild(gnode, VMM_DEVTREE_VCPUS_NODE_NAME);
	if (!vsnode) {
		vmm_printf("%s: vcpus node not found for Guest %s\n",
			   __func__, gnode->name);
		goto fail_destroy_guest;
	}

	vmm_devtree_for_each_child(vnode, vsnode) {
		int index;
		u32 cpu;
		struct vmm_cpumask mask;

		/* Sanity checks */
		if (CONFIG_MAX_VCPU_COUNT <= mngr.vcpu_count) {
			vmm_printf("%s: No more free VCPUs\n"
				   "for Guest %s VCPU %s\n",
				   __func__, gnode->name, vnode->name);
			vmm_devtree_dref_node(vnode);
			goto fail_dref_vsnode;
		}
		if (vmm_devtree_read_string(vnode,
				VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME, &str)) {
			vmm_printf("%s: No device_type attribute\n"
				   "for Guest %s VCPU %s\n",
				   __func__, gnode->name, vnode->name);
			vmm_devtree_dref_node(vnode);
			goto fail_dref_vsnode;
		}
		if (strcmp(str, VMM_DEVTREE_DEVICE_TYPE_VAL_VCPU) != 0) {
			vmm_printf("%s: Invalid device_type attribute\n"
				   "for Guest %s VCPU %s\n",
				   __func__, gnode->name, vnode->name);
			vmm_devtree_dref_node(vnode);
			goto fail_dref_vsnode;
		}

		/* Setup VCPU affinity mask */
		if (vmm_devtree_getattr(vnode,
				VMM_DEVTREE_VCPU_AFFINITY_ATTR_NAME)) {
			/* Start with empty affinity mask */
			mask = VMM_CPU_MASK_NONE;

			/* Set all assigned CPU in the mask */
			index = 0;
			while (vmm_devtree_read_u32_atindex(vnode,
				VMM_DEVTREE_VCPU_AFFINITY_ATTR_NAME,
				&cpu, index) == VMM_OK) {
				if ((cpu < CONFIG_CPU_COUNT) &&
				    vmm_cpu_online(cpu)) {
					vmm_cpumask_set_cpu(cpu, &mask);
				} else {
					vmm_printf(
						"%s: CPU%d is out of bound"
						" (%d <) or not online for"
						" Guest %s VCPU %s\n",
						__func__, cpu,
						CONFIG_CPU_COUNT,
						gnode->name, vnode->name);
					vmm_devtree_dref_node(vnode);
					goto fail_dref_vsnode;
				}
				index++;
			}

			/* If affinity mask turns-out to be empty then fail */
			if (vmm_cpumask_weight(&mask) < 1) {
				vmm_printf("%s: Empty affinity mask\n"
					   "for Guest %s VCPU %s\n", __func__,
					   gnode->name, vnode->name);
				vmm_devtree_dref_node(vnode);
				goto fail_dref_vsnode;
			}
		} else {
			memcpy(&mask, cpu_online_mask, sizeof(mask));
		}

		/* Acquire manager lock */
		vmm_manager_lock();

		/* Find next available VCPU instance */
		vcpu = NULL;
		for (vnum = 0; vnum < CONFIG_MAX_VCPU_COUNT; vnum++) {
			if (!mngr.vcpu_avail_array[vnum]) {
				continue;
			}
			vcpu = &mngr.vcpu_array[vnum];

			/* Update priority */
			if (vmm_devtree_read_u32(vnode,
				VMM_DEVTREE_PRIORITY_ATTR_NAME, &val)) {
				vcpu->priority = VMM_VCPU_DEF_PRIORITY;
			} else {
				vcpu->priority = val;
			}
			if (VMM_VCPU_MAX_PRIORITY < vcpu->priority) {
				vcpu->priority = VMM_VCPU_MAX_PRIORITY;
			}
			if (vcpu->priority < VMM_VCPU_MIN_PRIORITY) {
				vcpu->priority = VMM_VCPU_MIN_PRIORITY;
			}

			/* Update host CPU and affinity */
			memcpy(&mngr.vcpu_affinity_mask[vcpu->id],
				&mask, sizeof(mask));
			vcpu->hcpu =
				__vmm_manager_good_hcpu(vcpu->priority, &mask);
			vcpu->cpu_affinity =
				&mngr.vcpu_affinity_mask[vcpu->id];

			mngr.vcpu_avail_array[vcpu->id] = FALSE;
			break;
		}
		if (!vcpu) {
			vmm_printf("%s: No available VCPU instance found \n"
				   "for Guest %s VCPU %s\n",
				   __func__, gnode->name, vnode->name);
			vmm_manager_unlock();
			vmm_devtree_dref_node(vnode);
			goto fail_dref_vsnode;
		}

		/* Update general info and state */
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
			vmm_manager_unlock();
			vmm_devtree_dref_node(vnode);
			goto fail_dref_vsnode;
		}
		vmm_devtree_ref_node(vnode);
		vcpu->node = vnode;
		vcpu->is_normal = TRUE;
		vcpu->is_poweroff = FALSE;
		vcpu->guest = guest;
		arch_atomic_write(&vcpu->state, VMM_VCPU_STATE_UNKNOWN);

		/* Increment VCPU count */
		mngr.vcpu_count++;

		/* Release manager lock */
		vmm_manager_unlock();

		/* Setup start program counter and stack */
		vmm_devtree_read_virtaddr(vnode,
			VMM_DEVTREE_START_PC_ATTR_NAME, &vcpu->start_pc);
		vcpu->stack_va =
			(virtual_addr_t)vmm_malloc(CONFIG_IRQ_STACK_SIZE);
		if (!vcpu->stack_va) {
			vmm_printf("%s: stack alloc failed "
				   "for VCPU %s\n", __func__, vcpu->name);
			vmm_devtree_dref_node(vcpu->node);
			vcpu->node = NULL;
			vmm_manager_lock();
			mngr.vcpu_count--;
			mngr.vcpu_avail_array[vcpu->id] = TRUE;
			vmm_manager_unlock();
			vmm_devtree_dref_node(vnode);
			goto fail_dref_vsnode;
		}
		vcpu->stack_sz = CONFIG_IRQ_STACK_SIZE;

		/* Initialize dynamic scheduling context */
		INIT_RW_LOCK(&vcpu->sched_lock);
		vcpu->state_tstamp = vmm_timer_timestamp();
		vcpu->state_ready_nsecs = 0;
		vcpu->state_running_nsecs = 0;
		vcpu->state_paused_nsecs = 0;
		vcpu->state_halted_nsecs = 0;
		vcpu->reset_count = 0;
		vcpu->reset_tstamp = 0;
		vcpu->preempt_count = 0;
		vcpu->resumed = FALSE;
		vcpu->sched_priv = NULL;

		/* Initialize static scheduling context */
		if (vmm_devtree_read_u64(vnode,
			VMM_DEVTREE_TIME_SLICE_ATTR_NAME, &vcpu->time_slice)) {
			vcpu->time_slice = VMM_VCPU_DEF_TIME_SLICE;
		}
		if (vcpu->time_slice == 0) {
			vcpu->time_slice = VMM_VCPU_DEF_TIME_SLICE;
		}
		if (vmm_devtree_read_u64(vnode,
			VMM_DEVTREE_DEADLINE_ATTR_NAME, &vcpu->deadline)) {
			vcpu->deadline = VMM_VCPU_DEF_DEADLINE;
		}
		if (vcpu->deadline < vcpu->time_slice) {
			vcpu->deadline = vcpu->time_slice;
		}
		if (vmm_devtree_read_u64(vnode,
			VMM_DEVTREE_PERIODICITY_ATTR_NAME, &vcpu->periodicity)) {
			vcpu->periodicity = VMM_VCPU_DEF_PERIODICITY;
		}
		if (vcpu->periodicity < vcpu->deadline) {
			vcpu->periodicity = vcpu->deadline;
		}

		/* Initialize architecture specific context */
		vcpu->arch_priv = NULL;
		if (arch_vcpu_init(vcpu)) {
			vmm_free((void *)vcpu->stack_va);
			vmm_printf("%s: arch_vcpu_init() failed "
				   "for VCPU %s\n", __func__, vcpu->name);
			vmm_devtree_dref_node(vcpu->node);
			vcpu->node = NULL;
			vmm_manager_lock();
			mngr.vcpu_count--;
			mngr.vcpu_avail_array[vcpu->id] = TRUE;
			vmm_manager_unlock();
			vmm_devtree_dref_node(vnode);
			goto fail_dref_vsnode;
		}

		/* Initialize virtual IRQ context */
		if (vmm_vcpu_irq_init(vcpu)) {
			arch_vcpu_deinit(vcpu);
			vmm_free((void *)vcpu->stack_va);
			vmm_printf("%s: vmm_vcpu_irq_init() failed "
				   "for VCPU %s\n", __func__, vcpu->name);
			vmm_devtree_dref_node(vcpu->node);
			vcpu->node = NULL;
			vmm_manager_lock();
			mngr.vcpu_count--;
			mngr.vcpu_avail_array[vcpu->id] = TRUE;
			vmm_manager_unlock();
			vmm_devtree_dref_node(vnode);
			goto fail_dref_vsnode;
		}

		/* Initialize resource list */
		INIT_SPIN_LOCK(&vcpu->res_lock);
		INIT_LIST_HEAD(&vcpu->res_head);

		/* Initialize waitqueue context */
		INIT_LIST_HEAD(&vcpu->wq_head);
		vcpu->wq_priv = NULL;

		/* Notify scheduler about new VCPU */
		if (vmm_manager_vcpu_set_state(vcpu, VMM_VCPU_STATE_RESET)) {
			vmm_vcpu_irq_deinit(vcpu);
			arch_vcpu_deinit(vcpu);
			vmm_free((void *)vcpu->stack_va);
			vmm_printf("%s: Setting RESET state failed "
				   "for VCPU %s\n", __func__, vcpu->name);
			vmm_devtree_dref_node(vcpu->node);
			vcpu->node = NULL;
			vmm_manager_lock();
			mngr.vcpu_count--;
			mngr.vcpu_avail_array[vcpu->id] = TRUE;
			vmm_manager_unlock();
			vmm_devtree_dref_node(vnode);
			goto fail_dref_vsnode;
		}

		/* Get poweroff flag from device tree */
		if (vmm_devtree_getattr(vnode,
				VMM_DEVTREE_VCPU_POWEROFF_ATTR_NAME)) {
			vcpu->is_poweroff = TRUE;
		}

		/* Add VCPU to Guest child list */
		vmm_write_lock_irqsave_lite(&guest->vcpu_lock, flags);
		list_add_tail(&vcpu->head, &guest->vcpu_list);
		guest->vcpu_count++;
		vmm_write_unlock_irqrestore_lite(&guest->vcpu_lock, flags);
	}

	/* Release vcpus node */
	vmm_devtree_dref_node(vsnode);

	/* Fail if no VCPU is associated to the guest */
	vmm_read_lock_irqsave_lite(&guest->vcpu_lock, flags);
	if (list_empty(&guest->vcpu_list)) {
		vmm_read_unlock_irqrestore_lite(&guest->vcpu_lock, flags);
		goto fail_destroy_guest;
	}
	vmm_read_unlock_irqrestore_lite(&guest->vcpu_lock, flags);

	/* Initialize arch guest context */
	if (arch_guest_init(guest)) {
		goto fail_destroy_guest;
	}

	/* Initialize guest address space */
	if (vmm_guest_aspace_init(guest)) {
		goto fail_destroy_guest;
	}

	/* Reset guest address space */
	if (vmm_guest_aspace_reset(guest)) {
		goto fail_destroy_guest;
	}

	return guest;

fail_dref_vsnode:
	vmm_devtree_dref_node(vsnode);
fail_destroy_guest:
	vmm_manager_guest_destroy(guest);
	return NULL;
}

int vmm_manager_guest_destroy(struct vmm_guest *guest)
{
	int rc;
	irq_flags_t flags;
	struct vmm_vcpu *vcpu;

	/* Sanity Check */
	if (!guest) {
		return VMM_EFAIL;
	}

	/* For sanity reset guest (ignore reture value) */
	vmm_manager_guest_reset(guest);

	/* Flush all request for this guest */
	manager_flush_req(guest);

	/* Deinit the guest aspace */
	if ((rc = vmm_guest_aspace_deinit(guest))) {
		return rc;
	}

	/* Deinit arch guest context */
	if ((rc = arch_guest_deinit(guest))) {
		return rc;
	}

	/* Acquire Guest VCPU lock */
	vmm_write_lock_irqsave_lite(&guest->vcpu_lock, flags);

	/* Destroy each VCPU of guest */
	while (!list_empty(&guest->vcpu_list)) {
		vcpu = list_first_entry(&guest->vcpu_list,
					struct vmm_vcpu, head);

		/* Remove from guest->vcpu_list */
		guest->vcpu_count--;
		list_del(&vcpu->head);

		/* Release Guest VCPU lock */
		vmm_write_unlock_irqrestore_lite(&guest->vcpu_lock, flags);

		/* Flush all resources acquired by this VCPU */
		vmm_manager_vcpu_resource_flush(vcpu);

		/* Set VCPU state to unknown
		 * (This will clean scheduling context)
		 */
		if ((rc = vmm_manager_vcpu_set_state(vcpu,
						VMM_VCPU_STATE_UNKNOWN))) {
			return rc;
		}
		vcpu->sched_priv = NULL;

		/* Deinit Virtual IRQ context */
		if ((rc = vmm_vcpu_irq_deinit(vcpu))) {
			return rc;
		}

		/* Deinit architecture specific context */
		if ((rc = arch_vcpu_deinit(vcpu))) {
			return rc;
		}

		/* Free stack pages */
		if (vcpu->stack_va) {
			vmm_free((void *)vcpu->stack_va);
		}

		/* De-reference VCPU node */
		vmm_devtree_dref_node(vcpu->node);
		vcpu->node = NULL;

		/* Acquire manager lock */
		vmm_manager_lock();

		/* Decrement vcpu count */
		mngr.vcpu_count--;

		/* Mark this VCPU as available */
		mngr.vcpu_avail_array[vcpu->id] = TRUE;

		/* Release manager lock */
		vmm_manager_unlock();

		/* Acquire Guest VCPU lock */
		vmm_write_lock_irqsave_lite(&guest->vcpu_lock, flags);
	}

	/* Release Guest VCPU lock */
	vmm_write_unlock_irqrestore_lite(&guest->vcpu_lock, flags);

	/* Acquire manager lock */
	vmm_manager_lock();

	/* Reset guest instance members */
	vmm_devtree_dref_node(guest->node);
	guest->node = NULL;
	guest->name[0] = '\0';
	INIT_LIST_HEAD(&guest->vcpu_list);

	/* Decrement guest count */
	mngr.guest_count--;

	/* Remove from guest list */
	list_del(&guest->head);
	INIT_LIST_HEAD(&guest->head);

	/* Mark this guest instance as available */
	mngr.guest_avail_array[guest->id] = TRUE;

	/* Release manager lock */
	vmm_manager_unlock();

	return VMM_OK;
}

int __init vmm_manager_init(void)
{
	u32 vnum, gnum;

	/* Reset the manager control structure */
	memset(&mngr, 0, sizeof(mngr));

	/* Intialize guest & vcpu managment parameters */
	INIT_MUTEX(&mngr.lock);
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
		INIT_WORK(&mngr.guest_work_array[gnum], manager_req_work);
	}

	/* Initialze memory for vcpu instances */
	for (vnum = 0; vnum < CONFIG_MAX_VCPU_COUNT; vnum++) {
		INIT_LIST_HEAD(&mngr.vcpu_array[vnum].head);
		mngr.vcpu_array[vnum].id = vnum;
		mngr.vcpu_array[vnum].name[0] = 0;
		mngr.vcpu_array[vnum].node = NULL;
		mngr.vcpu_array[vnum].is_normal = FALSE;
		arch_atomic_write(&mngr.vcpu_array[vnum].state,
				  VMM_VCPU_STATE_UNKNOWN);
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
