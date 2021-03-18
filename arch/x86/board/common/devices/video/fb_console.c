/**
 * Copyright (c) 2021 Himanshu Chauhan.
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
 * @file fb_console.c
 * @author Himanshu Chauhan (hchauhan@xvisor-x86.org)
 * @brief Framebuffer console.
 */

#include <vmm_types.h>
#include <libs/fifo.h>
#include <vmm_error.h>
#include <vmm_compiler.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <vmm_completion.h>
#include <vmm_params.h>
#include <libs/vtemu.h>
#include <drv/input.h>
#include <video/fb_console.h>
#include <video/svga.h>
#include <video/ter-i16b.h>
#include <video/ter-i16n.h>
#include <brd_defterm.h>
#include <multiboot.h>

#if defined(CONFIG_VTEMU)

static struct fifo *fb_fifo;
static struct vmm_completion fb_fifo_cmpl;
static u32 fb_key_flags;
static struct input_handler fb_hndl;
static bool fb_key_handler_registered;

//static void fb_console_putpixel_24bpp(u8* screen, int x, int y, u32 color);
static void fb_console_scroll_up(unsigned int num_rows);

/* Pointers to fonts */
static u8 *font_reg, *font_bold;
/* Video mode info */
static u16 width, height, depth, bytesPerLine;
static void *video_base;//, video_size;
/* Cursor location (in text cells) */
static u16 col, row;
/* Set by escape sequences */
static u8 fg_colour, bg_colour;
/* Used to parse escape codes */
static bool next_char_is_escape_seq, is_bold;

/* Colour code -> 16bpp */
static u32 fb_console_col_map[16] = {
	0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
	0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,

	0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
	0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
};

static void memclr(void *area, u32 size)
{
	memset(area, 0, size);
}

static int fb_key_event(struct input_handler *ihnd,
			struct input_dev *idev,
			unsigned int type, unsigned int code, int value)
{
	int rc, i, len;
	char str[16];
	u32 key_flags;

	if (value) { /* value=1 (key-up) or value=2 (auto-repeat) */
		/* Update input key flags */
		key_flags = vtemu_key2flags(code);
		if ((key_flags & VTEMU_KEYFLAG_LOCKS) &&
		    (fb_key_flags & key_flags)) {
			fb_key_flags &= ~key_flags;
		} else {
			fb_key_flags |= key_flags;
		}

		/* Retrive input key string */
		rc = vtemu_key2str(code, fb_key_flags, str);
		if (rc) {
			return VMM_OK;
		}

		/* Add input key string to input buffer */
		len = strlen(str);
		for (i = 0; i < len; i++) {
			fifo_enqueue(fb_fifo, &str[i], TRUE);
			vmm_completion_complete(&fb_fifo_cmpl);
		}
	} else { /* value=0 (key-down) */
		/* Update input key flags */
		key_flags = vtemu_key2flags(code);
		if (!(key_flags & VTEMU_KEYFLAG_LOCKS)) {
			fb_key_flags &= ~key_flags;
		}
	}

	return VMM_OK;
}

int fb_defterm_getc(u8 *ch)
{
	int rc;

	if (!fb_key_handler_registered) {
		memset(&fb_hndl, 0, sizeof(fb_hndl));
		fb_hndl.name = "fbterm";
		fb_hndl.evbit[0] |= BIT_MASK(EV_KEY);
		fb_hndl.event = fb_key_event;
		fb_hndl.priv = NULL;

		rc = input_register_handler(&fb_hndl);
		if (rc) {
			return rc;
		}

		rc = input_connect_handler(&fb_hndl);
		if (rc) {
			return rc;
		}

		fb_key_handler_registered = TRUE;
	}

	if (fb_fifo) {
		/* Assume that we are always called from
		 * Orphan (or Thread) context hence we can
		 * sleep waiting for input characters.
		 */
		vmm_completion_wait(&fb_fifo_cmpl);

		/* Try to dequeue from defterm fifo */
		if (!fifo_dequeue(fb_fifo, ch)) {
			return VMM_ENOTAVAIL;
		}

		return VMM_OK;
	}

	return VMM_EFAIL;
}

extern struct multiboot_info boot_info;

/*
 * Initialises the framebuffer console
 */
int fb_defterm_init(void)
{
	vmm_printf("%s: init\n", __func__);

	fb_fifo = fifo_alloc(sizeof(u8), 128);
	if (!fb_fifo) {
		vmm_printf("%s: No memory for fifo\n", __func__);
		return VMM_ENOMEM;
	}
	INIT_COMPLETION(&fb_fifo_cmpl);

	fb_key_flags = 0;
	fb_key_handler_registered = FALSE;

	bytesPerLine = boot_info.framebuffer_pitch;
	width = boot_info.framebuffer_width;
	height = boot_info.framebuffer_height;
	depth = boot_info.framebuffer_bpp/8;
	vmm_printf("%s: BPL: %d width: %d height: %d depth: %d\n", __func__,
		   bytesPerLine, width, height, depth);

	video_base = (void*)svga_map_fb(boot_info.framebuffer_addr, bytesPerLine*height);
	vmm_printf("%s: Video base: %p\n", __func__, video_base);
	fb_console_set_font(&ter_i16n_raw, &ter_i16b_raw);

	/* Clear screen */
	memclr((void *) video_base, bytesPerLine * height);

	is_bold = false;
	next_char_is_escape_seq = false;
	fg_colour = 0x0F;
	bg_colour = 0x00;

	col = row = 0;

	return VMM_OK;
}

