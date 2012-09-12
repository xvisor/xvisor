/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @file ca9x4_board.h
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief Core Tile Cortex A9x4 board configuration
 */
#ifndef _CA9X4_BOARD_H__
#define _CA9X4_BOARD_H__

/*
 * On-Chip Peripherials Physical Addresses
 */
#define CT_CA9X4_CLCDC			(0x10020000)
#define CT_CA9X4_AXIRAM			(0x10060000)
#define CT_CA9X4_DMC			(0x100e0000)
#define CT_CA9X4_SMC			(0x100e1000)
#define CT_CA9X4_SCC			(0x100e2000)
#define CT_CA9X4_SP804_TIMER		(0x100e4000)
#define CT_CA9X4_SP805_WDT		(0x100e5000)
#define CT_CA9X4_TZPC			(0x100e6000)
#define CT_CA9X4_GPIO			(0x100e8000)
#define CT_CA9X4_FASTAXI		(0x100e9000)
#define CT_CA9X4_SLOWAXI		(0x100ea000)
#define CT_CA9X4_TZASC			(0x100ec000)
#define CT_CA9X4_CORESIGHT		(0x10200000)
#define CT_CA9X4_MPIC			(0x1e000000)
#define CT_CA9X4_SYSTIMER		(0x1e004000)
#define CT_CA9X4_SYSWDT			(0x1e007000)
#define CT_CA9X4_L2CC			(0x1e00a000)

#define CT_CA9X4_TIMER0			(CT_CA9X4_SP804_TIMER + 0x000)
#define CT_CA9X4_TIMER1			(CT_CA9X4_SP804_TIMER + 0x020)

#define A9_MPCORE_SCU			(CT_CA9X4_MPIC + 0x0000)
#define A9_MPCORE_GIC_CPU		(CT_CA9X4_MPIC + 0x0100)
#define A9_MPCORE_GIT			(CT_CA9X4_MPIC + 0x0200)
#define A9_MPCORE_TWD			(CT_CA9X4_MPIC + 0x0600)
#define A9_MPCORE_GIC_DIST		(CT_CA9X4_MPIC + 0x1000)

/*
 * Interrupts.  Those in {} are for AMBA devices
 */
#define IRQ_CT_CA9X4_CLCDC		{ 76 }
#define IRQ_CT_CA9X4_DMC		{ -1 }
#define IRQ_CT_CA9X4_SMC		{ 77, 78 }
#define IRQ_CT_CA9X4_TIMER0		80
#define IRQ_CT_CA9X4_TIMER1		81
#define IRQ_CT_CA9X4_GPIO		{ 82 }
#define IRQ_CT_CA9X4_PMU_CPU0		92
#define IRQ_CT_CA9X4_PMU_CPU1		93
#define IRQ_CT_CA9X4_PMU_CPU2		94
#define IRQ_CT_CA9X4_PMU_CPU3		95

#define IRQ_CT_CA9X4_LOCALTIMER		29
#define IRQ_CT_CA9X4_LOCALWDOG		30

#define IRQ_CA9X4_GIC_START		29
#define NR_IRQS_CA9X4			128
#define NR_GIC_CA9X4			1

#endif
