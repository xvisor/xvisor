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
 * @file motherboard.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief VExpress V2M platform configuration
 */
#ifndef __MOTHERBOARD_H__
#define __MOTHERBOARD_H__

/*
 * Offsets from SYSREGS base
 */
#define V2M_SYS_ID		0x000
#define V2M_SYS_SW		0x004
#define V2M_SYS_LED		0x008
#define V2M_SYS_100HZ		0x024
#define V2M_SYS_FLAGS		0x030
#define V2M_SYS_FLAGSSET	0x030
#define V2M_SYS_FLAGSCLR	0x034
#define V2M_SYS_NVFLAGS		0x038
#define V2M_SYS_NVFLAGSSET	0x038
#define V2M_SYS_NVFLAGSCLR	0x03c
#define V2M_SYS_MCI		0x048
#define V2M_SYS_FLASH		0x03c
#define V2M_SYS_CFGSW		0x058
#define V2M_SYS_24MHZ		0x05c
#define V2M_SYS_MISC		0x060
#define V2M_SYS_DMA		0x064
#define V2M_SYS_PROCID0		0x084
#define V2M_SYS_PROCID1		0x088
#define V2M_SYS_CFGDATA		0x0a0
#define V2M_SYS_CFGCTRL		0x0a4
#define V2M_SYS_CFGSTAT		0x0a8


/*
 * Interrupts.  Those in {} are for AMBA devices
 */
#define IRQ_V2M_WDT		{ (32 + 0) }
#define IRQ_V2M_TIMER0		(32 + 2)
#define IRQ_V2M_TIMER1		(32 + 2)
#define IRQ_V2M_TIMER2		(32 + 3)
#define IRQ_V2M_TIMER3		(32 + 3)
#define IRQ_V2M_RTC		{ (32 + 4) }
#define IRQ_V2M_UART0		{ (32 + 5) }
#define IRQ_V2M_UART1		{ (32 + 6) }
#define IRQ_V2M_UART2		{ (32 + 7) }
#define IRQ_V2M_UART3		{ (32 + 8) }
#define IRQ_V2M_MMCI		{ (32 + 9), (32 + 10) }
#define IRQ_V2M_AACI		{ (32 + 11) }
#define IRQ_V2M_KMI0		{ (32 + 12) }
#define IRQ_V2M_KMI1		{ (32 + 13) }
#define IRQ_V2M_CLCD		{ (32 + 14) }
#define IRQ_V2M_LAN9118		(32 + 15)
#define IRQ_V2M_ISP1761		(32 + 16)
#define IRQ_V2M_PCIE		(32 + 17)

/*
 * Configuration
 */
#define SYS_CFG_START		(1 << 31)
#define SYS_CFG_WRITE		(1 << 30)
#define SYS_CFG_OSC		(1 << 20)
#define SYS_CFG_VOLT		(2 << 20)
#define SYS_CFG_AMP		(3 << 20)
#define SYS_CFG_TEMP		(4 << 20)
#define SYS_CFG_RESET		(5 << 20)
#define SYS_CFG_SCC		(6 << 20)
#define SYS_CFG_MUXFPGA		(7 << 20)
#define SYS_CFG_SHUTDOWN	(8 << 20)
#define SYS_CFG_REBOOT		(9 << 20)
#define SYS_CFG_DVIMODE		(11 << 20)
#define SYS_CFG_POWER		(12 << 20)
#define SYS_CFG_SITE_MB		(0 << 16)
#define SYS_CFG_SITE_DB1	(1 << 16)
#define SYS_CFG_SITE_DB2	(2 << 16)
#define SYS_CFG_STACK(n)	((n) << 12)

#define SYS_CFG_ERR		(1 << 1)
#define SYS_CFG_COMPLETE	(1 << 0)

int v2m_cfg_write(u32 devfn, u32 data);
int v2m_cfg_read(u32 devfn, u32 *data);
void v2m_flags_set(u32 data);

/*
 * Miscellaneous
 */
#define SYS_MISC_MASTERSITE	(1 << 14)
#define SYS_PROCIDx_HBI_MASK	0xfff

/*
 * Core tile IDs
 */
#define V2M_CT_ID_CA9		0x0c000191
#define V2M_CT_ID_UNSUPPORTED	0xff000191
#define V2M_CT_ID_MASK		0xff000fff

#endif
