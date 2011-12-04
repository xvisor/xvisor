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
 * @file vmm_schedalgo.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file declaring interface of a scheduling algorithm
 */
#ifndef _VMM_SCHEDALGO_H__
#define _VMM_SCHEDALGO_H__

#include <vmm_types.h>
#include <vmm_manager.h>

/** Setup newly created VCPU for scheduling algorithm */
int vmm_schedalgo_vcpu_setup(vmm_vcpu_t * vcpu);

/** Cleanup existing VCPU for scheduling algorithm */
int vmm_schedalgo_vcpu_cleanup(vmm_vcpu_t * vcpu);

/** Enqueue VCPU to a ready queue */
int vmm_schedalgo_rq_enqueue(void * rq, vmm_vcpu_t * vcpu);

/** Dequeue VCPU to a ready queue */
vmm_vcpu_t * vmm_schedalgo_rq_dequeue(void * rq, vmm_vcpu_t * current);

/** Detach VCPU from its read queue */
int vmm_schedalgo_rq_detach(vmm_vcpu_t * vcpu);

/** Check if current VCPU is required to be prempted based on current 
 *  ready queue state
 */
bool vmm_schedalgo_rq_prempt_needed(void * rq, vmm_vcpu_t * current);

/** Create a new ready queue */
void * vmm_schedalgo_rq_create(void);

/** Destroy existing ready queue */
int vmm_schedalgo_rq_destroy(void * rq);

#endif
