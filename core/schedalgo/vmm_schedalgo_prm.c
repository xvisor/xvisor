/**
 * Copyright (c) 2015 Ossama Benbouidda.
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
 * @file vmm_schedalgo_prm.c
 * @author Ossama Benbouidda (ossama.benbouidda@gmail.com)
 * @brief Implementation of rate monotonic scheduling algorithm
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_schedalgo.h>
#include <libs/rbtree_augmented.h>

struct vmm_schedalgo_rq_entry {
	struct rb_node rb;
	struct vmm_vcpu *vcpu;
	u64 periodicity;
};

struct vmm_schedalgo_rq {
	u32 count[VMM_VCPU_MAX_PRIORITY+1];
	struct rb_root root[VMM_VCPU_MAX_PRIORITY+1];
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

	RB_CLEAR_NODE(&rq_entry->rb);
	rq_entry->vcpu = vcpu;
	rq_entry->periodicity = 0;
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
	struct vmm_schedalgo_rq *rqi = rq;

	if (!rqi) {
		return -1;
	}

	return rqi->count[priority];
}

int vmm_schedalgo_rq_enqueue(void *rq, struct vmm_vcpu *vcpu)
{
	struct vmm_schedalgo_rq_entry *rq_entry, *parent_e;
	struct vmm_schedalgo_rq *rqi = rq;
	struct rb_node **new = NULL, *parent = NULL;

	if (!rqi || !vcpu) {
		return VMM_EFAIL;
	}

	rq_entry = vcpu->sched_priv;
	if (!rq_entry) {
		return VMM_EFAIL;
	}

	new = &(rqi->root[vcpu->priority].rb_node);
	while (*new) {
		parent = *new;
		parent_e = rb_entry(parent, struct vmm_schedalgo_rq_entry, rb);
		if (vcpu->periodicity < parent_e->periodicity) {
			new = &parent->rb_left;
		} else if (parent_e->periodicity <= vcpu->periodicity) {
			new = &parent->rb_right;
		} else {
			return VMM_EFAIL;
		}
	}
	rq_entry->periodicity = vcpu->periodicity;
	rb_link_node(&rq_entry->rb, parent, new);
	rb_insert_color(&rq_entry->rb, &rqi->root[vcpu->priority]);
	rqi->count[vcpu->priority]++;

	return VMM_OK;
}

int vmm_schedalgo_rq_dequeue(void *rq,
			     struct vmm_vcpu **next,
			     u64 *next_time_slice)
{
	int p;
	struct rb_node *n;
	struct vmm_schedalgo_rq_entry *rq_entry;
	struct vmm_schedalgo_rq *rqi = rq;
	
	if (!rqi) {
		return VMM_EFAIL;
	}

	p = VMM_VCPU_MAX_PRIORITY + 1;
	while (p) {
		if (rqi->count[p-1]) {
			break;
		}
		p--;
	}
	if (!p) {
		return VMM_ENOTAVAIL;
	}
	p = p - 1;

	rq_entry = NULL;
	n = rqi->root[p].rb_node;
	while (n && n->rb_left) {
		n = n->rb_left;
	}
	if (!n) {
		return VMM_ENOTAVAIL;
	}
	rq_entry = rb_entry(n, struct vmm_schedalgo_rq_entry, rb);
	rb_erase(&rq_entry->rb, &rqi->root[p]);
	rq_entry->periodicity = 0;
	rqi->count[p]--;

	if (next) {
		*next = rq_entry->vcpu;
	}
	if (next_time_slice) {
		*next_time_slice = rq_entry->vcpu->time_slice;
	}

	return VMM_OK;
}

int vmm_schedalgo_rq_detach(void *rq, struct vmm_vcpu *vcpu)
{
	struct vmm_schedalgo_rq_entry *rq_entry;
	struct vmm_schedalgo_rq *rqi = rq;

	if (!vcpu || !rqi) {
		return VMM_EFAIL;
	}

	rq_entry = vcpu->sched_priv;
	if (!rq_entry) {
		return VMM_EFAIL;
	}

	rb_erase(&rq_entry->rb, &rqi->root[vcpu->priority]);
	rq_entry->periodicity = 0;
	rqi->count[vcpu->priority]--;

	return VMM_OK;
}

bool vmm_schedalgo_rq_prempt_needed(void *rq, struct vmm_vcpu *current)
{
	int p;
	bool ret = FALSE;
	struct vmm_schedalgo_rq *rqi;
	
	if (!rq || !current) {
		return FALSE;
	}

	rqi = rq;

	p = VMM_VCPU_MAX_PRIORITY;
	while (p > current->priority) {
		if (rqi->count[p]) {
			ret = TRUE;
			break;
		}
		p--;
	}

	/* TODO: check lowest periodicity of highest priority with vcpu */

	return ret;
}

void *vmm_schedalgo_rq_create(void)
{
	int p;
	struct vmm_schedalgo_rq *rq =
			vmm_zalloc(sizeof(struct vmm_schedalgo_rq));

	if (!rq) {
		return NULL;
	}

	for (p = 0; p <= VMM_VCPU_MAX_PRIORITY; p++) {
		rq->count[p] = 0;
		rq->root[p] = RB_ROOT;
	}

	return rq;
}

int vmm_schedalgo_rq_destroy(void *rq)
{
	if (!rq) {
		return VMM_EFAIL;
	}

	vmm_free(rq);
	return VMM_OK;
}

