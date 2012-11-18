/**
 * Copyright (c) 2011 Pranav Sawargaonkar.
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
 * @file intc.h
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief OMAP interrupt controller APIs
 */
#ifndef __OMAP_INTC_H__
#define __OMAP_INTC_H__

#include <vmm_types.h>

#define INTC_BITS_PER_REG			32

#define INTC_REVISION				0x00000000
#define INTC_REVISION_REV_S			0
#define INTC_REVISION_REV_M			0x000000FF

#define INTC_SYSCONFIG				0x00000010
#define INTC_SYSCONFIG_SOFTRST_S		1
#define INTC_SYSCONFIG_SOFTRST_M		0x00000002
#define INTC_SYSCONFIG_AUTOIDLE_S		0
#define INTC_SYSCONFIG_AUTOIDLE_M		0x00000001

#define INTC_SYSSTATUS				0x00000014
#define INTC_SYSSTATUS_RESETDONE_S 		0
#define INTC_SYSSTATUS_RESETDONE_M		0x00000001

#define INTC_SIR_IRQ				0x00000040
#define INTC_SIR_IRQ_SPURIOUSFLAG_S		7
#define INTC_SIR_IRQ_SPURIOUSFLAG_M		0xFFFFFF80
#define INTC_SIR_IRQ_ACTIVEIRQ_S		0
#define INTC_SIR_IRQ_ACTIVEIRQ_M		0x0000007F

#define INTC_SIR_FIQ				0x00000044
#define INTC_SIR_FIQ_SPURIOUSFLAG_S		7
#define INTC_SIR_FIQ_SPURIOUSFLAG_M		0xFFFFFF80
#define INTC_SIR_FIQ_ACTIVEIRQ_S		0
#define INTC_SIR_FIQ_ACTIVEIRQ_M		0x0000007F

#define INTC_CONTROL				0x00000048
#define INTC_CONTROL_NEWFIQAGR_S		1
#define INTC_CONTROL_NEWFIQAGR_M		0x00000002
#define INTC_CONTROL_NEWIRQAGR_S		0
#define INTC_CONTROL_NEWIRQAGR_M		0x00000001

#define INTC_PROTECTION				0x0000004C
#define INTC_PROTECTION_PROTECTION_S		0
#define INTC_PROTECTION_PROTECTION_M		0x00000001

#define INTC_IDLE				0x00000050
#define INTC_IDLE_TURBO_S			1
#define INTC_IDLE_TURBO_M			0x00000002
#define INTC_IDLE_FUNCIDLE_S			0
#define INTC_IDLE_FUNCIDLE_M			0x00000001

#define INTC_IRQ_PRIORITY			0x00000060
#define INTC_IRQ_PRIORITY_SPURIOUSFLAG_S	6
#define INTC_IRQ_PRIORITY_SPURIOUSFLAG_M	0xFFFFFFC0
#define INTC_IRQ_PRIORITY_ACTIVEIRQ_S		0
#define INTC_IRQ_PRIORITY_IRQPRIORITY_M		0x0000003F

#define INTC_FIQ_PRIORITY			0x00000064
#define INTC_FIQ_PRIORITY_SPURIOUSFLAG_S	6
#define INTC_FIQ_PRIORITY_SPURIOUSFLAG_M	0xFFFFFFC0
#define INTC_FIQ_PRIORITY_ACTIVEIRQ_S		0
#define INTC_FIQ_PRIORITY_IRQPRIORITY_M		0x0000003F

#define INTC_THRESHOLD				0x00000068
#define INTC_THRESHOLD_PRIOTHRESHOLD_S		0
#define INTC_THRESHOLD_PRIOTHRESHOLD_M		0x000000FF

#define INTC_ITR(n)				(0x00000080+(0x20*(n)))

#define INTC_MIR(n)				(0x00000084+(0x20*(n)))

#define INTC_MIR_CLEAR(n)			(0x00000088+(0x20*(n)))

#define INTC_MIR_SET(n)				(0x0000008C+(0x20*(n)))

#define INTC_ISR_SET(n)				(0x00000090+(0x20*(n)))

#define INTC_ISR_CLEAR(n)			(0x00000094+(0x20*(n)))

#define INTC_PENDING_IRQ(n)			(0x00000098+(0x20*(n)))

#define INTC_PENDING_FIQ(n)			(0x0000009C+(0x20*(n)))

#define INTC_ILR(m)				(0x00000100+(0x04*(m)))
#define INTC_ILR_PRIORITY_S			2
#define INTC_ILR_PRIORITY_M			0x000000FC
#define INTC_ILR_FIQNIRQ_S			1
#define INTC_ILR_FIQNIRQ_M			0x00000001

u32 intc_active_irq(u32 cpu_irq);
int intc_init(physical_addr_t base, u32 nrirq);

#endif
