/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file vmm_completion.h
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file of completion locks for Orphan VCPU (or Thread).
 */

#ifndef __VMM_COMPLETION_H__
#define __VMM_COMPLETION_H__

#include <vmm_waitqueue.h>

#define vmm_completion				vmm_waitqueue
#define vmm_completion_t			vmm_waitqueue_t

#define INIT_COMPLETION(cptr)			INIT_WAITQUEUE(cptr)

static inline bool vmm_completion_done(vmm_completion_t * cmpl)
{
	return vmm_waitqueue_count(cmpl) ? FALSE : TRUE;
}

#define vmm_completion_wait(cmpl)		vmm_waitqueue_sleep(cmpl)

#define vmm_completion_complete_first(cmpl)	vmm_waitqueue_wakefirst(cmpl)

#define vmm_completion_complete_all(cmpl)	vmm_waitqueue_wakeall(cmpl)

#endif /* __VMM_COMPLETION_H__ */
