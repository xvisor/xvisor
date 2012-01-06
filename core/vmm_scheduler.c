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
#include <vmm_stdio.h>
#include <vmm_cpu.h>
#include <vmm_vcpu_irq.h>
#include <vmm_timer.h>
#include <vmm_schedalgo.h>
#include <vmm_scheduler.h>

#define IDLE_VCPU_STACK_SZ CONFIG_THREAD_STACK_SIZE
#define IDLE_VCPU_PRIORITY VMM_VCPU_MIN_PRIORITY
#define IDLE_VCPU_TIMESLICE (VMM_VCPU_DEF_TIME_SLICE)

/** Control structure for Scheduler */
struct vmm_scheduler_ctrl {
	void * rq;
	vmm_vcpu_t *current_vcpu;
	vmm_vcpu_t *idle_vcpu;
	u8 idle_vcpu_stack[IDLE_VCPU_STACK_SZ];
	bool irq_context;
	bool yield_on_irq_exit;
	vmm_timer_event_t * ev;
};

static struct vmm_scheduler_ctrl sched;

static void vmm_scheduler_next(vmm_timer_event_t * ev, vmm_user_regs_t * regs)
{
	vmm_vcpu_t *current = sched.current_vcpu;
	vmm_vcpu_t *next = NULL; 

	if (current) {
		/* Normal scheduling */
		if (current->state & VMM_VCPU_STATE_SAVEABLE) {
			if (current->state == VMM_VCPU_STATE_RUNNING) {
				current->state = VMM_VCPU_STATE_READY;
				vmm_schedalgo_rq_enqueue(sched.rq, current);
			}
			next = vmm_schedalgo_rq_dequeue(sched.rq);
			if (next && (next->id != current->id)) {
				vmm_vcpu_regs_switch(current, next, regs);
			}
		} else {
			if (current->state == VMM_VCPU_STATE_READY) {
				vmm_schedalgo_rq_enqueue(sched.rq, current);
			}
			next = vmm_schedalgo_rq_dequeue(sched.rq);
			if (next) {
				vmm_vcpu_regs_switch(NULL, next, regs);
			}
		}
	} else {
		/* First time scheduling */
		next = vmm_schedalgo_rq_dequeue(sched.rq);
		if (next) {
			vmm_vcpu_regs_switch(NULL, next, regs);
		} else {
			/* This should never happen !!! */
			while (1);
		}
	}

	/* Make next VCPU as current VCPU */
	if (next) {
		next->state = VMM_VCPU_STATE_RUNNING;
		sched.current_vcpu = next;
		vmm_timer_event_start(ev, next->time_slice);
	}
}

static void vmm_scheduler_timer_event(vmm_timer_event_t * ev)
{
	vmm_vcpu_t * vcpu = sched.current_vcpu;
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
			if((sched.current_vcpu != vcpu) &&
			   (vcpu->state == VMM_VCPU_STATE_READY)) {
				rc = vmm_schedalgo_rq_detach(sched.rq, vcpu);
			}
		}
		break;
	case VMM_VCPU_STATE_READY:
		/* Enqueue VCPU to ready queue */
		if (sched.current_vcpu != vcpu) {
			rc = vmm_schedalgo_rq_enqueue(sched.rq, vcpu);
			if (!rc && sched.current_vcpu) {
				if (vmm_schedalgo_rq_prempt_needed(sched.rq, 
							sched.current_vcpu)) {
					vmm_timer_event_expire(sched.ev);
				}
			}
		}
		break;
	case VMM_VCPU_STATE_PAUSED:
	case VMM_VCPU_STATE_HALTED:
		/* Expire timer event if current VCPU is paused or halted */
		if(sched.current_vcpu == vcpu) {
			if (!vcpu->is_normal) {
				vmm_timer_event_expire(sched.ev);
			}
		} else {
			/* Make sure VCPU is not in a ready queue */
			if (vcpu->state == VMM_VCPU_STATE_READY) {
				rc = vmm_schedalgo_rq_detach(sched.rq, vcpu);
			}
		}
		break;
	}

	return rc;
}

