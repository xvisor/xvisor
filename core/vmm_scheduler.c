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
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for hypervisor scheduler
 */

#include <vmm_error.h>
#include <vmm_smp.h>
#include <vmm_percpu.h>
#include <vmm_cpumask.h>
#include <vmm_stdio.h>
#include <vmm_vcpu_irq.h>
#include <vmm_timer.h>
#include <vmm_schedalgo.h>
#include <vmm_scheduler.h>
#include <arch_cpu_irq.h>
#include <arch_vcpu.h>
#include <libs/stringlib.h>

#define IDLE_VCPU_STACK_SZ 		CONFIG_THREAD_STACK_SIZE
#define IDLE_VCPU_PRIORITY 		VMM_VCPU_MIN_PRIORITY
#define IDLE_VCPU_TIMESLICE 		(VMM_VCPU_DEF_TIME_SLICE * 100)

/** Control structure for Scheduler */
struct vmm_scheduler_ctrl {
	void * rq;
	struct vmm_vcpu *current_vcpu;
	struct vmm_vcpu *idle_vcpu;
	bool irq_context;
	bool yield_on_irq_exit;
	struct vmm_timer_event ev;
};

static DEFINE_PER_CPU(struct vmm_scheduler_ctrl, sched);

static void vmm_scheduler_next(struct vmm_timer_event *ev, arch_regs_t *regs)
{
	struct vmm_scheduler_ctrl *schedp = &this_cpu(sched);
	struct vmm_vcpu *current = schedp->current_vcpu;
	struct vmm_vcpu *next = NULL; 

	if (current) {
		/* Normal scheduling */
		if (current->state & VMM_VCPU_STATE_SAVEABLE) {
			if (current->state == VMM_VCPU_STATE_RUNNING) {
				current->state = VMM_VCPU_STATE_READY;
				vmm_schedalgo_rq_enqueue(schedp->rq, current);
			}
			next = vmm_schedalgo_rq_dequeue(schedp->rq);
			if (next && (next->id != current->id)) {
				arch_vcpu_switch(current, next, regs);
			}
		} else {
			if (current->state == VMM_VCPU_STATE_READY) {
				vmm_schedalgo_rq_enqueue(schedp->rq, current);
			}
			next = vmm_schedalgo_rq_dequeue(schedp->rq);
			if (next) {
				arch_vcpu_switch(NULL, next, regs);
			}
		}
	} else {
		/* First time scheduling */
		next = vmm_schedalgo_rq_dequeue(schedp->rq);
		if (next) {
			arch_vcpu_switch(NULL, next, regs);
		} else {
			/* This should never happen !!! */
			while (1);
		}
	}

	/* Make next VCPU as current VCPU */
	if (next) {
		next->state = VMM_VCPU_STATE_RUNNING;
		schedp->current_vcpu = next;
		vmm_timer_event_start(ev, next->time_slice);
	}
}

static void vmm_scheduler_timer_event(struct vmm_timer_event *ev)
{
	struct vmm_vcpu * vcpu = this_cpu(sched).current_vcpu;
	if (vcpu) {
		if (!vcpu->preempt_count) {
			vmm_scheduler_next(ev, ev->regs);
		} else {
			vmm_timer_event_restart(ev);
		}
	} else {
		vmm_scheduler_next(ev, ev->regs);
	}
}

