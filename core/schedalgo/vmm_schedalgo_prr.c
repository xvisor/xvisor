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
 * @file vmm_schedalgo_prr.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief implementation of priority round-robin scheduling algorithm
 */

#include <vmm_error.h>
#include <vmm_list.h>
#include <vmm_heap.h>
#include <vmm_schedalgo.h>

struct vmm_schedalgo_rq_entry {
	struct dlist head;
	vmm_vcpu_t * vcpu;	
};

struct vmm_schedalgo_rq {
	struct dlist list[VMM_VCPU_MAX_PRIORITY+1];
};

int vmm_schedalgo_vcpu_setup(vmm_vcpu_t * vcpu)
{
	struct vmm_schedalgo_rq_entry * rq_entry;

	if (!vcpu) {
		return VMM_EFAIL;
	}

	rq_entry = vmm_malloc(sizeof(struct vmm_schedalgo_rq_entry));
	if (!rq_entry) {
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&rq_entry->head);
	rq_entry->vcpu = vcpu;
	vcpu->sched_priv = rq_entry;

	return VMM_OK;
}

int vmm_schedalgo_vcpu_cleanup(vmm_vcpu_t * vcpu)
{
	if (!vcpu) {
		return VMM_EFAIL;
	}

	if (vcpu->sched_priv) {
		vmm_free(vcpu->sched_priv);
		vcpu->sched_priv = NULL;
	}

	return VMM_OK;
}

int vmm_schedalgo_rq_enqueue(void * rq, vmm_vcpu_t * vcpu)
{
	struct vmm_schedalgo_rq_entry * rq_entry;
	struct vmm_schedalgo_rq * rqi;

	if (!rq || !vcpu) {
		return VMM_EFAIL;
	}

	rqi = rq;
	rq_entry = vcpu->sched_priv;

	if (!rq_entry) {
		return VMM_EFAIL;
	}

	list_add_tail(&rqi->list[vcpu->priority], &rq_entry->head);

	return VMM_OK;
}

vmm_vcpu_t * vmm_schedalgo_rq_dequeue(void * rq)
{
	int p;
	struct dlist *l;
	struct vmm_schedalgo_rq_entry * rq_entry;
	struct vmm_schedalgo_rq * rqi;
	
	if (!rq) {
		return NULL;
	}

	rqi = rq;

	p = VMM_VCPU_MAX_PRIORITY + 1;
	while (p) {
		if (!list_empty(&rqi->list[p-1])) {
			break;
		}
		p--;
	}

	if (!p) {
		return NULL;
	}

	p = p - 1;
	l = list_pop(&rqi->list[p]);
	rq_entry = list_entry(l, struct vmm_schedalgo_rq_entry, head);
	
	return rq_entry->vcpu;
}

int vmm_schedalgo_rq_detach(void * rq, vmm_vcpu_t * vcpu)
{
	struct vmm_schedalgo_rq_entry * rq_entry;

	if (!vcpu) {
		return VMM_EFAIL;
	}

	rq_entry = vcpu->sched_priv;

	if (!rq_entry) {
		return VMM_EFAIL;
	}

	list_del(&rq_entry->head);

	return VMM_OK;
}

bool vmm_schedalgo_rq_prempt_needed(void * rq, vmm_vcpu_t * current)
{
	int p;
	bool ret = FALSE;
	struct vmm_schedalgo_rq * rqi;
	
	if (!rq || !current) {
		return FALSE;
	}

	rqi = rq;

	p = VMM_VCPU_MAX_PRIORITY;
	while (p > current->priority) {
		if (!list_empty(&rqi->list[p])) {
			ret = TRUE;
			break;
		}
		p--;
	}

	return ret;
}

void * vmm_schedalgo_rq_create(void)
{
	int p;
	struct vmm_schedalgo_rq * rq = 
			vmm_malloc(sizeof(struct vmm_schedalgo_rq));

	if (rq) {
		for (p = 0; p <= VMM_VCPU_MAX_PRIORITY; p++) {
			INIT_LIST_HEAD(&rq->list[p]);
		}
	}

	return rq;
}

int vmm_schedalgo_rq_destroy(void * rq)
{
	if (rq) {
		vmm_free(rq);
		return VMM_OK;
	}

	return VMM_EFAIL;
}

