/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file defterm_early.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief arch default terminal early functions
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_compiler.h>
#include <vmm_host_io.h>
#include <arch_defterm.h>

u8 __aligned(0x1000) defterm_early_base[0x1000];
void *early_base = &defterm_early_base;

#if defined(CONFIG_RISCV_DEFTERM_EARLY_SBI)

#include <cpu_sbi.h>

/*
 * SBI based single character TX.
 */
void __init arch_defterm_early_putc(u8 ch)
{
	sbi_console_putchar((int)ch);
}

#elif defined(CONFIG_RISCV_DEFTERM_EARLY_UART8250_8BIT)

#include <drv/serial/8250-uart.h>

/*
 * 8250/16550 (8-bit 1-byte aligned registers) single character TX.
 */
void __init arch_defterm_early_putc(u8 ch)
{
	while (!(vmm_readb(early_base + UART_LSR_OFFSET) & UART_LSR_THRE))
		;
	vmm_writeb(ch, early_base + UART_THR_OFFSET);
}

#elif defined(CONFIG_RISCV_DEFTERM_EARLY_UART8250_8BIT_4ALIGN)

#include <drv/serial/8250-uart.h>

/*
 * 8250/16550 (8-bit 4-byte aligned registers) single character TX.
 */
void __init arch_defterm_early_putc(u8 ch)
{
	while (!(vmm_readb(early_base + (UART_LSR_OFFSET << 2)) & UART_LSR_THRE))
		;
	vmm_writeb(ch, early_base + (UART_THR_OFFSET << 2));
}

#elif defined(CONFIG_RISCV_DEFTERM_EARLY_UART8250_32BIT)

#include <drv/serial/8250-uart.h>

/*
 * 8250/16550 (32-bit 4-byte aligned registers) single character TX.
 */
void __init arch_defterm_early_putc(u8 ch)
{
	while (!(vmm_readl(early_base + (UART_LSR_OFFSET << 2)) & UART_LSR_THRE))
		;
	vmm_writel(ch, early_base + (UART_THR_OFFSET << 2));
}

#else

void __init arch_defterm_early_putc(u8 ch)
{
	(void)early_base;
}

#endif
