/**
 * Copyright (c) 2012 Jean-Christophe Dubois.
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
 * @file arm_board.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief various platform specific functions
 */

#include <arm_types.h>
#include <arm_io.h>
#include <arm_board.h>
#include <arm_plat.h>
#include <arm_config.h>
#include <pic/gic.h>
#include <serial/pl01x.h>

void arm_board_reset(void)
{
	arm_writel(~0x0, (void *)(V2M_SYS_FLAGSCLR));
	arm_writel(0x0, (void *)(V2M_SYS_FLAGSSET));
	arm_writel(0xc0900000, (void *)(V2M_SYS_CFGCTRL));
}

void arm_board_init(void)
{
	/* Nothing to do */
}

char *arm_board_name(void)
{
	return "ARM VExpress-A9";
}

u32 arm_board_ram_start(void)
{
	return 0x60000000;
}

u32 arm_board_ram_size(void)
{
	return 0x6000000;
}

u32 arm_board_linux_machine_type(void)
{
	return 0x8e0;
}

u32 arm_board_flash_addr(void)
{
	return (u32)(V2M_NOR0);
}

u32 arm_board_iosection_count(void)
{
	return 6;
}

physical_addr_t arm_board_iosection_addr(int num)
{
	physical_addr_t ret = 0;

	switch (num) {
	case 0:
		ret = V2M_PA_CS7;
		break;
	case 1:
		ret = CT_CA9X4_MPIC;
		break;
	case 2:
	case 3:
	case 4:
	case 5:
		ret = V2M_NOR0 + (num - 2) * 0x100000;
		break;
	default:
		while (1);
		break;
	}

	return ret;
}

u32 arm_board_pic_nr_irqs(void)
{
	return NR_IRQS_CA9X4;
}

int arm_board_pic_init(void)
{
	int rc;

	/*
	 * Initialize Generic Interrupt Controller
	 */
	rc = gic_dist_init(0, A9_MPCORE_GIC_DIST, IRQ_CA9X4_GIC_START);
	if (rc) {
		return rc;
	}
	rc = gic_cpu_init(0, A9_MPCORE_GIC_CPU);
	if (rc) {
		return rc;
	}

	return 0;
}

u32 arm_board_pic_active_irq(void)
{
	return gic_active_irq(0);
}

int arm_board_pic_ack_irq(u32 irq)
{
	return 0;
}

int arm_board_pic_eoi_irq(u32 irq)
{
	return gic_eoi_irq(0, irq);
}

int arm_board_pic_mask(u32 irq)
{
	return gic_mask(0, irq);
}

int arm_board_pic_unmask(u32 irq)
{
	return gic_unmask(0, irq);
}

#define	CA9X4_UART_BASE			V2M_UART0
#define	CA9X4_UART_TYPE			PL01X_TYPE_1
#define	CA9X4_UART_INCLK		24000000
#define	CA9X4_UART_BAUD			115200

int arm_board_serial_init(void)
{
	pl01x_init(CA9X4_UART_BASE, 
			CA9X4_UART_TYPE, 
			CA9X4_UART_BAUD, 
			CA9X4_UART_INCLK);

	return 0;
}

void arm_board_serial_putc(char ch)
{
	if (ch == '\n') {
		pl01x_putc(CA9X4_UART_BASE, CA9X4_UART_TYPE, '\r');
	}
	pl01x_putc(CA9X4_UART_BASE, CA9X4_UART_TYPE, ch);
}

char arm_board_serial_getc(void)
{
	char ch = pl01x_getc(CA9X4_UART_BASE, CA9X4_UART_TYPE);
	if (ch == '\r') {
		ch = '\n';
	}
	arm_board_serial_putc(ch);
	return ch;
}


