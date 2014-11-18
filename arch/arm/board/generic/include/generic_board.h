/**
 * Copyright (c) 2014 Anup Patel.
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
 * @file generic_board.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Generic board interface
 */
#ifndef __GENERIC_BOARD_H_
#define __GENERIC_BOARD_H_

#include <vmm_limits.h>
#include <vmm_devtree.h>
#include <vmm_chardev.h>

struct generic_board {
	char name[VMM_FIELD_NAME_SIZE];
	int (*early_init)(struct vmm_devtree_node *node);
	int (*final_init)(struct vmm_devtree_node *node);
	void (*print_info)(struct vmm_chardev *cdev);
};

/* declare nodeid table based initialization for generic board */
#define GENERIC_BOARD_DECLARE(name, compat, inst)	\
VMM_DEVTREE_NIDTBL_ENTRY(name, "generic_board", "", "", compat, inst)

#endif