void vmm_scheduler_irq_enter(vmm_user_regs_t * regs, bool vcpu_context)
{
	/* Indicate that we have entered in IRQ */
	sched.irq_context = (vcpu_context) ? FALSE : TRUE;

	/* Ensure that yield on exit is disabled */
	sched.yield_on_irq_exit = FALSE;
}

void vmm_scheduler_irq_exit(vmm_user_regs_t * regs)
{
	vmm_vcpu_t * vcpu = NULL;

	/* Determine current vcpu */
	vcpu = sched.current_vcpu;
	if (!vcpu) {
		return;
	}

	/* Schedule next vcpu if state of 
	 * current vcpu is not RUNNING */
	if ((vcpu->state != VMM_VCPU_STATE_RUNNING) ||
	    sched.yield_on_irq_exit) {
		vmm_scheduler_next(sched.ev, regs);
		sched.yield_on_irq_exit = FALSE;
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
	return sched.current_vcpu;
}

bool vmm_scheduler_orphan_context(void)
{
	if (sched.current_vcpu && !sched.irq_context) {
		return (sched.current_vcpu->is_normal) ? FALSE : TRUE;
	}

	return FALSE;
}

bool vmm_scheduler_normal_context(void)
{
	if (sched.current_vcpu && !sched.irq_context) {
		return (sched.current_vcpu->is_normal) ? TRUE : FALSE;
	}

	return FALSE;
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
	vmm_vcpu_t * vcpu = NULL; 

	if (vmm_scheduler_irq_context()) {
		vmm_panic("%s: Cannot yield in IRQ context\n", __func__);
	}

	if (vmm_scheduler_orphan_context()) {
		/* For Orphan VCPU
		 * Forcefully expire timeslice 
		 */
		vmm_timer_event_expire(sched.ev);
	} else if (vmm_scheduler_normal_context()) {
		/* For Normal VCPU
		 * Just enable yield on exit and rest will be taken care
		 * by vmm_scheduler_irq_exit()
		 */
		vcpu = vmm_scheduler_current_vcpu();
		if (vcpu->state == VMM_VCPU_STATE_RUNNING) {
			sched.yield_on_irq_exit = TRUE;
		}
	}
}

static void idle_orphan(void)
{
	while(1) {
		if (vmm_schedalgo_rq_length(sched.rq, 
					    IDLE_VCPU_PRIORITY) == 0) {
			vmm_cpu_wait_for_irq();
		}

		vmm_scheduler_yield();
	}
}

int __init vmm_scheduler_init(void)
{
	int rc;

	/* Reset the scheduler control structure */
	vmm_memset(&sched, 0, sizeof(sched));

	/* Create ready queue (Per Host CPU) */
	sched.rq = vmm_schedalgo_rq_create();
	if (!sched.rq) {
		return VMM_EFAIL;
	}

	/* Initialize current VCPU. (Per Host CPU) */
	sched.current_vcpu = NULL;

	/* Initialize IRQ state (Per Host CPU) */
	sched.irq_context = FALSE;

	/* Initialize yield on exit (Per Host CPU) */
	sched.yield_on_irq_exit = FALSE;

	/* Create timer event and start it. (Per Host CPU) */
	sched.ev = vmm_timer_event_create("sched", 
					  &vmm_scheduler_timer_event, 
					  NULL);
	if (!sched.ev) {
		return VMM_EFAIL;
	}

	/* Create idle orphan vcpu with default time slice. (Per Host CPU) */
	sched.idle_vcpu = vmm_manager_vcpu_orphan_create("idle/0",
	(virtual_addr_t)&idle_orphan,
	(virtual_addr_t)&sched.idle_vcpu_stack[IDLE_VCPU_STACK_SZ - 4],
	IDLE_VCPU_PRIORITY, IDLE_VCPU_TIMESLICE);
	if (!sched.idle_vcpu) {
		return VMM_EFAIL;
	}

	/* Kick idle orphan vcpu */
	if ((rc = vmm_manager_vcpu_kick(sched.idle_vcpu))) {
		return rc;
	}

	/* Start scheduler timer event */
	vmm_timer_event_start(sched.ev, 0);

	return VMM_OK;
}
