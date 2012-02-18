/**
 * Copyright (c) 2011 Himanshu Chauhan
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
 * @file cpu_vcpu_helper.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief source of VCPU helper functions
 */

#include <arch_cpu.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_stdio.h>
#include <vmm_manager.h>
#include <vmm_guest_aspace.h>
#include <cpu_mmu.h>

extern char _stack_start;

#define VMM_REGION_TYPE_ROM	0
#define VMM_REGION_TYPE_RAM	1

static int map_guest_region(struct vmm_vcpu *vcpu, int region_type, int tlb_index)
{
	return VMM_OK;
}

int arch_vcpu_regs_init(struct vmm_vcpu *vcpu)
{
	return VMM_OK;
}

int arch_vcpu_regs_deinit(struct vmm_vcpu * vcpu)
{
	return VMM_OK;
}

void arch_vcpu_regs_switch(struct vmm_vcpu *tvcpu, 
			  struct vmm_vcpu *vcpu,
			  arch_regs_t *regs)
{
}

void arch_vcpu_regs_dump(struct vmm_vcpu *vcpu) 
{
}

void arch_vcpu_stat_dump(struct vmm_vcpu *vcpu) 
{
}
