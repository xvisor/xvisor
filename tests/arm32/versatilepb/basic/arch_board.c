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
 * @file arch_board.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief various platform specific functions
 */

#include <arch_types.h>
#include <arch_io.h>
#include <arch_math.h>
#include <arch_board.h>
#include <arm_plat.h>
#include <basic_string.h>
#include <pic/pl190.h>
#include <timer/sp804.h>
#include <serial/pl01x.h>
#include <sys/vminfo.h>

void arch_board_reset(void)
{
	arch_writel(0x101,
		   (void *)(VERSATILE_SYS_BASE + VERSATILE_SYS_RESETCTL_OFFSET));
}

void arch_board_init(void)
{
	/* Unlock Lockable reigsters */
	arch_writel(VERSATILE_SYS_LOCKVAL,
		   (void *)(VERSATILE_SYS_BASE + VERSATILE_SYS_LOCK_OFFSET));
}

char *arch_board_name(void)
{
	return "ARM VersatilePB";
}

physical_addr_t arch_board_ram_start(void)
{
	return (physical_addr_t)vminfo_ram_base(VERSATILE_VMINFO_BASE, 0);
}

physical_size_t arch_board_ram_size(void)
{
	return (physical_size_t)vminfo_ram_size(VERSATILE_VMINFO_BASE, 0);
}

void arch_board_linux_default_cmdline(char *cmdline, u32 cmdline_sz)
{
	basic_strcpy(cmdline, "root=/dev/ram rw earlyprintk "
			      "earlycon=pl011,0x101f1000 console=ttyAMA0");
}

void arch_board_fdt_fixup(void *fdt_addr)
{
	/* For now nothing to do here. */
}

physical_addr_t arch_board_autoexec_addr(void)
{
	return (VERSATILE_FLASH_BASE + 0xFF000);
}

u32 arch_board_boot_delay(void)
{
	return vminfo_boot_delay(VERSATILE_VMINFO_BASE);
}

u32 arch_board_iosection_count(void)
{
	return 19;
}

physical_addr_t arch_board_iosection_addr(int num)
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
		ret = VERSATILE_VMINFO_BASE;
		break;
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
	case 15:
	case 16:
	case 17:
	case 18:
		ret = VERSATILE_FLASH_BASE + (num - 3) * 0x100000;
		break;
	default:
		while (1);
		break;
	}

	return ret;
}

#define NR_IRQS_VERSATILE	64

u32 arch_board_pic_nr_irqs(void)
{
	return NR_IRQS_VERSATILE;
}

int arch_board_pic_init(void)
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

u32 arch_board_pic_active_irq(void)
{
	return pl190_active_irq(0);
}

int arch_board_pic_ack_irq(u32 irq)
{
	return 0;
}

int arch_board_pic_eoi_irq(u32 irq)
{
	return pl190_eoi_irq(0, irq);
}

int arch_board_pic_mask(u32 irq)
{
	return pl190_mask(0, irq);
}

int arch_board_pic_unmask(u32 irq)
{
	return pl190_unmask(0, irq);
}

void arch_board_timer_enable(void)
{
	return sp804_enable();
}

void arch_board_timer_disable(void)
{
	return sp804_disable();
}

u64 arch_board_timer_irqcount(void)
{
	return sp804_irqcount();
}

u64 arch_board_timer_irqdelay(void)
{
	return sp804_irqdelay();
}

u64 arch_board_timer_timestamp(void)
{
	return sp804_timestamp();
}

void arch_board_timer_change_period(u32 usecs)
{
	return sp804_change_period(usecs);
}

int arch_board_timer_init(u32 usecs)
{
	u32 val, irq;
	u64 counter_mult, counter_shift, counter_mask;

	counter_mask = 0xFFFFFFFFULL;
	counter_shift = 20;
	counter_mult = ((u64)1000000) << counter_shift;
	counter_mult += (((u64)1000) >> 1);
	counter_mult = arch_udiv64(counter_mult, ((u64)1000));

	irq = INT_TIMERINT0_1;

	/* set clock frequency:
	 *      VERSATILE_REFCLK is 32KHz
	 *      VERSATILE_TIMCLK is 1MHz
	 */
	val = arch_readl((void *)VERSATILE_SCTL_BASE) | (VERSATILE_TIMCLK << 1);
	arch_writel(val, (void *)VERSATILE_SCTL_BASE);

	return sp804_init(usecs, VERSATILE_TIMER0_1_BASE, irq,
			  counter_mask, counter_mult, counter_shift);
}

#define	VERSATILE_UART_BASE			0x101F1000
#define	VERSATILE_UART_TYPE			PL01X_TYPE_1
#define	VERSATILE_UART_INCLK			24000000
#define	VERSATILE_UART_BAUD			115200

int arch_board_serial_init(void)
{
	pl01x_init(VERSATILE_UART_BASE,
			VERSATILE_UART_TYPE,
			VERSATILE_UART_BAUD,
			VERSATILE_UART_INCLK);

	return 0;
}

void arch_board_serial_putc(char ch)
{
	if (ch == '\n') {
		pl01x_putc(VERSATILE_UART_BASE, VERSATILE_UART_TYPE, '\r');
	}
	pl01x_putc(VERSATILE_UART_BASE, VERSATILE_UART_TYPE, ch);
}

bool arch_board_serial_can_getc(void)
{
	return pl01x_can_getc(VERSATILE_UART_BASE, VERSATILE_UART_TYPE);
}

char arch_board_serial_getc(void)
{
	char ch = pl01x_getc(VERSATILE_UART_BASE, VERSATILE_UART_TYPE);
	if (ch == '\r') {
		ch = '\n';
	}
	arch_board_serial_putc(ch);
	return ch;
}
