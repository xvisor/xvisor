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
 * @file ca9x4_board.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Core Tile Cortex A9x4 board configuration
 */
#ifndef _CA9X4_BOARD_H__
#define _CA9X4_BOARD_H__

/*
 * Peripheral addresses
 */
#define VEXPRESS_CA9X4_UART0_BASE		0x10009000	/* UART 0 */
#define VEXPRESS_CA9X4_UART1_BASE		0x1000A000	/* UART 1 */
#define VEXPRESS_CA9X4_UART2_BASE		0x1000B000	/* UART 2 */
#define VEXPRESS_CA9X4_UART3_BASE		0x1000C000	/* UART 3 */
#define VEXPRESS_CA9X4_SSP_BASE			0x1000D000	/* Synchronous Serial Port */
#define VEXPRESS_CA9X4_WATCHDOG0_BASE		0x1000F000	/* Watchdog 0 */
#define VEXPRESS_CA9X4_WATCHDOG_BASE		0x10010000	/* watchdog interface */
#define VEXPRESS_CA9X4_TIMER0_1_BASE		0x10011000	/* Timer 0 and 1 */
#define VEXPRESS_CA9X4_TIMER2_3_BASE		0x10012000	/* Timer 2 and 3 */
#define VEXPRESS_CA9X4_GPIO0_BASE		0x10013000	/* GPIO port 0 */
#define VEXPRESS_CA9X4_RTC_BASE			0x10017000	/* Real Time Clock */
#define VEXPRESS_CA9X4_TIMER4_5_BASE		0x10018000	/* Timer 4/5 */
#define VEXPRESS_CA9X4_TIMER6_7_BASE		0x10019000	/* Timer 6/7 */
#define VEXPRESS_CA9X4_SCTL_BASE			0x1001A000	/* System Controller */
#define VEXPRESS_CA9X4_CLCD_BASE			0x10020000	/* CLCD */
#define VEXPRESS_CA9X4_ONB_SRAM_BASE		0x10060000	/* On-board SRAM */
#define VEXPRESS_CA9X4_DMC_BASE			0x100E0000	/* DMC configuration */
#define VEXPRESS_CA9X4_SMC_BASE			0x100E1000	/* SMC configuration */
#define VEXPRESS_CA9X4_CAN_BASE			0x100E2000	/* CAN bus */
#define VEXPRESS_CA9X4_GIC_CPU_BASE		0x1E000000	/* Generic interrupt controller CPU interface */
#define VEXPRESS_CA9X4_FLASH0_BASE		0x40000000
#define VEXPRESS_CA9X4_FLASH0_SIZE		SZ_64M
#define VEXPRESS_CA9X4_FLASH1_BASE		0x44000000
#define VEXPRESS_CA9X4_FLASH1_SIZE		SZ_64M
#define VEXPRESS_CA9X4_ETH_BASE			0x4E000000	/* Ethernet */
#define VEXPRESS_CA9X4_USB_BASE			0x4F000000	/* USB */
#define VEXPRESS_CA9X4_GIC_DIST_BASE		0x1E001000	/* Generic interrupt controller distributor */
#define VEXPRESS_CA9X4_LT_BASE			0xC0000000	/* Logic Tile expansion */
#define VEXPRESS_CA9X4_SDRAM6_BASE		0x70000000	/* SDRAM bank 6 256MB */
#define VEXPRESS_CA9X4_SDRAM7_BASE		0x80000000	/* SDRAM bank 7 256MB */

#define VEXPRESS_CA9X4_SYS_PLD_CTRL1		0x74

/*
 * CA9X4 PCI regions
 */
#define VEXPRESS_CA9X4_PCI_BASE			0x90040000	/* PCI-X Unit base */
#define VEXPRESS_CA9X4_PCI_IO_BASE		0x90050000	/* IO Region on AHB */
#define VEXPRESS_CA9X4_PCI_MEM_BASE		0xA0000000	/* MEM Region on AHB */

#define VEXPRESS_CA9X4_PCI_BASE_SIZE		0x10000	/* 16 Kb */
#define VEXPRESS_CA9X4_PCI_IO_SIZE		0x1000	/* 4 Kb */
#define VEXPRESS_CA9X4_PCI_MEM_SIZE		0x20000000	/* 512 MB */

/*
 * Irqs
 */
#define IRQ_CA9X4_GIC_START			32

/* L220
#define IRQ_CA9X4_L220_EVENT	(IRQ_CA9X4_GIC_START + 29)
#define IRQ_CA9X4_L220_SLAVE	(IRQ_CA9X4_GIC_START + 30)
#define IRQ_CA9X4_L220_DECODE	(IRQ_CA9X4_GIC_START + 31)
*/

/*
 * CA9X4 on-board gic irq sources
 */
