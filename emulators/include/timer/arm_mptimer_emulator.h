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
 * @file arm_mptimer_emulator.h
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief ARM MP private & watchdog timer emulator exported APIs
 */
#ifndef __ARM_MPTIMER_EMULATOR_H__
#define __ARM_MPTIMER_EMULATOR_H__

/** State is private to emulator */
struct mptimer_state;

/** MPTimer Register write */
int mptimer_reg_write(struct mptimer_state *s, u32 offset, u32 src_mask, 
		      u32 src);

/** MPTimer Register read */
int mptimer_reg_read(struct mptimer_state *s, u32 offset, u32 *dst);

/** Resets the MPTimer state */
int mptimer_state_reset(struct mptimer_state *mpt);

/** Allocate and initializes the MPTimer state */
struct mptimer_state *mptimer_state_alloc(struct vmm_guest *guest,
					  struct vmm_emudev * edev, 
					  u32 num_cpu,
					  u32 periphclk,
					  u32 irq[]);

/** Destructor for the MPTimer state */
int mptimer_state_free(struct mptimer_state *s);


#endif /* __ARM_MPTIMER_EMULATOR_H__ */
