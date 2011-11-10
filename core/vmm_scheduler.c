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

#include <vmm_math.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_cpu.h>
#include <vmm_spinlocks.h>
#include <vmm_vcpu_irq.h>
#include <vmm_timer.h>
#include <vmm_scheduler.h>

#define VMM_IDLE_VCPU_STACK_SZ 1024
#define VMM_IDLE_VCPU_TIMESLICE 100000000

/** Control structure for Scheduler */
struct vmm_scheduler_ctrl {
	vmm_spinlock_t lock;
	vmm_vcpu_t *idle_vcpu;
	u8 idle_vcpu_stack[VMM_IDLE_VCPU_STACK_SZ];
	s32 vcpu_current;
	bool irq_context;
	vmm_timer_event_t * ev;
};

typedef struct vmm_scheduler_ctrl vmm_scheduler_ctrl_t;

vmm_scheduler_ctrl_t sched;

void vmm_scheduler_next(vmm_timer_event_t * ev, vmm_user_regs_t * regs)
{
	int next, vcpu_count = vmm_manager_vcpu_count();
	vmm_vcpu_t *cur_vcpu, *nxt_vcpu;

	/* Determine current vcpu */
	cur_vcpu = vmm_manager_vcpu(sched.vcpu_current);

	/* Determine the next ready vcpu to schedule */
	next = (cur_vcpu) ? cur_vcpu->id : -1;
	next = ((next + 1) < vcpu_count) ? (next + 1) : 0;
	nxt_vcpu = vmm_manager_vcpu(next);
	while ((nxt_vcpu->state != VMM_VCPU_STATE_READY) &&
		(next != sched.vcpu_current)) {
		next = ((next + 1) < vcpu_count) ? (next + 1) : 0;
		nxt_vcpu = vmm_manager_vcpu(next);
	}

	/* Do context switch between current and next vcpus */
	if (!cur_vcpu || (cur_vcpu->id != nxt_vcpu->id)) {
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
		nxt_vcpu->state = VMM_VCPU_STATE_RUNNING;
		sched.vcpu_current = nxt_vcpu->id;
		vmm_timer_event_start(ev, nxt_vcpu->time_slice);
	}
}

void vmm_scheduler_timer_event(vmm_timer_event_t * ev)
{
	vmm_vcpu_t * vcpu = vmm_manager_vcpu(sched.vcpu_current);
	if (vcpu) {
		if (!vcpu->preempt_count) {
			vmm_scheduler_next(ev, ev->cpu_regs);
		} else {
			vmm_timer_event_restart(ev);
		}
	} else {
		vmm_scheduler_next(ev, ev->cpu_regs);
	}
}

int vmm_scheduler_notify_state_change(vmm_vcpu_t * vcpu, u32 new_state)
{
	int rc = VMM_OK;

	if(!vcpu) {
		return VMM_EFAIL;
	}

	switch(new_state) {
	case VMM_VCPU_STATE_PAUSED:
	case VMM_VCPU_STATE_HALTED:
		if(vmm_manager_vcpu(sched.vcpu_current) == vcpu) {
			vmm_timer_event_start(sched.ev, 0);
		}
		break;
	}

	return rc;
}

void vmm_scheduler_irq_enter(vmm_user_regs_t * regs, bool vcpu_context)
{
	/* Indicate that we have entered in IRQ */
	sched.irq_context = (vcpu_context) ? FALSE : TRUE;
}

void vmm_scheduler_irq_exit(vmm_user_regs_t * regs)
{
	vmm_vcpu_t * vcpu = NULL;

	/* Determine current vcpu */
	vcpu = vmm_manager_vcpu(sched.vcpu_current);
	if (!vcpu) {
		return;
	}

	/* Schedule next vcpu if state of 
	 * current vcpu is not RUNNING */
	if (vcpu->state != VMM_VCPU_STATE_RUNNING) {
		vmm_scheduler_next(sched.ev, regs);
		return;
	}

	/* VCPU irq processing */
	vmm_vcpu_irq_process(regs);

	/* Indicate that we have exited IRQ */
	sched.irq_context = FALSE;
}

bool vmm_scheduler_irq_context(void)
{
	return sched.irq_context;
}

vmm_vcpu_t * vmm_scheduler_current_vcpu(void)
{
	irq_flags_t flags;
	vmm_vcpu_t * vcpu = NULL;
	flags = vmm_spin_lock_irqsave(&sched.lock);
	if (sched.vcpu_current != -1) {
		vcpu = vmm_manager_vcpu(sched.vcpu_current);
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

void vmm_scheduler_yield(void)
{
	if (vmm_scheduler_irq_context()) {
		vmm_panic("%s: Tried to yield in IRQ context\n", __func__);
	}

	vmm_timer_event_start(sched.ev, 0);
}

static void idle_orphan(void)
{
	while(1) {
		vmm_scheduler_yield();
	}
}

int vmm_scheduler_init(void)
{
	/* Reset the scheduler control structure */
	vmm_memset(&sched, 0, sizeof(sched));

	/* Create idle orphan vcpu with 100 msec time slice. (Per Host CPU) */
	sched.idle_vcpu = vmm_manager_vcpu_orphan_create("idle/0",
	(virtual_addr_t)&idle_orphan,
	(virtual_addr_t)&sched.idle_vcpu_stack[VMM_IDLE_VCPU_STACK_SZ - 4],
	VMM_IDLE_VCPU_TIMESLICE);

	/* Initialize scheduling parameters. (Per Host CPU) */
	sched.vcpu_current = -1;

	/* Initialize IRQ state */
	sched.irq_context = FALSE;

	/* Create timer event and start it. (Per Host CPU) */
	sched.ev = vmm_timer_event_create("sched", 
					  &vmm_scheduler_timer_event, 
					  NULL);
	if (!sched.ev) {
		return VMM_EFAIL;
	}
	vmm_timer_event_start(sched.ev, 0);

	return VMM_OK;
}
