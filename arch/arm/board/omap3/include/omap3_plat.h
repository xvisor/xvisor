/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file omap3_plat.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief OMAP3 platform configuration
 */
#ifndef __OMAP3_PLAT_H__
#define __OMAP3_PLAT_H__

/* ===== SDRC & SMS ===== */

/** OMAP3/OMAP343X SDRC Base Physical Address */
#define OMAP3_SDRC_BASE				0x6D000000

/** OMAP3/OMAP343X SMS Base Physical Address */
#define OMAP3_SMS_BASE				0x6C000000

/* ===== PRM & CM ===== */

/** OMAP3/OMAP343X PRCM Base Physical Address */
#define OMAP3_PRCM_BASE			0x48004000

/** OMAP3/OMAP343X CM Base Physical Address */
#define OMAP3_CM_BASE			0x48004000
#define OMAP3_CM_SIZE			0x2000

/** OMAP3/OMAP343X PRM Base Physical Address */
#define OMAP3_PRM_BASE			0x48306000
#define OMAP3_PRM_SIZE			0x2000

/* ===== INTC ===== */

/** OMAP3/OMAP343X INTC Base Physical Address */
#define OMAP3_MPU_INTC_BASE			0x48200000

/** OMAP3/OMAP343X INTC IRQ Count */
#define OMAP3_MPU_INTC_NRIRQ			96

#define OMAP3_MPU_INTC_EMUINT			0
#define OMAP3_MPU_INTC_COMMTX			1
#define OMAP3_MPU_INTC_COMMRX			2
#define OMAP3_MPU_INTC_BENCH			3
#define OMAP3_MPU_INTC_MCBSP2_ST_IRQ		4
#define OMAP3_MPU_INTC_MCBSP3_ST_IRQ		5
#define OMAP3_MPU_INTC_RESERVED0		6
#define OMAP3_MPU_INTC_SYS_NIRQ			7
#define OMAP3_MPU_INTC_RESERVED1		8
#define OMAP3_MPU_INTC_SMX_DBG_IRQ		9
#define OMAP3_MPU_INTC_SMX_APP_IRQ		10
#define OMAP3_MPU_INTC_PRCM_MPU_IRQ		11
#define OMAP3_MPU_INTC_SDMA_IRQ_0		12
#define OMAP3_MPU_INTC_SDMA_IRQ_1		13
#define OMAP3_MPU_INTC_SDMA_IRQ_2		14
#define OMAP3_MPU_INTC_SDMA_IRQ_3		15
#define OMAP3_MPU_INTC_MCBSP1_IRQ		16
#define OMAP3_MPU_INTC_MCBSP2_IRQ		17
#define OMAP3_MPU_INTC_RESERVED2		18
#define OMAP3_MPU_INTC_RESERVED3		19
#define OMAP3_MPU_INTC_GPMC_IRQ			20
#define OMAP3_MPU_INTC_SGX_IRQ			21
#define OMAP3_MPU_INTC_MCBSP3_IRQ		22
#define OMAP3_MPU_INTC_MCBSP4_IRQ		23
#define OMAP3_MPU_INTC_CAM_IRQ0			24
#define OMAP3_MPU_INTC_DSS_IRQ			25
#define OMAP3_MPU_INTC_MAIL_U0_MPU_IRQ		26
#define OMAP3_MPU_INTC_MCBSP5_IRQ		27
#define OMAP3_MPU_INTC_IVA2_MMU_IRQ		28
#define OMAP3_MPU_INTC_GPIO1_MPU_IRQ		29
#define OMAP3_MPU_INTC_GPIO2_MPU_IRQ		30
#define OMAP3_MPU_INTC_GPIO3_MPU_IRQ		31
#define OMAP3_MPU_INTC_GPIO4_MPU_IRQ		32
#define OMAP3_MPU_INTC_GPIO5_MPU_IRQ		33
#define OMAP3_MPU_INTC_GPIO6_MPU_IRQ		34
#define OMAP3_MPU_INTC_RESERVED4		35
#define OMAP3_MPU_INTC_WDT3_IRQ			36
#define OMAP3_MPU_INTC_GPT1_IRQ			37
#define OMAP3_MPU_INTC_GPT2_IRQ			38
#define OMAP3_MPU_INTC_GPT3_IRQ			39
#define OMAP3_MPU_INTC_GPT4_IRQ			40
#define OMAP3_MPU_INTC_GPT5_IRQ			41
#define OMAP3_MPU_INTC_GPT6_IRQ			42
#define OMAP3_MPU_INTC_GPT7_IRQ			43
#define OMAP3_MPU_INTC_GPT8_IRQ			44
#define OMAP3_MPU_INTC_GPT9_IRQ			45
#define OMAP3_MPU_INTC_GPT10_IRQ		46
#define OMAP3_MPU_INTC_GPT11_IRQ		47
#define OMAP3_MPU_INTC_SPI4_IRQ			48
#define OMAP3_MPU_INTC_RESERVED5		49
#define OMAP3_MPU_INTC_RESERVED6		50
#define OMAP3_MPU_INTC_RESERVED7		51
#define OMAP3_MPU_INTC_RESERVED8		52
#define OMAP3_MPU_INTC_RESERVED9		53
#define OMAP3_MPU_INTC_MCBSP4_IRQ_TX		54
#define OMAP3_MPU_INTC_MCBSP4_IRQ_RX		55
#define OMAP3_MPU_INTC_I2C1_IRQ			56
#define OMAP3_MPU_INTC_I2C2_IRQ			57
#define OMAP3_MPU_INTC_HDQ_IRQ			58
#define OMAP3_MPU_INTC_McBSP1_IRQ_TX		59
#define OMAP3_MPU_INTC_McBSP1_IRQ_RX		60
#define OMAP3_MPU_INTC_I2C3_IRQ			61
#define OMAP3_MPU_INTC_McBSP2_IRQ_TX		62
#define OMAP3_MPU_INTC_McBSP2_IRQ_RX		63
#define OMAP3_MPU_INTC_RESERVED10		64
#define OMAP3_MPU_INTC_SPI1_IRQ			65
#define OMAP3_MPU_INTC_SPI2_IRQ			66
#define OMAP3_MPU_INTC_RESERVED11		67
#define OMAP3_MPU_INTC_RESERVED12		68
#define OMAP3_MPU_INTC_RESERVED13		69
#define OMAP3_MPU_INTC_RESERVED14		70
#define OMAP3_MPU_INTC_RESERVED15		71
#define OMAP3_MPU_INTC_UART1_IRQ		72
#define OMAP3_MPU_INTC_UART2_IRQ		73
#define OMAP3_MPU_INTC_UART3_IRQ		74
#define OMAP3_MPU_INTC_PBIAS_IRQ		75
#define OMAP3_MPU_INTC_OHCI_IRQ			76
#define OMAP3_MPU_INTC_EHCI_IRQ			77
#define OMAP3_MPU_INTC_TLL_IRQ			78
#define OMAP3_MPU_INTC_RESERVED16		79
#define OMAP3_MPU_INTC_RESERVED17		80
#define OMAP3_MPU_INTC_MCBSP5_IRQ_TX		81
#define OMAP3_MPU_INTC_MCBSP5_IRQ_RX		82
#define OMAP3_MPU_INTC_MMC1_IRQ			83
#define OMAP3_MPU_INTC_RESERVED18		84
#define OMAP3_MPU_INTC_RESERVED19		85
#define OMAP3_MPU_INTC_MMC2_IRQ			86
#define OMAP3_MPU_INTC_MPU_ICR_IRQ		87
#define OMAP3_MPU_INTC_D2DFRINT			88
#define OMAP3_MPU_INTC_MCBSP3_IRQ_TX		89
#define OMAP3_MPU_INTC_MCBSP3_IRQ_RX		90
#define OMAP3_MPU_INTC_SPI3_IRQ			91
#define OMAP3_MPU_INTC_HSUSB_MC_NINT		92
#define OMAP3_MPU_INTC_HSUSB_DMA_NINT		93
#define OMAP3_MPU_INTC_MMC3_IRQ			94
#define OMAP3_MPU_INTC_RESERVED20		95

