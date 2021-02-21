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
 * @file brd_defterm.h
 * @author Himanshu Chauhan (hchauhan@xvisor-x86.org)
 * @brief Board default terminal data structures.
 */

#ifndef __BRD_DEFTERM_H
#define __BRD_DEFTERM_H

#define SERIAL_CONSOLE_NAME	"serial"
#define VGA_CONSOLE_NAME	"vga"
#define FB_CONSOLE_NAME		"fb"

#define SERIAL0_CONFIG_DTS_PATH	"/motherboard/uart0"
#define SERIAL1_CONFIG_DTS_PATH	"/motherboard/uart1"
#define VGA_CONFIG_DTS_PATH	"/motherboard/vga"
#define FB_CONFIG_DTS_PATH      "/motherboard/fb"

#define DEFAULT_CONSOLE_STR	"console=vga"

#define CONSOLE_SETUP_STR_LEN	1024

struct defterm_ops {
	int (*putc)(u8 ch);
	int (*getc)(u8 *ch);
	int (*init)(void);
};

typedef int (*EARLY_PUTC)(u8 ch);

extern EARLY_PUTC early_putc;

#endif /* __BRD_DEFTERM_H */