int vmm_scheduler_notify_state_change(struct vmm_vcpu *vcpu, u32 new_state)
{
	int rc = VMM_OK;
	struct vmm_scheduler_ctrl *schedp = &this_cpu(sched);

	if(!vcpu) {
		return VMM_EFAIL;
	}

	switch(new_state) {
	case VMM_VCPU_STATE_UNKNOWN:
		/* Existing VCPU being destroyed */
		rc = vmm_schedalgo_vcpu_cleanup(vcpu);
		break;
	case VMM_VCPU_STATE_RESET:
		if (vcpu->state == VMM_VCPU_STATE_UNKNOWN) {
			/* New VCPU */
			rc = vmm_schedalgo_vcpu_setup(vcpu);
		} else {
			/* Existing VCPU */
			/* Make sure VCPU is not in a ready queue */
			if((schedp->current_vcpu != vcpu) &&
			   (vcpu->state == VMM_VCPU_STATE_READY)) {
				rc = vmm_schedalgo_rq_detach(schedp->rq, vcpu);
			}
		}
		break;
	case VMM_VCPU_STATE_READY:
		/* Enqueue VCPU to ready queue */
		if (schedp->current_vcpu != vcpu) {
			rc = vmm_schedalgo_rq_enqueue(schedp->rq, vcpu);
			if (!rc && schedp->current_vcpu) {
				if (vmm_schedalgo_rq_prempt_needed(schedp->rq, 
							schedp->current_vcpu)) {
					vmm_timer_event_expire(&schedp->ev);
				}
			}
		}
		break;
	case VMM_VCPU_STATE_PAUSED:
	case VMM_VCPU_STATE_HALTED:
		/* Expire timer event if current VCPU is paused or halted */
		if(schedp->current_vcpu == vcpu) {
			if (!vcpu->is_normal) {
				vmm_timer_event_expire(&schedp->ev);
			}
		} else {
			/* Make sure VCPU is not in a ready queue */
			if (vcpu->state == VMM_VCPU_STATE_READY) {
				rc = vmm_schedalgo_rq_detach(schedp->rq, vcpu);
			}
		}
		break;
	}

	return rc;
}

void vmm_scheduler_irq_enter(arch_regs_t *regs, bool vcpu_context)
{
	struct vmm_scheduler_ctrl *schedp = &this_cpu(sched);

	/* Indicate that we have entered in IRQ */
	schedp->irq_context = (vcpu_context) ? FALSE : TRUE;

	/* Ensure that yield on exit is disabled */
	schedp->yield_on_irq_exit = FALSE;
}

void vmm_scheduler_irq_exit(arch_regs_t *regs)
{
	struct vmm_scheduler_ctrl *schedp = &this_cpu(sched);
	struct vmm_vcpu *vcpu = NULL;

	/* Determine current vcpu */
	vcpu = schedp->current_vcpu;
	if (!vcpu) {
		return;
	}

	/* Schedule next vcpu if state of 
	 * current vcpu is not RUNNING */
	if ((vcpu->state != VMM_VCPU_STATE_RUNNING) ||
	    schedp->yield_on_irq_exit) {
		vmm_scheduler_next(&schedp->ev, regs);
		schedp->yield_on_irq_exit = FALSE;
	}

	/* VCPU irq processing */
	vmm_vcpu_irq_process(vcpu, regs);

	/* Indicate that we have exited IRQ */
	schedp->irq_context = FALSE;
}

bool vmm_scheduler_irq_context(void)
{
	return this_cpu(sched).irq_context;
}

struct vmm_vcpu * vmm_scheduler_current_vcpu(void)
{
	return this_cpu(sched).current_vcpu;
}

bool vmm_scheduler_orphan_context(void)
{
	struct vmm_scheduler_ctrl *schedp = &this_cpu(sched);

	if (schedp->current_vcpu && !schedp->irq_context) {
		return (schedp->current_vcpu->is_normal) ? FALSE : TRUE;
	}

	return FALSE;
}

bool vmm_scheduler_normal_context(void)
{
	struct vmm_scheduler_ctrl *schedp = &this_cpu(sched);

	if (schedp->current_vcpu && !schedp->irq_context) {
		return (schedp->current_vcpu->is_normal) ? TRUE : FALSE;
	}

	return FALSE;
}

struct vmm_guest * vmm_scheduler_current_guest(void)
{
	struct vmm_vcpu *vcpu = vmm_scheduler_current_vcpu();