#define IRQ_CA9X4_WATCHDOG	(IRQ_CA9X4_GIC_START + 0)	/* Watchdog timer */
#define IRQ_CA9X4_SOFT		(IRQ_CA9X4_GIC_START + 1)	/* Software interrupt */
#define IRQ_CA9X4_COMMRx	(IRQ_CA9X4_GIC_START + 2)	/* Debug Comm Rx interrupt */
#define IRQ_CA9X4_COMMTx	(IRQ_CA9X4_GIC_START + 3)	/* Debug Comm Tx interrupt */
#define IRQ_CA9X4_TIMER0_1	(IRQ_CA9X4_GIC_START + 4)	/* Timer 0/1 (default timer) */
#define IRQ_CA9X4_TIMER2_3	(IRQ_CA9X4_GIC_START + 5)	/* Timer 2/3 */
#define IRQ_CA9X4_GPIO0		(IRQ_CA9X4_GIC_START + 6)	/* GPIO 0 */
#define IRQ_CA9X4_GPIO1		(IRQ_CA9X4_GIC_START + 7)	/* GPIO 1 */
#define IRQ_CA9X4_GPIO2		(IRQ_CA9X4_GIC_START + 8)	/* GPIO 2 */
								/* 9 reserved */
#define IRQ_CA9X4_RTC		(IRQ_CA9X4_GIC_START + 10)	/* Real Time Clock */
#define IRQ_CA9X4_SSP		(IRQ_CA9X4_GIC_START + 11)	/* Synchronous Serial Port */
#define IRQ_CA9X4_UART0		(IRQ_CA9X4_GIC_START + 12)	/* UART 0 on development chip */
#define IRQ_CA9X4_UART1		(IRQ_CA9X4_GIC_START + 13)	/* UART 1 on development chip */
#define IRQ_CA9X4_UART2		(IRQ_CA9X4_GIC_START + 14)	/* UART 2 on development chip */
#define IRQ_CA9X4_UART3		(IRQ_CA9X4_GIC_START + 15)	/* UART 3 on development chip */
#define IRQ_CA9X4_SCI		(IRQ_CA9X4_GIC_START + 16)	/* Smart Card Interface */
#define IRQ_CA9X4_MMCI0A	(IRQ_CA9X4_GIC_START + 17)	/* Multimedia Card 0A */
#define IRQ_CA9X4_MMCI0B	(IRQ_CA9X4_GIC_START + 18)	/* Multimedia Card 0B */
#define IRQ_CA9X4_AACI		(IRQ_CA9X4_GIC_START + 19)	/* Audio Codec */
#define IRQ_CA9X4_KMI0		(IRQ_CA9X4_GIC_START + 20)	/* Keyboard/Mouse port 0 */
#define IRQ_CA9X4_KMI1		(IRQ_CA9X4_GIC_START + 21)	/* Keyboard/Mouse port 1 */
#define IRQ_CA9X4_CHARLCD	(IRQ_CA9X4_GIC_START + 22)	/* Character LCD */
#define IRQ_CA9X4_CLCD		(IRQ_CA9X4_GIC_START + 23)	/* CLCD controller */
#define IRQ_CA9X4_DMAC		(IRQ_CA9X4_GIC_START + 24)	/* DMA controller */
#define IRQ_CA9X4_PWRFAIL	(IRQ_CA9X4_GIC_START + 25)	/* Power failure */
#define IRQ_CA9X4_PISMO		(IRQ_CA9X4_GIC_START + 26)	/* PISMO interface */
#define IRQ_CA9X4_DoC		(IRQ_CA9X4_GIC_START + 27)	/* Disk on Chip memory controller */
#define IRQ_CA9X4_ETH		(IRQ_CA9X4_GIC_START + 28)	/* Ethernet controller */
#define IRQ_CA9X4_USB		(IRQ_CA9X4_GIC_START + 29)	/* USB controller */
#define IRQ_CA9X4_TSPEN		(IRQ_CA9X4_GIC_START + 30)	/* Touchscreen pen */
#define IRQ_CA9X4_TSKPAD	(IRQ_CA9X4_GIC_START + 31)	/* Touchscreen keypad */

/* ... */
#define IRQ_CA9X4_PCI0		(IRQ_CA9X4_GIC_START + 50)
#define IRQ_CA9X4_PCI1		(IRQ_CA9X4_GIC_START + 51)
#define IRQ_CA9X4_PCI2		(IRQ_CA9X4_GIC_START + 52)
#define IRQ_CA9X4_PCI3		(IRQ_CA9X4_GIC_START + 53)

#define IRQ_CA9X4_SMC		-1
#define IRQ_CA9X4_SCTL		-1

#define NR_GIC_CA9X4		1

/*
 * Only define NR_IRQS if less than NR_IRQS_CA9X4
 */
#define NR_IRQS_CA9X4		(IRQ_CA9X4_GIC_START + 64)

#endif
