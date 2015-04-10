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
#include <libs/stacktrace.h>

#define BUG_ON(x)							\
	do {								\
		if (x) {						\
			vmm_printf("Bug in %s() at %s:%d\n",		\
				   __func__, __FILE__, __LINE__);	\
			dump_stacktrace();				\
			__vmm_panic("Please reset the system ...\n");	\
		}							\
	} while(0)

#define BUG()	BUG_ON(1)

#define WARN_ON(x)							\
	({								\
		if (x) {						\
			vmm_printf("Warning in %s() at %s:%d\n",	\
				   __func__, __FILE__, __LINE__);	\
			dump_stacktrace();				\
		}							\
		(x);							\
	})

#define WARN(x, msg...)							\
	({								\
		if (x) {						\
			vmm_printf("Warning: " msg);			\
			vmm_printf("Warning in %s() at %s:%d\n",	\
				   __func__, __FILE__, __LINE__);	\
			dump_stacktrace();				\
		}							\
		(x);							\
	})

/** Representation of input history for use with (c)gets */
struct vmm_history {
	int length;	/* Number of entries in the history table */
	int width;	/* Width of each entry */
	char **table;	/* Circular History Table */
	int tail;	/* Last entry */
};

/** Initialize vmm_history pointer h having l length and w width */
#define INIT_HISTORY(h,l,w)						\
	{	int iter = 0;						\
		(h)->length = (l);					\
		(h)->width = (w);					\
		(h)->table = vmm_malloc((l) * sizeof(char *));		\
		for (iter = 0; iter < (l) ; iter++) {			\
			(h)->table[iter] = vmm_malloc((w) * sizeof(char));\
			(h)->table[iter][0] = '\0';			\
		}							\
		(h)->tail = 0;						\
	}								

struct vmm_chardev;

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

/** Print formatted string to another string */
int vmm_sprintf(char *out, const char *format, ...);

/** Print formatted string to another string */
int vmm_snprintf(char *out, u32 out_sz, const char *format, ...);

/** Print formatted string to character device */
int vmm_cprintf(struct vmm_chardev *cdev, const char *format, ...);

/** Print formatted string to default device */
#define vmm_printf(...)	vmm_cprintf(NULL, __VA_ARGS__)

/** Predefined log levels */
enum vmm_print_level {
	VMM_LOGLEVEL_EMERGENCY=0,
	VMM_LOGLEVEL_ALERT=1,
	VMM_LOGLEVEL_CRITICAL=2,
	VMM_LOGLEVEL_ERROR=3,
	VMM_LOGLEVEL_WARNING=4,
	VMM_LOGLEVEL_NOTICE=5,
	VMM_LOGLEVEL_INFO=6,
};

/** Print formatted string to default device if current
 *  stdio log level is greater than or equal to specified level
 */
#define vmm_lprintf(level, msg...) \
	do {								\
		if (vmm_stdio_loglevel() >= (level))			\
			vmm_printf(msg);				\
	} while (0)

#define vmm_lemergency(msg...)	vmm_lprintf(VMM_LOGLEVEL_EMERGENCY, msg)
#define vmm_lalert(msg...)	vmm_lprintf(VMM_LOGLEVEL_ALERT, msg)
#define vmm_lcritical(msg...)	vmm_lprintf(VMM_LOGLEVEL_CRITICAL, msg)
#define vmm_lerror(msg...)	vmm_lprintf(VMM_LOGLEVEL_ERROR, msg)
#define vmm_lwarning(msg...)	vmm_lprintf(VMM_LOGLEVEL_WARNING, msg)
#define vmm_lnotice(msg...)	vmm_lprintf(VMM_LOGLEVEL_NOTICE, msg)
#define vmm_linfo(msg...)	vmm_lprintf(VMM_LOGLEVEL_INFO, msg)

/** Panic & Print formatted message
 * Note: This function is less verbose so perfer vmm_panic().
 */
void __noreturn __vmm_panic(const char *format, ...);

#define vmm_panic(msg...)						\
	do {								\
		vmm_printf(msg);					\
		dump_stacktrace();					\
		__vmm_panic("Please reset the system ...\n");		\
	} while(0)

/** Low-level scan characters function */
int vmm_scanchars(struct vmm_chardev *cdev, char *ch, u32 num_ch, bool block);

/** Get character from character device */
char vmm_cgetc(struct vmm_chardev *cdev, bool lecho) ;

/** Get character from default device */
char vmm_getc(bool lecho);

/** Get string from character device
 *  If history is NULL does not support UP/DN keys */
char *vmm_cgets(struct vmm_chardev *cdev, char *s, int maxwidth,
		char endchar, struct vmm_history *history, bool lecho);

/** Get string from default device
 *  If history is NULL does not support UP/DN keys */
char *vmm_gets(char *s, int maxwidth, char endchar,
	       struct vmm_history *history, bool lecho);

/** Get default character device used by stdio */
struct vmm_chardev *vmm_stdio_device(void);

/** Change default character device used by stdio */
int vmm_stdio_change_device(struct vmm_chardev *cdev);

/** Get log level used by stdio */
long vmm_stdio_loglevel(void);

/** Change log level used by stdio */
void vmm_stdio_change_loglevel(long loglevel);

/** Initialize standerd IO library */
int vmm_stdio_init(void);

#endif