	if (vcpu) {
		return vcpu->guest;
	}

	return NULL;
}

void vmm_scheduler_preempt_disable(void)
{
	irq_flags_t flags;
	struct vmm_vcpu * vcpu = vmm_scheduler_current_vcpu();
	if (vcpu) {
		arch_cpu_irq_save(flags);
		vcpu->preempt_count++;
		arch_cpu_irq_restore(flags);
	}
}

void vmm_scheduler_preempt_enable(void)
{
	irq_flags_t flags;
	struct vmm_vcpu * vcpu = vmm_scheduler_current_vcpu();
	if (vcpu && vcpu->preempt_count) {
		arch_cpu_irq_save(flags);
		vcpu->preempt_count--;
		arch_cpu_irq_restore(flags);
	}
}

void vmm_scheduler_yield(void)
{
	struct vmm_scheduler_ctrl *schedp = &this_cpu(sched);
	struct vmm_vcpu * vcpu = NULL; 

	if (vmm_scheduler_irq_context()) {
		vmm_panic("%s: Cannot yield in IRQ context\n", __func__);
	}

	if (vmm_scheduler_orphan_context()) {
		/* For Orphan VCPU
		 * Forcefully expire timeslice 
		 */
		vmm_timer_event_expire(&schedp->ev);
	} else if (vmm_scheduler_normal_context()) {
		/* For Normal VCPU
		 * Just enable yield on exit and rest will be taken care
		 * by vmm_scheduler_irq_exit()
		 */
		vcpu = vmm_scheduler_current_vcpu();
		if (vcpu->state == VMM_VCPU_STATE_RUNNING) {
			schedp->yield_on_irq_exit = TRUE;
		}
	}
}

static void idle_orphan(void)
{
	while(1) {
		if (vmm_schedalgo_rq_length(this_cpu(sched).rq, 
					    IDLE_VCPU_PRIORITY) == 0) {
			arch_cpu_wait_for_irq();
		}

		vmm_scheduler_yield();
	}
}

int __init vmm_scheduler_init(void)
{
	int rc;
	char vcpu_name[32];
	u32 cpu = vmm_smp_processor_id();
	struct vmm_scheduler_ctrl *schedp = &this_cpu(sched);

	/* Reset the scheduler control structure */
	memset(schedp, 0, sizeof(struct vmm_scheduler_ctrl));

	/* Create ready queue (Per Host CPU) */
	schedp->rq = vmm_schedalgo_rq_create();
	if (!schedp->rq) {
		return VMM_EFAIL;
	}

	/* Initialize current VCPU. (Per Host CPU) */
	schedp->current_vcpu = NULL;

	/* Initialize IRQ state (Per Host CPU) */
	schedp->irq_context = FALSE;

	/* Initialize yield on exit (Per Host CPU) */
	schedp->yield_on_irq_exit = FALSE;

	/* Create timer event and start it. (Per Host CPU) */
	INIT_TIMER_EVENT(&schedp->ev, &vmm_scheduler_timer_event, schedp);

	/* Create idle orphan vcpu with default time slice. (Per Host CPU) */
	vmm_sprintf(vcpu_name, "idle/%d", cpu);
	schedp->idle_vcpu = vmm_manager_vcpu_orphan_create(vcpu_name,
						(virtual_addr_t)&idle_orphan,
						IDLE_VCPU_STACK_SZ,
						IDLE_VCPU_PRIORITY, 
						IDLE_VCPU_TIMESLICE);
	if (!schedp->idle_vcpu) {
		return VMM_EFAIL;
	}

	/* Kick idle orphan vcpu */
	if ((rc = vmm_manager_vcpu_kick(schedp->idle_vcpu))) {
		return rc;
	}

	/* Start scheduler timer event */
	vmm_timer_event_start(&schedp->ev, 0);

	/* Mark this CPU online */
	vmm_set_cpu_online(cpu, TRUE);

	return VMM_OK;
}