/*
 * Handles the character as a control character.
 */
void fb_console_control(unsigned char c)
{
	switch(c) {
		case '\n':
			col = 0;
			/* We're on the last row, a newline should only scroll the viewport up */
			if(row == (height / CHAR_HEIGHT)-1) {
				fb_console_scroll_up(1);
			} else {
				row++;
			}

			break;

		default:
			break;
	}
}

/*
 * Prints a character to the framebuffer console.
 */
int fb_defterm_putc(unsigned char c)
{
	if (c == '\n') {
		fb_console_control('\n');
		return VMM_OK;
	}

	if(c == 0x01) {
		next_char_is_escape_seq = true;
		return VMM_OK;
	}

	/* Check if the following character is part of an escape sequence */
	if(next_char_is_escape_seq) {
		/* Codes 0x00 to 0x0F are colours */
		if(c >= 0x00 && c <= 0x0F) {
			fg_colour = c;
			next_char_is_escape_seq = false;
		} else if(c == 0x10 || c == 0x11) {
			is_bold = (c == 0x11) ? true : false;

			next_char_is_escape_seq = false;
		} else {
			next_char_is_escape_seq = false;
		}
	} else { /* Handle printing of a regular character */
		/* Characters are 16 px tall, i.e. 0x10 bytes in stored rep */
		u8 *read_ptr = (u8 *) ((is_bold) ? font_bold : font_reg) + (c * CHAR_HEIGHT);
		u32 *write_base;

		const u8 x_to_bitmap[CHAR_WIDTH] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
		u8 fontChar = 0;
		u32 out = 0;

		for(u8 y = 0; y < CHAR_HEIGHT; y++) {
			fontChar = read_ptr[y];

			/* Process one column at a time */
			if(depth == 4) {
				write_base = (u32 *) (video_base) + (((bytesPerLine / 4) * CHAR_HEIGHT * row)) + (CHAR_WIDTH * col);

				for(u8 x = 0; x < CHAR_WIDTH; x++) {
					if(x_to_bitmap[x] & fontChar) {
						write_base[x+(y * (bytesPerLine / 4))] = fb_console_col_map[fg_colour];
					}
				}
			} else if(depth == 2) {
				write_base = (u32 *) (video_base) + (((bytesPerLine / 4) * CHAR_HEIGHT * row)) + ((CHAR_WIDTH * (col)) >> 1);

				/* In 16bpp, process two pixels at once */
				for(u8 x = 0; x < CHAR_WIDTH; x+=2) {
					out = 0;

					if(x_to_bitmap[x] & fontChar) {
						out = ((SVGA_24TO16BPP(fb_console_col_map[fg_colour])) & 0xFFFF) << 16;
					}

					if(x_to_bitmap[x+1] & fontChar) {
						out |= ((SVGA_24TO16BPP(fb_console_col_map[fg_colour])) & 0xFFFF);
					}

					write_base[(x >> 1) + (y * (bytesPerLine / 4))] = out;
				}
			}
		}

		/* Increment column and check row */
		col++;

		if(col > (width / CHAR_WIDTH)) {
			fb_console_control('\n');
		}
	}

	return VMM_OK;
}

/*
 * Sets the regular and bold fonts. If a font pointer is NULL, it is ignored.
 */
void fb_console_set_font(void* reg, void* bold)
{
	if(reg) font_reg = reg;
	if(bold) font_bold = bold;
}

/*
 * Scrolls the display up number of rows
 */
static void fb_console_scroll_up(unsigned int num_rows)
{
	/* Copy rows upwards */
	u8 *read_ptr = (u8 *) video_base + ((num_rows * CHAR_HEIGHT) * bytesPerLine);
	u8 *write_ptr = (u8 *) video_base;
	unsigned int num_bytes = (bytesPerLine * height) - (bytesPerLine * (num_rows * CHAR_HEIGHT));
	memcpy(write_ptr, read_ptr, num_bytes);

	/* Clear the rows at the end */
	read_ptr = (u8 *)(video_base + (bytesPerLine * height) - (bytesPerLine * (num_rows * CHAR_HEIGHT)));
	memclr(read_ptr, bytesPerLine * (num_rows * CHAR_HEIGHT));
}

#if 0
/*
 * Plots a pixel in 24bpp mode.
 */
static void fb_console_putpixel_24bpp(u8* screen, int x, int y, u32 color)
{
    int where = (x * depth) + (y * bytesPerLine);
    screen[where] = color & 255;              // BLUE
    screen[where + 1] = (color >> 8) & 255;   // GREEN
    screen[where + 2] = (color >> 16) & 255;  // RED
}
#endif

static struct defterm_ops fb_ops = {
	.putc = fb_defterm_putc,
	.getc = fb_defterm_getc,
	.init = fb_defterm_init
};

struct defterm_ops *get_fb_defterm_ops(void *data)
{
	return &fb_ops;
}

#else /* !CONFIG_VT_EMU */

struct defterm_ops *get_fb_defterm_ops(void *data)
{
	return NULL;
}
#endif
