/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file vmm_board.h
 * @version 1.0
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief board specific initialization functions
 */
#ifndef _VMM_BOARD_H__
#define _VMM_BOARD_H__

#include "vmm_types.h"
#include "vmm_devtree.h"

int vmm_defterm_getc(u8 *ch);
int vmm_defterm_putc(u8 ch);
int vmm_defterm_init(void);

/** Interrupt controller related function required by VMM core */
u32 vmm_pic_irq_count(void);
int vmm_pic_cpu_to_host_map(u32 cpu_irq_no);
int vmm_pic_pre_condition(u32 host_irq_no);
int vmm_pic_post_condition(u32 host_irq_no);
int vmm_pic_irq_enable(u32 host_irq_no);
int vmm_pic_irq_disable(u32 host_irq_no);
int vmm_pic_init(void);

/** RAM related functions required by VMM core */
int vmm_board_ram_start(physical_addr_t * addr);
int vmm_board_ram_size(physical_size_t * size);

int vmm_devtree_populate(vmm_devtree_node_t **root,
                         char **string_buffer,
                         size_t *string_buffer_size);
int vmm_board_getclock(vmm_devtree_node_t *node, u32 *clock);

int vmm_board_early_init(void);
int vmm_board_final_init(void);
int vmm_board_reset(void);
int vmm_board_shutdown(void);

#endif
