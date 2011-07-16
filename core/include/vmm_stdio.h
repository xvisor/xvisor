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
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for standerd input/output
 */
#ifndef _VMM_STDIO_H__
#define _VMM_STDIO_H__

#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <vmm_chardev.h>

struct vmm_stdio_ctrl {
	vmm_spinlock_t lock;
	vmm_chardev_t *cdev;
};

typedef struct vmm_stdio_ctrl vmm_stdio_ctrl_t;

/** Low-level print char function */
int vmm_printchar(char **str, char c, bool block);

/** putc for VMM */
void vmm_putc(char ch);

/** printf for VMM */
int vmm_printf(const char *format, ...);

/** sprintf for VMM */
int vmm_sprintf(char *out, const char *format, ...);

/** panic function for VMM */
int vmm_panic(const char *format, ...);

/** getc for VMM */
char vmm_getc(void);

/** Low-level scan character function */
int vmm_scanchar(char **str, char *c, bool block);

/** gets for VMM */
char *vmm_gets(char *s, int maxwidth, char endchar);

/** Get character device used by stdio for input/output */
vmm_chardev_t *vmm_stdio_device(void);

/** Change character device used by stdio for input/output */
int vmm_stdio_change_device(vmm_chardev_t * cdev);

/** Initialize stdio library */
int vmm_stdio_init(void);

#endif
