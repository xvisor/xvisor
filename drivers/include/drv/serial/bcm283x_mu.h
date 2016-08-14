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
 * @file bcm283x_mu.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file for BCM283x Miniuart serial driver.
 */

#ifndef __BCM283X_MU_H__
#define __BCM283X_MU_H__

#include <vmm_types.h>

#define BCM283X_MU_IO			0x00
#define BCM283X_MU_IER			0x04
#define BCM283X_MU_IIR			0x08
#define BCM283X_MU_LCR			0x0c
#define BCM283X_MU_MCR			0x10
#define BCM283X_MU_LSR			0x14
#define BCM283X_MU_MSR			0x18
#define BCM283X_MU_SCRATCH		0x1c
#define BCM283X_MU_CNTL			0x20
#define BCM283X_MU_STAT			0x24
#define BCM283X_MU_BAUD			0x28

#define BCM283X_MU_IER_TX_INTERRUPT	(1 << 1)
#define BCM283X_MU_IER_RX_INTERRUPT	(1 << 0)

#define BCM283X_MU_IIR_RX_INTERRUPT	(1 << 2)
#define BCM283X_MU_IIR_TX_INTERRUPT	(1 << 1)
#define BCM283X_MU_IIR_FLUSH		0xc6

#define BCM283X_MU_LCR_7BIT		2
#define BCM283X_MU_LCR_8BIT		3

#define BCM283X_MU_LSR_TX_IDLE		(1 << 6)
#define BCM283X_MU_LSR_TX_EMPTY		(1 << 5)
#define BCM283X_MU_LSR_RX_OVERRUN	(1 << 1)
#define BCM283X_MU_LSR_RX_READY		(1 << 0)

#define BCM283X_MU_CNTL_RX_ENABLE	(1 << 0)
#define BCM283X_MU_CNTL_TX_ENABLE	(1 << 1)

bool bcm283x_mu_lowlevel_can_getc(virtual_addr_t base);
u8 bcm283x_mu_lowlevel_getc(virtual_addr_t base);
bool bcm283x_mu_lowlevel_can_putc(virtual_addr_t base);
void bcm283x_mu_lowlevel_putc(virtual_addr_t base, u8 ch);
void bcm283x_mu_lowlevel_init(virtual_addr_t base,
			      u32 baudrate, u32 input_clock);

#endif /* __BCM283X_MU__ */
