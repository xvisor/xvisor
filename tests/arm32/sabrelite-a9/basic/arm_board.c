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
#include <pic/gic.h>
#include <timer/imx_gpt.h>
#include <serial/imx.h>
#include <sys/vminfo.h>

void arm_board_reset(void)
{
	/* Nothing to do */
}

void arm_board_init(void)
{
	/* Nothing to do */
}

char *arm_board_name(void)
{
	return "ARM SabreLite";
}

u32 arm_board_ram_start(void)
{
	return (u32)vminfo_ram_base(IMX_VMINFO_BASE, 0);
}

u32 arm_board_ram_size(void)
{
	return (u32)vminfo_ram_size(IMX_VMINFO_BASE, 0);
}

u32 arm_board_linux_machine_type(void)
{
	return 0x8e0;
}

void arm_board_linux_default_cmdline(char *cmdline, u32 cmdline_sz)
{
	arm_strcpy(cmdline, "root=/dev/ram rw earlyprintk");
	/* VirtIO Network Device */
	arm_strcat(cmdline, " virtio_mmio.device=64K@0x20100000:42");
	/* SabreLite/Nitrogen6X specific */
	arm_strcat(cmdline, " enable_wait_mode=off "
		   "video=mxcfb0:dev=ldb,LDB-XGA,if=RGB666 video=mxcfb1:off "
		   "video=mxcfb2:off video=mxcfb3:off fbmem=10M "
		   "console=ttymxc1,115200 vmalloc=400M consoleblank=0 "
		   "mxc_hdmi.only_cea=1");
}

void arm_board_fdt_fixup(void *fdt_addr)
{
	/* For now nothing to do here. */
}

u32 arm_board_flash_addr(void)
{
	return (u32)(IMX_NOR);
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
		ret = IMX_IOMUX;
		break;
	case 1:
		ret = CT_CA9X4_MPIC;
		break;
	case 2:
		ret = IMX_NOR;
		break;
	case 3:
		ret = IMX_UART1;
		break;
	case 4:
		ret = IMX_TIMER0;
		break;
	case 5:
		ret = IMX_VMINFO_BASE;
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

void arm_board_timer_enable(void)
{
	return imx_gpt_enable();
}

void arm_board_timer_disable(void)
{
	return imx_gpt_disable();
}

u64 arm_board_timer_irqcount(void)
{
	return imx_gpt_irqcount();
}

u64 arm_board_timer_irqdelay(void)
{
	return imx_gpt_irqdelay();
}

u64 arm_board_timer_timestamp(void)
{
	return imx_gpt_timestamp();
}

void arm_board_timer_change_period(u32 usecs)
{
	return imx_gpt_change_period(usecs);
}

int arm_board_timer_init(u32 usecs)
{
	arm_board_pic_unmask(IRQ_IMX_TIMER0);

	return imx_gpt_init(usecs, IMX_TIMER0, IRQ_IMX_TIMER0, 0);
}

#define	IMX_UART_BASE		IMX_UART1
#define	IMX_UART_INCLK		80000000
#define	IMX_UART_BAUD		115200

int arm_board_serial_init(void)
{
	imx_init(IMX_UART_BASE, IMX_UART_BAUD, IMX_UART_INCLK);

	return 0;
}

void arm_board_serial_putc(char ch)
{
	if (ch == '\n') {
		imx_putc(IMX_UART_BASE, '\r');
	}

	imx_putc(IMX_UART_BASE, ch);
}

char arm_board_serial_getc(void)
{
	char ch = imx_getc(IMX_UART_BASE);
	if (ch == '\r') {
		ch = '\n';
	}
	arm_board_serial_putc(ch);
	return ch;
}
