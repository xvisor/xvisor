/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file arm-gic.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file for ARM GICv2 interrupt controller.
 */

#ifndef __ARM_GIC_H__
#define __ARM_GIC_H__

#define GICC_CTRL			0x00
#define GICC_PRIMASK			0x04
#define GICC_BINPOINT			0x08
#define GICC_INTACK			0x0c
#define GICC_EOI			0x10
#define GICC_RUNNINGPRI			0x14
#define GICC_HIGHPRI			0x18

#define GICC2_DIR			0x00

#define GICD_CTRL			0x000
#define GICD_CTR			0x004
#define GICD_ENABLE_SET			0x100
#define GICD_ENABLE_CLEAR		0x180
#define GICD_PENDING_SET		0x200
#define GICD_PENDING_CLEAR		0x280
#define GICD_ACTIVE_SET			0x300
#define GICD_ACTIVE_CLEAR		0x380
#define GICD_PRI			0x400
#define GICD_TARGET			0x800
#define GICD_CONFIG			0xc00
#define GICD_SOFTINT			0xf00

#define GICH_HCR			0x0
#define GICH_VTR			0x4
#define GICH_VMCR			0x8
#define GICH_MISR			0x10
#define GICH_EISR0 			0x20
#define GICH_EISR1 			0x24
#define GICH_ELRSR0 			0x30
#define GICH_ELRSR1 			0x34
#define GICH_APR			0xf0
#define GICH_LR0			0x100

#define GICH_HCR_EN			(1 << 0)
#define GICH_HCR_UIE			(1 << 1)

#define GICH_VTR_LRCNT_MASK		0x3f

#define GICH_LR_MAX_COUNT		0x40

#define GICH_LR_HW			(1 << 31)
#define GICH_LR_STATE			(3 << 28)
#define GICH_LR_PENDING			(1 << 28)
#define GICH_LR_ACTIVE			(1 << 29)
#define GICH_LR_PRIO_SHIFT		(23)
#define GICH_LR_PRIO			(0x1F << GICH_LR_PRIO_SHIFT)
#define GICH_LR_PHYSID_SHIFT		(10)
#define GICH_LR_PHYSID			(0x3ff << GICH_LR_PHYSID_SHIFT)
#define GICH_LR_PHYSID_EOI_SHIFT	(19)
#define GICH_LR_PHYSID_EOI		(1 << GICH_LR_PHYSID_EOI_SHIFT)
#define GICH_LR_PHYSID_CPUID_SHIFT	(10)
#define GICH_LR_PHYSID_CPUID		(7 << GICH_LR_PHYSID_CPUID_SHIFT)
#define GICH_LR_VIRTUALID		(0x3ff << 0)

#define GICH_MISR_EOI			(1 << 0)
#define GICH_MISR_U			(1 << 1)

#endif /* __ARM_GIC_H__ */