/* ===== Synchronous 32k Timer ===== */

/** OMAP3/OMAP343X S32K Base Physical Address */
#define OMAP3_S32K_BASE 			0x48320000

/* ===== General Purpose Timers ===== */

/** OMAP3/OMAP343X GPT Base Physical Addresses */
#define OMAP3_GPT1_BASE 			0x48318000
#define OMAP3_GPT2_BASE 			0x49032000
#define OMAP3_GPT3_BASE 			0x49034000
#define OMAP3_GPT4_BASE 			0x49036000
#define OMAP3_GPT5_BASE 			0x49038000
#define OMAP3_GPT6_BASE 			0x4903A000
#define OMAP3_GPT7_BASE 			0x4903C000
#define OMAP3_GPT8_BASE 			0x4903E000
#define OMAP3_GPT9_BASE 			0x49040000
#define OMAP3_GPT10_BASE 			0x48086000
#define OMAP3_GPT11_BASE 			0x48088000
#define OMAP3_GPT12_BASE 			0x48304000

/* ===== UART (or Serial Port) ===== */

#define OMAP3_COM_FREQ   	48000000L

/** OMAP3/OMAP343X UART Base Physical Address */
#define OMAP3_UART_BASE 	0x49020000
#define OMAP3_UART_BAUD 	115200
#define OMAP3_UART_INCLK 	OMAP3_COM_FREQ

#endif
