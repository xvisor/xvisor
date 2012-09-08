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
#include <vmm_compiler.h>
#include <vmm_spinlocks.h>
#include <vmm_chardev.h>
#include <stacktrace.h>

#define BUG_ON(x)							\
	do {								\
		if (x) {						\
			vmm_panic("Bug in %s() at %s:%d\n",		\
				   __func__, __FILE__, __LINE__);	\
		}							\
	} while(0)

#define BUG()	BUG_ON(1)

#define WARN_ON(x)							\
	do {								\
		if (x) {						\
			vmm_printf("Warning in %s() at %s:%d\n",	\
				   __func__, __FILE__, __LINE__);	\
			dump_stacktrace();				\
		}							\
	} while(0)

#define WARN()	WARN_ON(1)

/** Check if a character is a control character */
bool vmm_iscontrol(char c);

/** Check if a character is printable */
bool vmm_isprintable(char c);

/** Low-level print characters function */
int vmm_printchars(struct vmm_chardev *cdev, char *ch, u32 num_ch, bool block);

/** Put character to character device */
void vmm_cputc(struct vmm_chardev *cdev, char ch);

/** Put character to default device */
void vmm_putc(char ch);

/** Put string to character device */
void vmm_cputs(struct vmm_chardev *cdev, char *str);

/** Put string to default device */
void vmm_puts(char *str);

/** Print formatted string to default device */
int vmm_printf(const char *format, ...);

/** Print formatted string to another string */
int vmm_sprintf(char *out, const char *format, ...);

/** Print formatted string to another string */
int vmm_snprintf(char *out, u32 out_sz, const char *format, ...);

/** Print formatted string to character device */
int vmm_cprintf(struct vmm_chardev *cdev, const char *format, ...);

/** Panic & Print formatted message */
void __noreturn vmm_panic(const char *format, ...);

/** Low-level scan characters function */
int vmm_scanchars(struct vmm_chardev *cdev, char *ch, u32 num_ch, bool block);

/** Get character from character device */
char vmm_cgetc(struct vmm_chardev *cdev) ;

/** Get character from default device */
char vmm_getc(void);

/** Get string from character device */
char *vmm_cgets(struct vmm_chardev *cdev, char *s, int maxwidth, char endchar);

/** Get string from default device */
char *vmm_gets(char *s, int maxwidth, char endchar);

/** Get default character device used by stdio */
struct vmm_chardev *vmm_stdio_device(void);

/** Change default character device used by stdio */
int vmm_stdio_change_device(struct vmm_chardev * cdev);

/** Initialize standerd IO library */
int vmm_stdio_init(void);

#endif
