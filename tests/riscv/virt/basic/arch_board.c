/**
 * Copyright (c) 2019 Anup Patel.
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
 * @author Anup Patel (anup@brainfault.org)
 * @brief various platform specific functions
 */

#include <arch_types.h>
#include <arch_board.h>
#include <basic_stdio.h>
#include <basic_string.h>
#include <display/simplefb.h>
#include <pic/riscv_intc.h>
#include <serial/uart8250.h>
#include <sys/vminfo.h>
#include <timer/riscv_timer.h>

#define VIRT_NOR_FLASH			(0x00000000)
#define VIRT_NOR_FLASH_SIZE		(0x02000000)
#define VIRT_PLIC			(0x0c000000)
#define VIRT_PLIC_SIZE			(0x04000000)
#define VIRT_UART0			(0x10000000)
#define VIRT_VMINFO			(0x10001000)
#define VIRT_SIMPLEFB			(0x10002000)
#define VIRT_VIRTIO_NET			(0x20000000)
#define VIRT_VIRTIO_NET_SIZE		(0x00001000)
#define VIRT_VIRTIO_BLK			(0x20001000)
#define VIRT_VIRTIO_BLK_SIZE		(0x00001000)
#define VIRT_VIRTIO_CON			(0x20002000)
#define VIRT_VIRTIO_CON_SIZE		(0x00001000)
#define VIRT_PCI			(0x30000000)
#define VIRT_PCI_SIZE			(0x20000000)
#define VIRT_RAM0			(0x80000000)
#define VIRT_RAM0_SIZE			(0x06000000)

/*
 * Interrupts.
 */
#define IRQ_VIRT_UART0			10
#define IRQ_VIRT_VIRTIO_NET		1
#define IRQ_VIRT_VIRTIO_BLK		2
#define IRQ_VIRT_VIRTIO_CON		3

#define VIRT_PLIC_NUM_SOURCES		127
#define VIRT_PLIC_NUM_PRIORITIES	7

void arch_board_reset(void)
{
	/* Nothing to do */
}

void arch_board_init(void)
{
	/* Nothing to do */
}

char *arch_board_name(void)
{
	return "Virt";
}

physical_addr_t arch_board_ram_start(void)
{
	return vminfo_ram_base(VIRT_VMINFO, 0);
}

physical_size_t arch_board_ram_size(void)
{
	return vminfo_ram_size(VIRT_VMINFO, 0);
}

void arch_board_linux_default_cmdline(char *cmdline, u32 cmdline_sz)
{
	basic_strcpy(cmdline, "root=/dev/ram rw "
			      "earlycon=sbi "
			      "console=ttyS0,115200");
}

void arch_board_fdt_fixup(void *fdt_addr)
{
	simplefb_fdt_fixup(VIRT_SIMPLEFB, fdt_addr);
}

physical_addr_t arch_board_autoexec_addr(void)
{
	return VIRT_NOR_FLASH + 0xFF000;
}

u32 arch_board_boot_delay(void)
{
	return vminfo_boot_delay(VIRT_VMINFO);
}

u32 arch_board_iosection_count(void)
{
	return 8;
}

physical_addr_t arch_board_iosection_addr(int num)
{
	physical_addr_t ret = 0;

	switch (num) {
	case 0:
		/* nor-flash */
		ret = VIRT_NOR_FLASH;
		break;
	case 1:
		/* PLIC */
		ret = VIRT_PLIC;
		break;
	case 2:
		/* uart0 */
		ret = VIRT_UART0;
		break;
	case 3:
		/* vminfo */
		ret = VIRT_VMINFO;
		break;
	case 4:
		/* simplefb */
		ret = VIRT_SIMPLEFB;
		break;
	case 5:
		/* virtio-net */
		ret = VIRT_VIRTIO_NET;
		break;
	case 6:
		/* virtio-blk */
		ret = VIRT_VIRTIO_BLK;
		break;
	case 7:
		/* virtio-con */
		ret = VIRT_VIRTIO_CON;
		break;
	default:
		while (1);
		break;
	}

	return ret;
}

u32 arch_board_pic_nr_irqs(void)
{
	return riscv_intc_nr_irqs();
}

int arch_board_pic_init(void)
{
	return riscv_intc_init();
}

u32 arch_board_pic_active_irq(void)
{
	return riscv_intc_active_irq();
}

int arch_board_pic_ack_irq(u32 irq)
{
	return riscv_intc_ack_irq(irq);
}

int arch_board_pic_eoi_irq(u32 irq)
{
	return riscv_intc_eoi_irq(irq);
}

int arch_board_pic_mask(u32 irq)
{
	return riscv_intc_mask(irq);
}

int arch_board_pic_unmask(u32 irq)
{
	return riscv_intc_unmask(irq);
}

void arch_board_timer_enable(void)
{
	riscv_timer_enable();
}

void arch_board_timer_disable(void)
{
	riscv_timer_disable();
}

u64 arch_board_timer_irqcount(void)
{
	return riscv_timer_irqcount();
}

u64 arch_board_timer_irqdelay(void)
{
	return riscv_timer_irqdelay();
}

u64 arch_board_timer_timestamp(void)
{
	return riscv_timer_timestamp();
}

void arch_board_timer_change_period(u32 usecs)
{
	riscv_timer_change_period(usecs);
}

int arch_board_timer_init(u32 usecs)
{
	/*
	 * TODO: RISC-V timer frequency should be same
	 * as underlying host timer frequency. Instead
	 * of hard-coding figure-out a better way.
	 */
	return riscv_timer_init(usecs, 10000000);
}

int arch_board_serial_init(void)
{
	uart8250_init(VIRT_UART0, 0, 1, 115200, 500000000);

	return 0;
}

void arch_board_serial_putc(char ch)
{
	if (ch == '\n') {
		uart8250_putc(VIRT_UART0, 0, 1, '\r');
	}
	uart8250_putc(VIRT_UART0, 0, 1, ch);
}

bool arch_board_serial_can_getc(void)
{
	return uart8250_can_getc(VIRT_UART0, 0, 1);
}

char arch_board_serial_getc(void)
{
	char ch = uart8250_getc(VIRT_UART0, 0, 1);
	if (ch == '\r') {
		ch = '\n';
	}
	arch_board_serial_putc(ch);
	return ch;
}
