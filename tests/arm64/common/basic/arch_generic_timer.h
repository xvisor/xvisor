/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file arch_generic_timer.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file for arch specific generic timer access
 */

#ifndef __ARCH_GENERIC_TIMER_H__
#define __ARCH_GENERIC_TIMER_H__

#include <arch_types.h>
#include <arm_inline_asm.h>

#define arch_read_cntfrq()	mrs(cntfrq_el0)

#define arch_read_cntv_ctl()	mrs(cntv_ctl_el0)

#define arch_write_cntv_ctl(val) msr(cntv_ctl_el0, (val))

#define arch_read_cntv_cval()	mrs(cntv_cval_el1)

#define arch_write_cntv_cval(val) msr(cntv_cval_el1, (val))

#define arch_read_cntv_tval()	mrs(cntv_tval_el0)

#define arch_write_cntv_tval(val) msr(cntv_tval_el0, (val))

#define arch_read_cntvct()	mrs(cntvct_el0)

#endif /* __ARCH_GENERIC_TIMER_H__ */
