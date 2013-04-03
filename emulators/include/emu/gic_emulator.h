/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @file gic_emulator.h
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief Generic Interrupt Controller Emulator exported APIs
 */
#ifndef __GIC_EMULATOR_H__
#define __GIC_EMULATOR_H__

/** State is private to emulator */
struct gic_state;

enum gic_type {
	GIC_TYPE_ARM11MPCORE,
	GIC_TYPE_REALVIEW,
	GIC_TYPE_VEXPRESS,
	GIC_TYPE_VEXPRESS_V2,
};

/* GIC register write */
int gic_reg_write(struct gic_state *s, physical_addr_t offset,
		  u32 src_mask, u32 src);

/** GIC register read */
int gic_reg_read(struct gic_state *s, physical_addr_t offset, u32 *dst);

/** Resets the GIC state */
int gic_state_reset(struct gic_state *s);

/** Allocate and initializes the GIC state */
struct gic_state *gic_state_alloc(struct vmm_guest *guest,
				  enum gic_type type,
				  u32 num_cpu,
				  bool is_child_pic,
	       			  u32 parent_irq);

/** Destructor for the GIC state */
int gic_state_free(struct gic_state *s);

#endif
