/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation
 * 51 Franklin Street, Fifth Floor
 * Boston, MA  02110-1301, USA.
 *
 * @file cmd_imx6.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief i.MX specific commands
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_cmdmgr.h>
#include <imx-common.h>
#include <linux/clk-provider.h>
#include <linux/clk-private.h>
#include <drv/imx-uart.h>
#include <libs/mathlib.h>

#include <imx-common.h>
#include <imx6qdl-clock.h>

#define do_readw(X)			readw((virtual_addr_t *)(X))
#define do_readl(X)			readl((virtual_addr_t *)(X))

static void cmd_imx6_usage(struct vmm_chardev *cdev);

/* This have to be independent from the device tree, to get information even
   from undeclared devices */
static unsigned int imx6q_uart_base[] = {
	0,
	0x02020000,
	0x021e8000,
	0x021ec000,
	0x021f0000,
	0x021f4000,
};

static const char* bit_status(unsigned int reg, unsigned int bit)
{
	if (do_readl(reg) & bit)
		return "enabled";
	else
		return "disabled";
}

static int cmd_uart_info(struct vmm_chardev *cdev, unsigned int port)
{
	virtual_addr_t uart_addr = 0;
	unsigned int div = 0;
	unsigned int freq = 0;
	unsigned short ubmr = 0;
	unsigned short ubir = 0;
	unsigned int baudrate = 0;
	const char *status;
	struct clk *clk = NULL;

	if ((port < 1) || (port > 5)) {
		vmm_printf("No UART port #%d\n", port);
		return VMM_ENODEV;
	}

	clk = imx_clk_get(IMX6QDL_CLK_UART_SERIAL);
	freq = __clk_get_rate(clk);
	vmm_printf("%s is set to %d MHz\n", __clk_get_name(clk),
		   udiv32(freq, 1000000));

	/* This function cannot fail, it only calls BUG() */
	uart_addr = vmm_host_iomap(imx6q_uart_base[port], 0x4000);

	vmm_cprintf(cdev, "UART %d\n", port);
	status = bit_status(uart_addr + UCR1, UCR1_UARTEN);
	vmm_cprintf(cdev, "  %s", status);
	status = bit_status(uart_addr + UCR2, UCR2_RXEN);
	vmm_cprintf(cdev, "(RX %s", status);
	status = bit_status(uart_addr + UCR2, UCR2_TXEN);
	vmm_cprintf(cdev, "/ TX %s)\n", status);


	div = (do_readl(uart_addr + UFCR) >> 7) & 0x7;
	if (div == 7) {
		vmm_printf("    RFDIV value is reserved\n");
	}
	else if (div == 6) {
		div = 7;
	} else {
		div = 6 - div;
	}
	freq = udiv32(freq, div);
	vmm_cprintf(cdev, "  UART port clock divided by %d (%d MHz)\n", div,
		    udiv32(freq, 1000000));

	ubmr = do_readw(uart_addr + UBMR);
	ubir = do_readw(uart_addr + UBIR);
	freq /= 16;
	baudrate = udiv32(freq, ubmr + 1) * (ubir + 1);
	vmm_cprintf(cdev, "  Baudrate %d\n", baudrate);
	vmm_host_iounmap(uart_addr);

	return VMM_OK;
}

static int cmd_imx6_uart(struct vmm_chardev *cdev, int argc, char **argv)
{
	int id = -1;

	if (argc < 3) {
		cmd_imx6_usage(cdev);
		return VMM_EFAIL;
	}

	id = atoi(argv[2]);
	return cmd_uart_info(cdev, id);
}


static int cmd_imx6_clocks(struct vmm_chardev *cdev, int __unused argc,
			  char __unused **argv)
{
	/* clk_summary_show(cdev, NULL); */
	return clk_dump(cdev, NULL);
}

static void cmd_imx6_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   imx6 uart X - Display UARTX info\n");
	vmm_cprintf(cdev, "   imx6 clocks - Display i.MX6 clock tree\n");
}

static int cmd_imx6_help(struct vmm_chardev *cdev,
			 int __unused argc,
			 char __unused **argv)
{
	cmd_imx6_usage(cdev);
	return VMM_OK;
}

static const struct {
	char *name;
	int (*function) (struct vmm_chardev *, int, char **);
} const command[] = {
	{"help", cmd_imx6_help},
	{"uart", cmd_imx6_uart},
	{"clocks", cmd_imx6_clocks},
	{NULL, NULL},
};

static int cmd_imx6_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	int index = 0;

	while (command[index].name) {
		if (strcmp(argv[1], command[index].name) == 0) {
			return command[index].function(cdev, argc, argv);
		}
		index++;
	}

	cmd_imx6_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_imx6 = {
	.name = "imx6",
	.desc = "control commands for imx6",
	.usage = cmd_imx6_usage,
	.exec = cmd_imx6_exec,
};

int __init imx6_command_setup(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_imx6);
}

