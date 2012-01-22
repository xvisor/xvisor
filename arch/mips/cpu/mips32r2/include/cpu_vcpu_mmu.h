/*
 * Copyright (c) 2010-2020 Himanshu Chauhan.
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
 * @author: Himanshu Chauhan <hschauhan@nulltrace.org>
 * @brief: VCPU MMU handling function and structures.
 */

#ifndef __CPU_VCPU_MMU_H_
#define __CPU_VCPU_MMU_H_

#include <vmm_types.h>
#include <arch_regs.h>

int do_vcpu_tlbmiss(arch_regs_t *uregs);
u32 mips_probe_vcpu_tlb(struct vmm_vcpu *vcpu, arch_regs_t *uregs);
u32 mips_read_vcpu_tlb(struct vmm_vcpu *vcpu, arch_regs_t *uregs);
u32 mips_write_vcpu_tlbi(struct vmm_vcpu *vcpu, arch_regs_t *uregs);
u32 mips_write_vcpu_tlbr(struct vmm_vcpu *vcpu, arch_regs_t *uregs);

#endif /* __CPU_VCPU_MMU_H_ */
