/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_stdio.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for standerd input/output
 */
#ifndef _VMM_STDIO_H__
#define _VMM_STDIO_H__

#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <vmm_chardev.h>

#define BUG_ON(x, bug_string, ...)				\
	do {							\
		if (x) {					\
			vmm_panic(bug_string, #__VA_ARGS__);	\
		}						\
	} while(0);

/** Check if a character is a control character */
bool vmm_iscontrol(char c);

/** Check if a character is printable */
bool vmm_isprintable(char c);

/** Low-level print char function */
int vmm_printchar(char **str, struct vmm_chardev *cdev, char c, bool block);

/** Put character to default terminal */
void vmm_putc(char ch);

/** Put character to character device */
void vmm_cputc(struct vmm_chardev *cdev, char ch);

/** Print formatted string to default terminal */
int vmm_printf(const char *format, ...);

/** Print formatted string to another string */
int vmm_sprintf(char *out, const char *format, ...);

/** Print formatted string to character device */
int vmm_cprintf(struct vmm_chardev *cdev, const char *format, ...);

/** Panic & Print formatted message */
int vmm_panic(const char *format, ...);

/** Low-level scan character function */
int vmm_scanchar(char **str, struct vmm_chardev *cdev, char *c, bool block);

/** Get character from default terminal */
char vmm_getc(void);

/** Get string from default terminal */
char *vmm_gets(char *s, int maxwidth, char endchar);

/** Get character device used by stdio */
struct vmm_chardev *vmm_stdio_device(void);

/** Change character device used by stdio */
int vmm_stdio_change_device(struct vmm_chardev * cdev);

/** Initialize standerd IO library */
int vmm_stdio_init(void);

#endif
