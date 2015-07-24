/**
 * Copyright (c) 2015 Himanshu Chauhan.
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
 * @file cpu_extables.h
 * @author Himanshu Chauhan (hchauhan@xvisor-x86.org)
 * @brief Exception fixup related header file.
 */

#ifndef __CPU_EXTABLES_H
#define __CPU_EXTABLES_H

#include <vmm_extable.h>
#include <arch_regs.h>

extern int fixup_exception(struct arch_regs *regs);

#endif /* __CPU_EXTABLES_H */
