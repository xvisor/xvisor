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
 * @file cpu_emulate_thumb.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file to emulate Thumb instructions
 */
#ifndef _CPU_VCPU_EMULATE_THUMB_H__
#define _CPU_VCPU_EMULATE_THUMB_H__

#include <vmm_types.h>

/** Emulate Priviledged Thumb instructions */
int cpu_vcpu_emulate_thumb_inst(vmm_vcpu_t *vcpu, 
				vmm_user_regs_t * regs, 
				bool is_hypercall);

#endif
