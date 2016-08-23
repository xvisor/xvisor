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
#include <arm_math.h>
#include <arm_string.h>
#include <arm_board.h>
#include <arm_plat.h>
#include <pic/pl190.h>
#include <timer/sp804.h>
#include <serial/pl01x.h>
#include <sys/vminfo.h>

void arm_board_reset(void)
{
	arm_writel(0x101,
		   (void *)(VERSATILE_SYS_BASE + VERSATILE_SYS_RESETCTL_OFFSET));
}

void arm_board_init(void)
{
	/* Unlock Lockable reigsters */
	arm_writel(VERSATILE_SYS_LOCKVAL,
		   (void *)(VERSATILE_SYS_BASE + VERSATILE_SYS_LOCK_OFFSET));
}

char *arm_board_name(void)
{
	return "ARM VersatilePB";
}

u32 arm_board_ram_start(void)
{
	return (u32)vminfo_ram_base(0x14000000, 0);
}

u32 arm_board_ram_size(void)
{
	return (u32)vminfo_ram_size(0x14000000, 0);
}

u32 arm_board_linux_machine_type(void)
{
	return 0x183;
}

void arm_board_linux_default_cmdline(char *cmdline, u32 cmdline_sz)
{
	arm_strcpy(cmdline, "root=/dev/ram rw earlyprintk console=ttyAMA0");
}

u32 arm_board_flash_addr(void)
{
	return (u32)(VERSATILE_FLASH_BASE);
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
		ret = VERSATILE_SYS_BASE;
		break;
	case 1:
		ret = VERSATILE_VIC_BASE;
		break;
	case 2:
	case 3:
	case 4:
	case 5:
		ret = VERSATILE_FLASH_BASE + (num - 2) * 0x100000;
		break;
	default:
		while (1);
		break;
	}

	return ret;
}

#define NR_IRQS_VERSATILE	64

u32 arm_board_pic_nr_irqs(void)
{
	return NR_IRQS_VERSATILE;
}

int arm_board_pic_init(void)
{
	int rc;

	/*
	 * Initialize Vectored Interrupt Controller
	 */
	rc = pl190_cpu_init(0, VERSATILE_VIC_BASE);
	if (rc) {
		return rc;
	}

	return 0;
}

u32 arm_board_pic_active_irq(void)
{
	return pl190_active_irq(0);
}

int arm_board_pic_ack_irq(u32 irq)
{
	return 0;
}

int arm_board_pic_eoi_irq(u32 irq)
{
	return pl190_eoi_irq(0, irq);
}

int arm_board_pic_mask(u32 irq)
{
	return pl190_mask(0, irq);
}

int arm_board_pic_unmask(u32 irq)
{
	return pl190_unmask(0, irq);
}

void arm_board_timer_enable(void)
{
	return sp804_enable();
}

void arm_board_timer_disable(void)
{
	return sp804_disable();
}

u64 arm_board_timer_irqcount(void)
{
	return sp804_irqcount();
}

u64 arm_board_timer_irqdelay(void)
{
	return sp804_irqdelay();
}

u64 arm_board_timer_timestamp(void)
{
	return sp804_timestamp();
}

void arm_board_timer_change_period(u32 usecs)
{
	return sp804_change_period(usecs);
}

int arm_board_timer_init(u32 usecs)
{
	u32 val, irq;
	u64 counter_mult, counter_shift, counter_mask;

	counter_mask = 0xFFFFFFFFULL;
	counter_shift = 20;
	counter_mult = ((u64)1000000) << counter_shift;
	counter_mult += (((u64)1000) >> 1);
	counter_mult = arm_udiv64(counter_mult, ((u64)1000));

	irq = INT_TIMERINT0_1;

	/* set clock frequency: 
	 *      VERSATILE_REFCLK is 32KHz
	 *      VERSATILE_TIMCLK is 1MHz
	 */
	val = arm_readl((void *)VERSATILE_SCTL_BASE) | (VERSATILE_TIMCLK << 1);
	arm_writel(val, (void *)VERSATILE_SCTL_BASE);

	return sp804_init(usecs, VERSATILE_TIMER0_1_BASE, irq, 
			  counter_mask, counter_mult, counter_shift);
}

#define	VERSATILE_UART_BASE			0x101F1000
#define	VERSATILE_UART_TYPE			PL01X_TYPE_1
#define	VERSATILE_UART_INCLK			24000000
#define	VERSATILE_UART_BAUD			115200

int arm_board_serial_init(void)
{
	pl01x_init(VERSATILE_UART_BASE, 
			VERSATILE_UART_TYPE, 
			VERSATILE_UART_BAUD, 
			VERSATILE_UART_INCLK);

	return 0;
}

void arm_board_serial_putc(char ch)
{
	if (ch == '\n') {
		pl01x_putc(VERSATILE_UART_BASE, VERSATILE_UART_TYPE, '\r');
	}
	pl01x_putc(VERSATILE_UART_BASE, VERSATILE_UART_TYPE, ch);
}

char arm_board_serial_getc(void)
{
	char ch = pl01x_getc(VERSATILE_UART_BASE, VERSATILE_UART_TYPE);
	if (ch == '\r') {
		ch = '\n';
	}
	arm_board_serial_putc(ch);
	return ch;
}

