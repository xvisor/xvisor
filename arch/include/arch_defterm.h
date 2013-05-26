/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file arch_defterm.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief generic interface for arch specific default terminal
 */
#ifndef _ARCH_DEFTERM_H__
#define _ARCH_DEFTERM_H__

#include <vmm_types.h>

/** Get input character from default terminal */
int arch_defterm_getc(u8 *ch);

/** Early output character to default terminal 
 *  NOTE: This function is optional.
 *  NOTE: it is called before default terminal is initialized.
 *  NOTE: If arch implments this function then arch_config.h
 *  will define ARCH_HAS_DEFTERM_EARLY_PRINT feature.
 */
void arch_defterm_early_putc(u8 ch);

/** Output character to default terminal */
int arch_defterm_putc(u8 ch);

/** Initialize default terminal */
int arch_defterm_init(void);

#endif
