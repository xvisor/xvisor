/**
 * Copyright (c) 2021 Himanshu Chauhan.
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
 * @file brd_defterm.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief text-mode VGA console as default terminal.
 */

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_compiler.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <vmm_completion.h>
#include <vmm_params.h>
#include <libs/vtemu.h>
#include <libs/fifo.h>
#include <drv/input.h>
#include <brd_defterm.h>
#include <video/vga.h>
#include <video/fb_console.h>
#include <serial.h>

static char cmdline_console_string[CONSOLE_SETUP_STR_LEN];
static struct defterm_ops *ops = NULL;
EARLY_PUTC early_putc = NULL;
int init_early_fb_console(void);

void arch_defterm_early_putc(u8 ch)
{
	if (early_putc)
		early_putc(ch);
}

/*
 * Just set what is needed. Ops will be initialzied when
 * arch_defterm_init will be called.
 */
static int __init setup_early_print(char *buf)
{
	if (!strncmp(buf, SERIAL_CONSOLE_NAME, strlen(SERIAL_CONSOLE_NAME)))
		return init_early_serial_console(buf);

	if (!strncmp(buf, VGA_CONSOLE_NAME, strlen(VGA_CONSOLE_NAME)))
		return init_early_vga_console();

	if (!strncmp(buf, FB_CONSOLE_NAME, strlen(FB_CONSOLE_NAME)))
		return init_early_fb_console();

	return VMM_EFAIL;
}

vmm_early_param("earlyprint", setup_early_print);

static int __init set_default_console(char *buf)
{
	if (buf == NULL) return VMM_OK;

	if (!strncmp(buf, SERIAL_CONSOLE_NAME, strlen(SERIAL_CONSOLE_NAME))) {
		memcpy(cmdline_console_string, buf, CONSOLE_SETUP_STR_LEN);
		return VMM_OK;
	}

	if (!strncmp(buf, FB_CONSOLE_NAME, strlen(FB_CONSOLE_NAME))) {
		memcpy(cmdline_console_string, buf, CONSOLE_SETUP_STR_LEN);
		return VMM_OK;
	}

	vmm_snprintf(cmdline_console_string, strlen(DEFAULT_CONSOLE_STR),
		     DEFAULT_CONSOLE_STR);

	return VMM_OK;
}

vmm_early_param("console", set_default_console);

int arch_defterm_putc(u8 ch)
{
	if (ops)
		return ops->putc(ch);

	return VMM_EFAIL;
}

int arch_defterm_getc(u8 *ch)
{
	if (ops)
		return ops->getc(ch);

	return VMM_EFAIL;
}

int __init arch_defterm_init(void)
{
	int rc;

	vmm_printf("%s: init (%s)\n", __func__, cmdline_console_string);
	if (!strcmp(cmdline_console_string, "fb")) {
		vmm_printf("%s: Framebuffer as console\n", __func__);
		ops = get_fb_defterm_ops(NULL);
		goto out;
	}

	if (strcmp(cmdline_console_string, "serial") >= 0) {
		vmm_printf("%s: Serial as console\n", __func__);
		ops = get_serial_defterm_ops((char *)&cmdline_console_string[0]);
		goto out;
	}

	vmm_printf("%s: Defaulting to vga as console\n", __func__);
	ops = get_vga_defterm_ops(NULL);
 out:
	rc = ops->init();

	return rc;
}
