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
 * @file ca15x4_board.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Core Tile Cortex A15x4 board configuration
 */
#ifndef _CT_CA15X4_H__
#define _CT_CA15X4_H__

/*
 * On-Chip Peripherials Physical Addresses
 */
    /* 0x2a430000: system counter: not modelled */
    /* 0x2e000000: system SRAM */
    /* 0x7ffb0000: DMA330 DMA controller: not modelled */
    /* 0x7ffd0000: PL354 static memory controller: not modelled */
/* HDLCD controller: not modelled */
#define CT_CA15X4_HDLCD			(0x2b000000)
#define CT_CA15X4_AXIRAM		(0x10060000)
/* PL341 dynamic memory controller: not modelled */
#define CT_CA15X4_DMC			(0x2b0a0000)
#define CT_CA15X4_SMC			(0x100e1000)
/* SCC: not modelled */
#define CT_CA15X4_SCC			(0x2a420000)
#define CT_CA15X4_SP804_TIMER		(0x100e4000)
/* SP805 watchdog: not modelled */
#define CT_CA15X4_SP805_WDT		(0x2b060000)
/* PL301 AXI interconnect: not modelled */
#define CT_CA15X4_AXI			(0x2a000000)
/* CoreSight interfaces: not modelled */
#define CT_CA15X4_CORESIGHT		(0x20000000)
/* A15MPCore private memory region (GIC) */
#define CT_CA15X4_MPIC			(0x2c000000)

#define A15_MPCORE_GIC_CPU		(CT_CA15X4_MPIC + 0x2000)
#define A15_MPCORE_GIC_DIST		(CT_CA15X4_MPIC + 0x1000)

/*
 * Interrupts.  Those in {} are for AMBA devices
 */
#define IRQ_CT_CA15X4_HDLCD		{ 76 }
#define IRQ_CT_CA15X4_DMC		{ -1 }
#define IRQ_CT_CA15X4_SMC		{ 77, 78 }
#define IRQ_CT_CA15X4_TIMER0		80
#define IRQ_CT_CA15X4_TIMER1		81
#define IRQ_CT_CA15X4_GPIO		{ 82 }
#define IRQ_CT_CA15X4_PMU_CPU0		92
#define IRQ_CT_CA15X4_PMU_CPU1		93
#define IRQ_CT_CA15X4_PMU_CPU2		94
#define IRQ_CT_CA15X4_PMU_CPU3		95

#endif
