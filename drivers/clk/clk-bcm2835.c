/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file clk-bcm2835.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief BCM2835 clock implementation
 *
 * Adapted from linux/drivers/clk/clk-bcm2835.c
 *
 * Copyright (C) 2010 Broadcom
 * Copyright (C) 2012 Stephen Warren
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <vmm_error.h>
#include <vmm_compiler.h>
#include <vmm_stdio.h>
#include <drv/clk-provider.h>
#include <drv/clkdev.h>
#include <drv/clk/bcm2835.h>

/*
 * These are fixed clocks. They're probably not all root clocks and it may
 * be possible to turn them on and off but until this is mapped out better
 * it's the only way they can be used.
 */
void __init bcm2835_init_clocks(void)
{
	struct clk *clk;
	int ret;

	clk = clk_register_fixed_rate(NULL, "sys_pclk", NULL, CLK_IS_ROOT,
					250000000);
	if (!clk)
		vmm_printf("sys_pclk not registered\n");

	clk = clk_register_fixed_rate(NULL, "apb_pclk", NULL, CLK_IS_ROOT,
					126000000);
	if (!clk)
		vmm_printf("apb_pclk not registered\n");

	clk = clk_register_fixed_rate(NULL, "uart0_pclk", NULL, CLK_IS_ROOT,
					3000000);
	if (!clk)
		vmm_printf("uart0_pclk not registered\n");
	ret = clk_register_clkdev(clk, NULL, "20201000.uart");
	if (ret)
		vmm_printf("uart0_pclk alias not registered\n");

	clk = clk_register_fixed_rate(NULL, "uart1_pclk", NULL, CLK_IS_ROOT,
					125000000);
	if (!clk)
		vmm_printf("uart1_pclk not registered\n");
	ret = clk_register_clkdev(clk, NULL, "20215000.uart");
	if (ret)
		vmm_printf("uart1_pclk alias not registered\n");
}
