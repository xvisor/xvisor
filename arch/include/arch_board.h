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
 * @file arch_board.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief generic interface for arch specific Board functions
 */
#ifndef _ARCH_BOARD_H__
#define _ARCH_BOARD_H__

#include <vmm_types.h>

struct vmm_chardev;

/** Print board specific information */
void arch_board_print_info(struct vmm_chardev *cdev);

/**
 * Board nascent init
 *
 * Only Host aspace, Heap, and Device tree available.
 */
int arch_board_nascent_init(void);

/**
 * Board early init
 *
 * Only Host aspace, Heap, Device tree, Per-CPU areas, CPU hotplug,
 * and Host IRQ available.
 */
int arch_board_early_init(void);

/**
 * Board final init
 *
 * Almost all initialization (including builtin module) done. Only
 * driver probing remains which has to be done by this function.
 */
int arch_board_final_init(void);

#endif
