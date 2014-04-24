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
 * @file vmm_schedalgo_rr.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief implementation of round-robin scheduling algorithm
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_schedalgo.h>
#include <libs/list.h>

struct vmm_schedalgo_rq_entry {
	struct dlist head;
	struct vmm_vcpu *vcpu;	
};

struct vmm_schedalgo_rq {
	struct dlist list;
};

int vmm_schedalgo_vcpu_setup(struct vmm_vcpu *vcpu)
{
	struct vmm_schedalgo_rq_entry *rq_entry;

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

int vmm_schedalgo_vcpu_cleanup(struct vmm_vcpu *vcpu)
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

int vmm_schedalgo_rq_length(void *rq, u8 priority)
{
	struct vmm_schedalgo_rq_entry *rq_entry;
	struct vmm_schedalgo_rq *rqi;
	int count = 0;

	if (!rq) {
		return -1;
	}

	rqi = rq;

	list_for_each_entry(rq_entry, &rqi->list, head) {
		count++;
	}

	return count;

}

int vmm_schedalgo_rq_enqueue(void *rq, struct vmm_vcpu *vcpu)
{
	struct vmm_schedalgo_rq_entry *rq_entry;
	struct vmm_schedalgo_rq *rqi;

	if (!rq || !vcpu) {
		return VMM_EFAIL;
	}

	rqi = rq;
	rq_entry = vcpu->sched_priv;

	if (!rq_entry) {
		return VMM_EFAIL;
	}

	list_add_tail(&rq_entry->head, &rqi->list);

	return VMM_OK;
}

struct vmm_vcpu *vmm_schedalgo_rq_dequeue(void *rq)
{
	struct vmm_schedalgo_rq_entry *rq_entry;
	struct vmm_schedalgo_rq *rqi;
	
	if (!rq) {
		return NULL;
	}

	rqi = rq;

	if (list_empty(&rqi->list)) {
		return NULL;
	}

	rq_entry = list_first_entry(&rqi->list,
				struct vmm_schedalgo_rq_entry, head);
	list_del(&rq_entry->head);
	
	return rq_entry->vcpu;
}

int vmm_schedalgo_rq_detach(void *rq, struct vmm_vcpu *vcpu)
{
	struct vmm_schedalgo_rq_entry *rq_entry;

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

bool vmm_schedalgo_rq_prempt_needed(void *rq, struct vmm_vcpu *current)
{
	return FALSE;
}

void *vmm_schedalgo_rq_create(void)
{
	struct vmm_schedalgo_rq *rq = 
			vmm_malloc(sizeof(struct vmm_schedalgo_rq));

	if (rq) {
		INIT_LIST_HEAD(&rq->list);
	}

	return rq;
}

int vmm_schedalgo_rq_destroy(void *rq)
{
	if (rq) {
		vmm_free(rq);
		return VMM_OK;
	}

	return VMM_EFAIL;
}

