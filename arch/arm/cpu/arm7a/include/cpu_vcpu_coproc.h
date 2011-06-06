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
 * @file cpu_vcpu_coproc.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for coprocessor access
 */
#ifndef _CPU_VCPU_COPROC_H__
#define _CPU_VCPU_COPROC_H__

#include <vmm_types.h>
#include <vmm_guest.h>

typedef bool (*cpu_coproc_ldcstc_accept)(vmm_vcpu_t * vcpu, 
					 u32 D, u32 CRd, 
					 u32 uopt, u32 imm8);

typedef bool (*cpu_coproc_ldcstc_done)(vmm_vcpu_t * vcpu, 
					u32 index, u32 D, u32 CRd, 
					u32 uopt, u32 imm8);

typedef u32 (*cpu_coproc_ldcstc_read)(vmm_vcpu_t * vcpu,
					u32 index, u32 D, u32 CRd, 
					u32 uopt, u32 imm8);

typedef void (*cpu_coproc_ldcstc_write)(vmm_vcpu_t * vcpu, 
					u32 index, u32 D, u32 CRd, 
					u32 uopt, u32 imm8, u32 data);

typedef bool (*cpu_coproc_read2)(vmm_vcpu_t * vcpu, 
				 u32 opc1, u32 CRm, 
				 u32 *data, u32 *data2);

typedef bool (*cpu_coproc_write2)(vmm_vcpu_t * vcpu, 
				  u32 opc1, u32 CRm, 
				  u32 data, u32 data2);

typedef bool (*cpu_coproc_data_process)(vmm_vcpu_t *vcpu, 
					u32 opc1, u32 opc2, 
					u32 CRd, u32 CRn, u32 CRm);

typedef bool (*cpu_coproc_read)(vmm_vcpu_t * vcpu, 
				u32 opc1, u32 opc2, u32 CRm, 
				u32 *data);

typedef bool (*cpu_coproc_write)(vmm_vcpu_t * vcpu, 
				 u32 opc1, u32 opc2, u32 CRm, 
				 u32 data);

struct cpu_vcpu_coproc {
	u32 cpnum;
	cpu_coproc_ldcstc_accept ldcstc_accept;
	cpu_coproc_ldcstc_done ldcstc_done;
	cpu_coproc_ldcstc_read ldcstc_read;
	cpu_coproc_ldcstc_write ldcstc_write;
	cpu_coproc_read2 read2;
	cpu_coproc_write2 write2;
	cpu_coproc_data_process data_process;
	cpu_coproc_read read;
	cpu_coproc_write write;
};

typedef struct cpu_vcpu_coproc cpu_vcpu_coproc_t;

/** Retrive a coprocessor with given number */
cpu_vcpu_coproc_t *cpu_vcpu_coproc_get(u32 cpnum);

#endif /* _CPU_VCPU_COPROC_H */
