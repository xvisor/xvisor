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
 * @file vtemu.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Video terminal emulation library interface
 */

#ifndef __VTEMU_H_
#define __VTEMU_H_

#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <vmm_completion.h>
#include <vmm_chardev.h>
#include <input/vmm_input.h>
#include <fb/vmm_fb.h>
#include <vtemu_font.h>

#define VTEMU_NAME_SIZE		VMM_CHARDEV_NAME_SIZE
#define VTEMU_INBUF_SIZE	32
#define VTEMU_ESCMD_SIZE	(17 * 3)
#define VTEMU_ESC_NPAR		(16)

typedef enum {
	VTEMU_COLOR_BLACK,
	VTEMU_COLOR_RED,
	VTEMU_COLOR_GREEN,
	VTEMU_COLOR_YELLOW,
	VTEMU_COLOR_BLUE,
	VTEMU_COLOR_MAGENTA,
	VTEMU_COLOR_CYAN,
	VTEMU_COLOR_WHITE,
}vtemu_color;
#define VTEMU_DEFAULT_FC	VTEMU_COLOR_WHITE
#define VTEMU_DEFAULT_BC	VTEMU_COLOR_BLACK

struct vtemu_cell
{
	/* char value */
	u8 ch;

	/* cell location */
	u32 x, y;

	/* foreground color and background color */
	u32 fc, bc;
};

struct vtemu {
	/* pseudo character device */
	struct vmm_chardev cdev;

	/* underlying input handler */
	struct vmm_input_handler hndl;

	/* underlying frame buffer*/
	struct vmm_fb_info *info;

	/* video mode to be used */
	const struct vmm_fb_videomode *mode;

	/* color map to be used */
	struct vmm_fb_cmap cmap;

	/* fonts to be used */
	const struct vtemu_font *font;
	u32 font_img_sz;

	/* width and height */
	u32 w, h;

	/* current x, y */
	u32 x, y;
	u32 start_y;

	/* saved x, y */
	u32 saved_x, saved_y;

	/* current foreground color and background color */
	u32 fc, bc;

	/* saved fc, bc */
	u32 saved_fc, saved_bc;

	/* freeze state of vtemu */
	bool freeze;

	/* screen data */
	struct vtemu_cell *cell;
	u32 cell_head;
	u32 cell_tail;
	u32 cell_count;
	u32 cell_len;
	u8 *cursor_bkp;
	u32 cursor_bkp_size;
	u8 esc_cmd[VTEMU_ESCMD_SIZE];
	u8 esc_attrib[VTEMU_ESC_NPAR];
	u8 esc_cmd_count;
	u8 esc_attrib_count;
	bool esc_cmd_active;

	/* input data */
	u8 in_buf[VTEMU_INBUF_SIZE];
	u32 in_head;
	u32 in_tail;
	u32 in_count;
	u32 in_key_flags;
	vmm_spinlock_t in_lock;
	struct vmm_completion in_done;
};

static inline struct vmm_fb_info *vtemu_fbinfo(struct vtemu *v)
{
	return (v) ? v->info : NULL;
}

static inline struct vmm_chardev *vtemu_chardev(struct vtemu *v)
{
	return (v) ? &v->cdev : NULL;
}

/* Create vtemu instance */
struct vtemu *vtemu_create(const char *name, 
			   struct vmm_fb_info *info,
			   const char *font_name);

/* Destroy vtemu instance */
int vtemu_destroy(struct vtemu *v);

#endif /* __VTEMU_H_ */
