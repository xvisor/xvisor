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
 * @file emulate_psci.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARM PSCI emulation APIs
 */

#ifndef __EMULATE_ARM_PSCI_H__
#define __EMULATE_ARM_PSCI_H__

#include <vmm_types.h>
#include <vmm_manager.h>

/* Emulate PSCI interface */
#define EMU_PSCI_FN_BASE		0x95c1ba5e
#define EMU_PSCI_FN(n)			(EMU_PSCI_FN_BASE + (n))

#define EMU_PSCI_FN_CPU_SUSPEND		EMU_PSCI_FN(0)
#define EMU_PSCI_FN_CPU_OFF		EMU_PSCI_FN(1)
#define EMU_PSCI_FN_CPU_ON		EMU_PSCI_FN(2)
#define EMU_PSCI_FN_MIGRATE		EMU_PSCI_FN(3)

#define EMU_PSCI_RET_SUCCESS		0
#define EMU_PSCI_RET_NI			((unsigned long)-1)
#define EMU_PSCI_RET_INVAL		((unsigned long)-2)
#define EMU_PSCI_RET_DENIED		((unsigned long)-3)
#define EMU_PSCI_RET_ALREADY_ON		((unsigned long)-4)
#define EMU_PSCI_RET_ON_PENDING		((unsigned long)-5)
#define EMU_PSCI_RET_INTERNAL_FAILURE	((unsigned long)-6)
#define EMU_PSCI_RET_NOT_PRESENT	((unsigned long)-7)
#define EMU_PSCI_RET_DISABLED		((unsigned long)-8)

/* Emulate PSCI call from Guest VCPU */
int emulate_psci_call(struct vmm_vcpu *vcpu, arch_regs_t *regs, bool is_smc);

#endif /* __EMULATE_ARM_PSCI_H__ */
