/**
 * Copyright (c) 2014 Himanshu Chauhan
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
 * @file i8259.h
 * @author Himanshu Chauhan <hschauhan@nulltrace.org>
 * @brief Definitions related to i8259 PIC.
 *
 * This file has been largely adapted from QEMU source code. (i8259_internal.h)
 * Copyright (c) 2011 Jan Kiszka, Siemens AG
 */

#ifndef _I8259_INTERNAL_H
#define _I8259_INTERNAL_H

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_manager.h>

typedef struct i8259_state i8259_state_t;

struct i8259_state {
	struct dlist head;

	u8 last_irr; /* edge detection */
	u8 irr; /* interrupt request register */
	u8 imr; /* interrupt mask register */
	u8 isr; /* interrupt service register */
	u8 priority_add; /* highest irq priority */
	u8 int_base; /* base of CPU programmed vector */
	u8 read_reg_select;
	u8 poll;
	u8 special_mask;
	u8 init_state;
	u8 auto_eoi;
	u8 rotate_on_auto_eoi;
	u8 special_fully_nested_mode;
	u8 init4; /* true if 4 byte init */
	u8 single_mode; /* true if slave pic is not initialized */
	u8 elcr; /* PIIX edge/trigger selection*/
	u8 elcr_mask;
	u32 master; /* reflects /SP input pin */
	u32 iobase;
	u32 elcr_addr;
	struct vmm_guest *guest;
	vmm_spinlock_t lock;
	u32 base_irq;
	u32 num_irq;
	u32 parent_irq;
	u32 pic_slave_id; /* valid only if slave */
};

void arch_set_guest_master_pic(struct vmm_guest *guest, struct i8259_state *s);
void arch_set_guest_pic_list(struct vmm_guest *guest, void *pic_list);
void *arch_get_guest_pic_list(struct vmm_guest *guest);
int pic_read_irq(i8259_state_t *s);

#endif /* !_I8259_INTERNAL_H */
