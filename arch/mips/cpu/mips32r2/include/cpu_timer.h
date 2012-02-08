/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file cpu_timer.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief header file for cpu timer handling functions
 */
#ifndef _CPU_TIMER_H__
#define _CPU_TIMER_H__

#include <vmm_types.h>
#include <vmm_scheduler.h>

#define VCPU_TIMER_FREQ_MHZ			VMM_VCPU_TIMER_FREQ_MHZ
#define VCPU_DECREMENTER_SCALE			(VCPU_TIMER_FREQ_MHZ*CPU_DECREMENTER_TIMEOUT_MICROSECS)

#endif
