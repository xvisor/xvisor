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
 * @file vmm_mutex.h
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file of mutext locks for Orphan VCPU (or Thread).
 */

#ifndef __VMM_MUTEX_H__
#define __VMM_MUTEX_H__

#include <vmm_semaphore.h>

#define vmm_mutex				vmm_semaphore

#define INIT_MUTEX(mptr)			INIT_SEMAPHORE(mptr, 1)

#define vmm_mutex_avail(mut)			vmm_semaphore_avail(mut)

#define vmm_mutex_lock(mut)			vmm_semaphore_dowm(mut)

#define vmm_mutex_unlock(mut)			vmm_semaphore_up(mut)

#endif /* __VMM_MUTEX_H__ */
