/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file arch/sys_arch.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief OS interface header required for lwIP
 */

#ifndef __LWIP_SYS_ARCH_H_
#define __LWIP_SYS_ARCH_H_

#define SYS_MBOX_NULL NULL
#define SYS_SEM_NULL  NULL

struct sys_sem;
typedef struct sys_sem * sys_sem_t;

/* let sys.h use binary semaphores for mutexes */
#define LWIP_COMPAT_MUTEX 1

struct sys_mbox;
typedef struct sys_mbox * sys_mbox_t;

struct sys_thread;
typedef struct sys_thread * sys_thread_t;

#endif /* __LWIP_SYS_ARCH_H_ */
