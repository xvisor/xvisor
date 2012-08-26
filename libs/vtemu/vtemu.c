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
 * @file vtemu.c
 * @author Anup Patel. (anup@brainfault.org)
 * @brief Video terminal emulation library implementation
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <stringlib.h>
#include <mathlib.h>
#include <vtemu.h>

#define VTEMU_ERASE_CHAR			'\0'
#define VTEMU_TABSPACE_COUNT			5

static void vtemu_cell_draw(struct vtemu *v, struct vtemu_cell *vcell)
{
	struct vmm_fb_image img;

	if ((vcell->y < v->start_y) ||
	    ((v->start_y + v->h) <= vcell->y)) {
		return;
	}

	img.dx = vcell->x * v->font->width;
	img.dy = (vcell->y - v->start_y) * v->font->height;
	img.width = v->font->width;
	img.height = v->font->height;
	img.fg_color = vcell->fc;
	img.bg_color = vcell->bc;
	img.depth = 1;
	img.data = v->font->data + v->font_img_sz * vcell->ch;
	img.cmap.start = 0;
	img.cmap.len = 0;
	img.cmap.red = NULL;
	img.cmap.green = NULL;
	img.cmap.blue = NULL;
	img.cmap.transp = NULL;

	v->info->fbops->fb_imageblit(v->info, &img);
}

static void vtemu_cursor_erase(struct vtemu *v)
{
	u8 *fbdst;
	u32 dx, dy, fboffset;

	if ((v->start_y + v->h) <= v->y) {
		return;
	}

	dx = v->x * v->font->width;
	dy = (v->y - v->start_y + 1) * v->font->height - 2;
	fboffset = ((dy * v->info->var.xres_virtual) + dx) * 
					v->info->var.bits_per_pixel;
	fboffset = fboffset / 8;
	fbdst = (u8 *)v->info->screen_base + fboffset;

	fb_memcpy_tofb(fbdst, v->cursor_bkp, v->cursor_bkp_size);
}

static void vtemu_cursor_draw(struct vtemu *v)
{
	u8 *fbsrc;
	u32 fboffset;
	struct vmm_fb_fillrect rect;

	if ((v->start_y + v->h) <= v->y) {
		return;
	}

	rect.dx = v->x * v->font->width;
	rect.dy = (v->y - v->start_y + 1) * v->font->height - 2;
	rect.width = v->font->width;
	rect.height = 1;
	rect.color = v->fc;
	rect.rop = ROP_COPY;

	fboffset = ((rect.dy * v->info->var.xres_virtual) + rect.dx) * 
						v->info->var.bits_per_pixel;
	fboffset = fboffset / 8;
	fbsrc = (u8 *)v->info->screen_base + fboffset;
	fb_memcpy_fromfb(v->cursor_bkp, fbsrc, v->cursor_bkp_size);

	v->info->fbops->fb_fillrect(v->info, &rect);
}

static void vtemu_cursor_clear_down(struct vtemu *v)
{
	u32 pos, c;
	struct vmm_fb_fillrect rect;

	if ((v->start_y + v->h) <= v->y) {
		return;
	}

	rect.dx = v->x * v->font->width;
	rect.dy = (v->y - v->start_y) * v->font->height;
	rect.width = (v->w - v->x) * v->font->width;
	rect.height = (v->h - v->y + v->start_y) * v->font->height;
	rect.color = v->bc;
	rect.rop = ROP_COPY;

	v->info->fbops->fb_fillrect(v->info, &rect);

	vtemu_cursor_draw(v);

	pos = v->cell_head;
	for (c = 0; c < v->cell_count; c++) {
		if ((v->x <= v->cell[pos].x) &&
		    (v->y <= v->cell[pos].y)) {
			v->cell[pos].ch = VTEMU_ERASE_CHAR;
		}
		pos++;
		if (pos == v->cell_len) {
			pos = 0;
		}
	}
}

static void vtemu_scroll_down(struct vtemu *v, u32 lines)
{
	u32 c, pos;
	struct vmm_fb_copyarea reg;
	struct vmm_fb_fillrect rect;

	if (!lines) {
		return;
	}

	reg.dx = 0;
	reg.dy = 0;
	reg.width = (v->w - lines) * v->font->width;
	reg.height = (v->h - lines) * v->font->height;
	reg.sx = 0;
	reg.sy = lines * v->font->height;

	v->info->fbops->fb_copyarea(v->info, &reg);

	rect.dx = 0;
	rect.dy = (v->h - lines) * v->font->height;
	rect.width = v->w * v->font->width;
	rect.height = lines * v->font->height;
	rect.color = v->bc;
	rect.rop = ROP_COPY;

	v->info->fbops->fb_fillrect(v->info, &rect);

	v->start_y += lines;

	pos = v->cell_head;
	for (c = 0; c < v->cell_count; c++) {
		if ((v->start_y + v->h - lines) <= v->cell[pos].y) {
			vtemu_cell_draw(v, &v->cell[pos]);
		}
		pos++;
		if (pos == v->cell_len) {
			pos = 0;
		}
	}
}

static int vtemu_putchar(struct vtemu *v, u8 ch)
{
	u32 i;

	/* Erase cursor */
	vtemu_cursor_erase(v);

	switch (ch) {
	case '\t':
		/* Update location */
		for (i = 0; i < VTEMU_TABSPACE_COUNT; i++) {
			if (v->x == v->w) {
				break;
			}
			v->x++;
		}
		if (v->x == v->w) {
			v->x = 0;
			v->y++;
			if (v->y == (v->start_y + v->h)) {
				vtemu_scroll_down(v, 1);
			}
		}

		break;

	case '\b':
		/* Update location */
		if (v->x) {
			v->x--;
		}

		break;

	case '\r':
		/* Update location */
		v->x = 0;

		break;

	case '\n':
		/* Update location */
		v->y++;
		if (v->y == (v->start_y + v->h)) {
			vtemu_scroll_down(v, 1);
		}

		break;

	default:
		/* Pop cell if full */
		if ((v->cell_tail == v->cell_head) &&
		    (v->cell_count == v->cell_len)) {
			v->cell_head++;
			if (v->cell_head == v->cell_len) {
				v->cell_head = 0;
			}
			v->cell_count--;
		}

		/* Save character to cell */
		v->cell[v->cell_tail].ch = ch;
		v->cell[v->cell_tail].x = v->x;
		v->cell[v->cell_tail].y = v->y;
		v->cell[v->cell_tail].fc = v->fc;
		v->cell[v->cell_tail].bc = v->bc;
	
		/* Draw cell */
		vtemu_cell_draw(v, &v->cell[v->cell_tail]);
	
		/* Next cell */
		v->cell_tail++;
		if (v->cell_tail == v->cell_len) {
			v->cell_tail = 0;
		}
		v->cell_count++;
	
		/* Update location */
		v->x++;
		if (v->x == v->w) {
			v->x = 0;
			v->y++;
			if (v->y == (v->start_y + v->h)) {
				vtemu_scroll_down(v, 1);
			}
		}

		break;
	};

	
	/* Draw cursor */
	vtemu_cursor_draw(v);

	return VMM_OK;
}

static int vtemu_startesc(struct vtemu *v)
{
	v->esc_cmd_active = TRUE;
	v->esc_cmd_count = 0;
	v->esc_attrib_count = 0;
	v->esc_attrib[0] = 0;
	return VMM_OK;
}

static int vtemu_putesc(struct vtemu *v, u8 ch)
{
	u32 tmp;

	if (v->esc_cmd_count < VTEMU_ESCMD_SIZE) {
		v->esc_cmd[v->esc_cmd_count] = ch;
		v->esc_cmd_count++;
	} else {
		v->esc_cmd_active = FALSE;
		return VMM_OK;
	}

	switch(v->esc_cmd[0]) {
	case 'c':	/* Reset */
		/* FIXME */
		v->fc = VTEMU_DEFAULT_FC;
		v->bc = VTEMU_DEFAULT_BC;
		v->esc_cmd_active = FALSE;
		break;
	case 'r':	/* Enable Scrolling */
	case 'D':	/* Scroll Down one line or linefeed */
	case 'M':	/* Scroll Up one line or reverse-linefeed */
		/* FIXME */
		v->esc_cmd_active = FALSE;
		break;
	case 'E':	/* Newline */
		/* FIXME */
		v->esc_cmd_active = FALSE;
		break;
	case '7':	/* Save Cursor Position and Attrs */
		v->saved_x = v->x;
		v->saved_y = v->y;
		v->saved_fc = v->fc;
		v->saved_bc = v->bc;
		v->esc_cmd_active = FALSE;
		break;
	case '8':	/* Restore Cursor Position and Attrs */
		v->x = v->saved_x;
		v->y = v->saved_y;
		v->fc = v->saved_fc;
		v->bc = v->saved_bc;
		v->esc_cmd_active = FALSE;
		break;
	case '[':		/* CSI codes */
		if(v->esc_cmd_count == 1) {
			break;
		}

		switch(v->esc_cmd[v->esc_cmd_count - 1]) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			v->esc_attrib[v->esc_attrib_count] *= 10;
			v->esc_attrib[v->esc_attrib_count] += 
					(v->esc_cmd[v->esc_cmd_count - 1] - '0');
			break;
		case ';':
			v->esc_attrib_count++;
			v->esc_attrib[v->esc_attrib_count] = 0;
			break;
		case 'D':		/* Move Left */
			tmp = v->esc_attrib[0];
			tmp = (tmp) ? tmp : 1;

			vtemu_cursor_erase(v);

			while (v->x && tmp) {
				v->x--;
				tmp--;
			}
			v->esc_cmd_active = FALSE;

			vtemu_cursor_draw(v);
			break;
		case 'C':		/* Move Right */
			tmp = v->esc_attrib[0];
			tmp = (tmp) ? tmp : 1;

			vtemu_cursor_erase(v);

			while (tmp) {
				v->x++;
				if (v->x == v->w) {
					v->x = 0;
					v->y++;
					if (v->y == (v->start_y + v->h)) {
						vtemu_scroll_down(v, 1);
					}
				}
				tmp--;
			}
			v->esc_cmd_active = FALSE;

			vtemu_cursor_draw(v);
			break;
		case 'm':		/* Set Display Attributes */
			for(tmp = 0; tmp <= v->esc_attrib_count; tmp++) {
				switch(v->esc_attrib[tmp]) {
				case 0:		/* Reset all attribs */
					v->fc = VTEMU_DEFAULT_FC;
					v->bc = VTEMU_DEFAULT_BC;
					break;
				case 1:		/* Bold or Bright */
				case 2:		/* Dim */
				case 4:		/* Underscore */
				case 5:		/* Blink */
					break;
				case 7:		/* Reverse */
					tmp = v->fc;
					v->fc = v->bc;
					v->bc = tmp;
					break;
				case 30:	/* Set FG color */
				case 31:
				case 32:
				case 33:
				case 34:
				case 35:
				case 36:
				case 37:
					v->fc = (v->esc_attrib[tmp] - 30);
					break;
				case 40:	/* Set BG color */
				case 41:
				case 42:
				case 43:
				case 44:
				case 45:
				case 46:
				case 47:
					v->bc = (v->esc_attrib[tmp] - 40);
					break;
				case 49:
					v->bc = VTEMU_DEFAULT_BC;
					break;
				};
			}
			v->esc_cmd_active = FALSE;
			break;
		case 'c':		/* Device status */
		case 'n':
			v->esc_cmd_active = FALSE;
			break;
		case 's':		/* Save Cursor Position */
			v->saved_x = v->x;
			v->saved_y = v->y;
			v->esc_cmd_active = FALSE;
			break;
		case 'u':		/* Restore Cursor Position */
			v->x = v->saved_x;
			v->y = v->saved_y;
			v->esc_cmd_active = FALSE;
			break;
		case 'H':		/* Cursor Home */
		case 'f':		/* Force Cursor Position */
			if(v->esc_attrib_count == 0) {
				v->x = 0;
				v->y = v->start_y;
			} else {
				v->x = v->esc_attrib[0];
				v->y = v->esc_attrib[1];
			}
			v->esc_cmd_active = FALSE;
			break;
		case 'J':		/* Clear screen */
			/* FIXME: */
			vtemu_cursor_clear_down(v);
			v->esc_cmd_active = FALSE;
			break;
		default:
			goto unhandled;
		}
		break;
	default:
		goto unhandled;
	};

	return VMM_OK;

unhandled:
	v->esc_cmd_active = FALSE;
	return VMM_OK;
}

static u32 vtemu_write (struct vmm_chardev *cdev,
			 u8 *src, u32 offset, u32 len,
			 bool sleep)
{
	int rc;
	u32 i;
	struct vtemu *v = cdev->priv;

	if (!v) {
		return 0;
	}

	for (i = 0; i < len; i++) {
		if (v->esc_cmd_active) {
			rc = vtemu_putesc(v, src[i]);
		} else if (src[i] == '\e') {
			rc = vtemu_startesc(v);
		} else {
			rc = vtemu_putchar(v, src[i]);
		}
		if (rc) {
			break;
		}
	}

	return i;
}

#define VTEMU_KEYFLAG_LEFTCTRL		0x00000001
#define VTEMU_KEYFLAG_RIGHTCTRL		0x00000002
#define VTEMU_KEYFLAG_LEFTALT		0x00000004
#define VTEMU_KEYFLAG_RIGHTALT		0x00000008
#define VTEMU_KEYFLAG_LEFTSHIFT		0x00000010
#define VTEMU_KEYFLAG_RIGHTSHIFT	0x00000020
#define VTEMU_KEYFLAG_CAPSLOCK		0x00000040
#define VTEMU_KEYFLAG_NUMLOCK		0x00000080
#define VTEMU_KEYFLAG_SCROLLLOCK	0x00000100

#define VTEMU_KEYFLAG_LOCKS		(VTEMU_KEYFLAG_CAPSLOCK | \
					 VTEMU_KEYFLAG_NUMLOCK | \
					 VTEMU_KEYFLAG_SCROLLLOCK)

u32 vtemu_key2flags(unsigned int code)
{
	u32 ret = 0;

	switch (code) {
	case KEY_LEFTCTRL:
		ret = VTEMU_KEYFLAG_LEFTCTRL;
		break;
	case KEY_RIGHTCTRL:
		ret = VTEMU_KEYFLAG_RIGHTCTRL;
		break;
	case KEY_LEFTSHIFT:
		ret = VTEMU_KEYFLAG_LEFTSHIFT;
		break;
	case KEY_RIGHTSHIFT:
		ret = VTEMU_KEYFLAG_RIGHTSHIFT;
		break;
	case KEY_LEFTALT:
		ret = VTEMU_KEYFLAG_LEFTALT;
		break;
	case KEY_RIGHTALT:
		ret = VTEMU_KEYFLAG_RIGHTALT;
		break;
	case KEY_CAPSLOCK:
		ret = VTEMU_KEYFLAG_CAPSLOCK;
		break;
	case KEY_NUMLOCK:
		ret = VTEMU_KEYFLAG_NUMLOCK;
		break;
	case KEY_SCROLLLOCK:
		ret = VTEMU_KEYFLAG_SCROLLLOCK;
		break;
	}

	return ret;
}

/* FIXME: */
int vtemu_key2str(unsigned int code, u32 flags, char *out)
{
	bool uc = FALSE;

	if (flags & (VTEMU_KEYFLAG_LEFTSHIFT | VTEMU_KEYFLAG_RIGHTSHIFT)) {
		uc = (uc) ? FALSE : TRUE;
	}
	if (flags & VTEMU_KEYFLAG_CAPSLOCK) {
		uc = (uc) ? FALSE : TRUE;
	}

	switch (code) {
	case KEY_ESC:
		out[0] = '\e'; out[1] = '\0';
		break;
	case KEY_1:
		out[0] = (uc) ? '!' : '1'; out[1] = '\0';
		break;
	case KEY_2:
		out[0] = (uc) ? '@' : '2'; out[1] = '\0';
		break;
	case KEY_3:
		out[0] = (uc) ? '#' : '3'; out[1] = '\0';
		break;
	case KEY_4:
		out[0] = (uc) ? '$' : '4'; out[1] = '\0';
		break;
	case KEY_5:
		out[0] = (uc) ? '%' : '5'; out[1] = '\0';
		break;
	case KEY_6:
		out[0] = (uc) ? '^' : '6'; out[1] = '\0';
		break;
	case KEY_7:
		out[0] = (uc) ? '&' : '7'; out[1] = '\0';
		break;
	case KEY_8:
		out[0] = (uc) ? '*' : '8'; out[1] = '\0';
		break;
	case KEY_9:
		out[0] = (uc) ? '(' : '9'; out[1] = '\0';
		break;
	case KEY_0:
		out[0] = (uc) ? ')' : '0'; out[1] = '\0';
		break;
	case KEY_MINUS:
		out[0] = (uc) ? '_' : '-'; out[1] = '\0';
		break;
	case KEY_EQUAL:
		out[0] = (uc) ? '+' : '='; out[1] = '\0';
		break;
	case KEY_BACKSPACE:
		out[0] = 127; out[1] = '\0';
		break;
	case KEY_TAB:
		out[0] = '\t'; out[1] = '\0';
		break;
	case KEY_Q:
		out[0] = (uc) ? 'Q' : 'q'; out[1] = '\0';
		break;
	case KEY_W:
		out[0] = (uc) ? 'W' : 'w'; out[1] = '\0';
		break;
	case KEY_E:
		out[0] = (uc) ? 'E' : 'e'; out[1] = '\0';
		break;
	case KEY_R:
		out[0] = (uc) ? 'R' : 'r'; out[1] = '\0';
		break;
	case KEY_T:
		out[0] = (uc) ? 'T' : 't'; out[1] = '\0';
		break;
	case KEY_Y:
		out[0] = (uc) ? 'Y' : 'y'; out[1] = '\0';
		break;
	case KEY_U:
		out[0] = (uc) ? 'U' : 'u'; out[1] = '\0';
		break;
	case KEY_I:
		out[0] = (uc) ? 'I' : 'i'; out[1] = '\0';
		break;
	case KEY_O:
		out[0] = (uc) ? 'O' : 'o'; out[1] = '\0';
		break;
	case KEY_P:
		out[0] = (uc) ? 'P' : 'p'; out[1] = '\0';
		break;
	case KEY_LEFTBRACE:
		out[0] = (uc) ? '{' : '['; out[1] = '\0';
		break;
	case KEY_RIGHTBRACE:
		out[0] = (uc) ? '}' : ']'; out[1] = '\0';
		break;
	case KEY_ENTER:
		out[0] = '\n'; out[1] = '\0';
		break;
	case KEY_A:
		out[0] = (uc) ? 'A' : 'a'; out[1] = '\0';
		break;
	case KEY_S:
		out[0] = (uc) ? 'S' : 's'; out[1] = '\0';
		break;
	case KEY_D:
		out[0] = (uc) ? 'D' : 'd'; out[1] = '\0';
		break;
	case KEY_F:
		out[0] = (uc) ? 'F' : 'f'; out[1] = '\0';
		break;
	case KEY_G:
		out[0] = (uc) ? 'G' : 'g'; out[1] = '\0';
		break;
	case KEY_H:
		out[0] = (uc) ? 'H' : 'h'; out[1] = '\0';
		break;
	case KEY_J:
		out[0] = (uc) ? 'J' : 'j'; out[1] = '\0';
		break;
	case KEY_K:
		out[0] = (uc) ? 'K' : 'k'; out[1] = '\0';
		break;
	case KEY_L:
		out[0] = (uc) ? 'L' :  'l'; out[1] = '\0';
		break;
	case KEY_SEMICOLON:
		out[0] = (uc) ? ':' : ';'; out[1] = '\0';
		break;
	case KEY_APOSTROPHE:
		out[0] = (uc) ? '\"' : '\''; out[1] = '\0';
		break;
	case KEY_GRAVE:
		out[0] = (uc) ? '~' : '`'; out[1] = '\0';
		break;
	case KEY_BACKSLASH:
		out[0] = (uc) ? '|' : '\\'; out[1] = '\0';
		break;
	case KEY_Z:
		out[0] = (uc) ? 'Z' : 'z'; out[1] = '\0';
		break;
	case KEY_X:
		out[0] = (uc) ? 'X' : 'x'; out[1] = '\0';
		break;
	case KEY_C:
		out[0] = (uc) ? 'C' : 'c'; out[1] = '\0';
		break;
	case KEY_V:
		out[0] = (uc) ? 'V' : 'v'; out[1] = '\0';
		break;
	case KEY_B:
		out[0] = (uc) ? 'B' : 'b'; out[1] = '\0';
		break;
	case KEY_N:
		out[0] = (uc) ? 'N' : 'n'; out[1] = '\0';
		break;
	case KEY_M:
		out[0] = (uc) ? 'M' : 'm'; out[1] = '\0';
		break;
	case KEY_COMMA:
		out[0] = (uc) ? '<' : ','; out[1] = '\0';
		break;
	case KEY_DOT:
		out[0] = (uc) ? '>' : '.'; out[1] = '\0';
		break;
	case KEY_SLASH:
		out[0] = (uc) ? '?' : '/'; out[1] = '\0';
		break;
	case KEY_KPASTERISK:
		out[0] = '*'; out[1] = '\0';
		break;
	case KEY_SPACE:
		out[0] = ' '; out[1] = '\0';
		break;
	case KEY_KP7:
		out[0] = '7'; out[1] = '\0';
		break;
	case KEY_KP8:
		out[0] = '8'; out[1] = '\0';
		break;
	case KEY_KP9:
		out[0] = '9'; out[1] = '\0';
		break;
	case KEY_KPMINUS:
		out[0] = '-'; out[1] = '\0';
		break;
	case KEY_KP4:
		out[0] = '4'; out[1] = '\0';
		break;
	case KEY_KP5:
		out[0] = '5'; out[1] = '\0';
		break;
	case KEY_KP6:
		out[0] = '6'; out[1] = '\0';
		break;
	case KEY_KPPLUS:
		out[0] = '+'; out[1] = '\0';
		break;
	case KEY_KP1:
		out[0] = '1'; out[1] = '\0';
		break;
	case KEY_KP2:
		out[0] = '2'; out[1] = '\0';
		break;
	case KEY_KP3:
		out[0] = '3'; out[1] = '\0';
		break;
	case KEY_KP0:
		out[0] = '0'; out[1] = '\0';
		break;
	case KEY_KPDOT:
		out[0] = '.'; out[1] = '\0';
		break;
	case KEY_KPENTER:
		out[0] = '\n'; out[1] = '\0';
		break;
	case KEY_KPSLASH:
		out[0] = '/'; out[1] = '\0';
		break;
	case KEY_HOME:
		out[0] = '\e'; out[1] = '['; out[2] = 'H'; out[3] = '\0';
		break;
	case KEY_UP:
		out[0] = '\e'; out[1] = '['; out[2] = 'A'; out[3] = '\0';
		break;
	case KEY_LEFT:
		out[0] = '\e'; out[1] = '['; out[2] = 'D'; out[3] = '\0';
		break;
	case KEY_RIGHT:
		out[0] = '\e'; out[1] = '['; out[2] = 'C'; out[3] = '\0';
		break;
	case KEY_END:
		out[0] = '\e'; out[1] = '['; out[2] = 'F'; out[3] = '\0';
		break;
	case KEY_DOWN:
		out[0] = '\e'; out[1] = '['; out[2] = 'B'; out[3] = '\0';
		break;
	case KEY_DELETE:
		out[0] = '\e'; out[1] = '['; out[2] = '3'; out[3] = '~'; out[4] = '\0';
		break;
	default:
		out[0] = '\0';
		break;
	}

	return VMM_OK;
}

int vtemu_key_event(struct vmm_input_handler *ihnd, 
		    struct vmm_input_dev *idev, 
		    unsigned int type, unsigned int code, int value)
{
	int rc, i;
	char str[16];
	u32 key_flags;
	irq_flags_t flags;
	struct vtemu *v = ihnd->priv;

	if (value) {
		/* Update input key flags */
		key_flags = vtemu_key2flags(code);
		if ((key_flags & VTEMU_KEYFLAG_LOCKS) &&
		    (v->in_key_flags & key_flags)) {
			v->in_key_flags &= ~key_flags;
		} else {
			v->in_key_flags |= key_flags;
		}

		/* Retrive input key string */
		rc = vtemu_key2str(code, v->in_key_flags, str);
		if (rc) {
			return VMM_OK;
		}

		/* Save input key string */
		i = 0;
		vmm_spin_lock_irqsave(&v->in_lock, flags);
		while(str[i] != '\0') {
			if ((v->in_tail == v->in_head) && 
			    (v->in_count == VTEMU_INBUF_SIZE)) {
				v->in_head++;
				if (v->in_head == VTEMU_INBUF_SIZE) {
					v->in_head = 0;
				}
				v->in_count--;
			}
			v->in_buf[v->in_tail] = str[i];
			v->in_tail++;
			if (v->in_tail == VTEMU_INBUF_SIZE) {
				v->in_tail = 0;
			}
			v->in_count++;
			i++;
		}
		vmm_spin_unlock_irqrestore(&v->in_lock, flags);

		/* Signal completion */
		vmm_completion_complete_all(&v->in_done);
	} else  {
		/* Update input key flags */
		key_flags = vtemu_key2flags(code);
		if (!(key_flags & VTEMU_KEYFLAG_LOCKS)) {
			v->in_key_flags &= ~key_flags;
		}
	}

	return VMM_OK;
}

static u32 vtemu_read(struct vmm_chardev *cdev,
			u8 *dest, u32 offset, u32 len,
			bool sleep)
{
	u32 i;
	irq_flags_t flags;
	struct vtemu *v = cdev->priv;

	if (!v) {
		return 0;
	}

	vmm_spin_lock_irqsave(&v->in_lock, flags);
	for (i = 0; i < len; i++) {
		if (!v->in_count) {
			if (sleep) {
				vmm_spin_unlock_irqrestore(&v->in_lock, flags);
				REINIT_COMPLETION(&v->in_done);
				vmm_completion_wait(&v->in_done);
				vmm_spin_lock_irqsave(&v->in_lock, flags);
			}
			break;
		}
		dest[i] = v->in_buf[v->in_head];
		v->in_head++;
		if (v->in_head == VTEMU_INBUF_SIZE) {
			v->in_head = 0;
		}
		v->in_count--;
	}
	vmm_spin_unlock_irqrestore(&v->in_lock, flags);

	return i;
}

struct vtemu *vtemu_create(const char *name, 
			   struct vmm_fb_info *info,
			   const char *font_name)
{
	u32 c;
	struct vtemu *v;

	/* Sanity check */
	if (!name || !info) {
		return NULL;
	}

	/* Allocate new video terminal */
	v = vmm_zalloc(sizeof(*v));
	if (!v) {
		return NULL;
	}

	/* Setup pseudo character device */
	strncpy(v->cdev.name, name, VMM_CHARDEV_NAME_SIZE);
	v->cdev.dev = NULL; 
	v->cdev.read = vtemu_read;
	v->cdev.write = vtemu_write;
	v->cdev.priv = v;
	if (vmm_chardev_register(&v->cdev)) {
		goto free_vtemu;
	}

	/* Setup input handler */
	v->hndl.name = v->cdev.name;
	v->hndl.evbit[0] |= BIT_MASK(EV_KEY);
	v->hndl.event = vtemu_key_event;
	v->hndl.priv = v;
	if (vmm_input_register_handler(&v->hndl)) {
		goto free_vtemu;
	}

	/* Connect input handler */
	if (vmm_input_connect_handler(&v->hndl)) {
		goto unreg_ihndl;
	}

	/* Open frame buffer*/
	v->info = info;
	if (vmm_fb_open(v->info)) {
		goto discon_ihndl;
	}

	/* Find video mode */
	v->mode = vmm_fb_find_best_mode(&v->info->var, &v->info->modelist);
	if (!v->mode) {
		goto release_fb;
	}

	/* Set video mode */
	if (v->info->fbops->fb_set_par(v->info)) {
		goto release_fb;
	}

	/* Find color map */
	if (v->info->fix.visual == FB_VISUAL_TRUECOLOR ||
	    v->info->fix.visual == FB_VISUAL_DIRECTCOLOR) {
		if (vmm_fb_alloc_cmap(&v->cmap, 8, 0)) {
			goto release_fb;
		}
		v->cmap.red[VTEMU_COLOR_BLACK] = 0x0000;
		v->cmap.green[VTEMU_COLOR_BLACK] = 0x0000;
		v->cmap.blue[VTEMU_COLOR_BLACK] = 0x0000;
		v->cmap.red[VTEMU_COLOR_RED] = 0xffff;
		v->cmap.green[VTEMU_COLOR_RED] = 0x0000;
		v->cmap.blue[VTEMU_COLOR_RED] = 0x0000;
		v->cmap.red[VTEMU_COLOR_GREEN] = 0x0000;
		v->cmap.green[VTEMU_COLOR_GREEN] = 0xffff;
		v->cmap.blue[VTEMU_COLOR_GREEN] = 0x0000;
		v->cmap.red[VTEMU_COLOR_YELLOW] = 0xffff;
		v->cmap.green[VTEMU_COLOR_YELLOW] = 0xffff;
		v->cmap.blue[VTEMU_COLOR_YELLOW] = 0x0000;
		v->cmap.red[VTEMU_COLOR_BLUE] = 0x0000;
		v->cmap.green[VTEMU_COLOR_BLUE] = 0x0000;
		v->cmap.blue[VTEMU_COLOR_BLUE] = 0xffff;
		v->cmap.red[VTEMU_COLOR_MAGENTA] = 0xffff;
		v->cmap.green[VTEMU_COLOR_MAGENTA] = 0x0000;
		v->cmap.blue[VTEMU_COLOR_MAGENTA] = 0xffff;
		v->cmap.red[VTEMU_COLOR_CYAN] = 0x0000;
		v->cmap.green[VTEMU_COLOR_CYAN] = 0xffff;
		v->cmap.blue[VTEMU_COLOR_CYAN] = 0xffff;
		v->cmap.red[VTEMU_COLOR_WHITE] = 0xffff;
		v->cmap.green[VTEMU_COLOR_WHITE] = 0xffff;
		v->cmap.blue[VTEMU_COLOR_WHITE] = 0xffff;
		v->fc = VTEMU_DEFAULT_FC;
		v->bc = VTEMU_DEFAULT_BC;
	} else {
		/* Don't require color map for 32-bit colors */
		v->fc = 0xFFFFFFFF; /* White foreground color (default) */
		v->bc = 0x00000000; /* Black background color (default) */
	}

	/* Set color map (if required) */
	if (v->info->fix.visual == FB_VISUAL_TRUECOLOR ||
	    v->info->fix.visual == FB_VISUAL_DIRECTCOLOR) {
		if (vmm_fb_set_cmap(&v->cmap, v->info)) {
			goto dealloc_cmap;
		}
	}

	/* Find monochrome fonts */
	if (font_name) {
		v->font = vtemu_find_font(font_name);
	} else {
		v->font = vtemu_get_default_font(v->info->var.xres_virtual,
						 v->info->var.yres_virtual,
						 8, 8);
	}
	if (!v->font) {
		goto dealloc_cmap;
	}
	if (v->font->width <= 8) {
		v->font_img_sz = 1;
	} else {
		v->font_img_sz = v->font->width / 8;
		if (v->font->width % 8) {
			v->font_img_sz++;
		}
	}
	v->font_img_sz *= v->font->height;

	/* Setup screen parameters */
	v->w = udiv32(v->info->var.xres_virtual, v->font->width);
	v->h = udiv32(v->info->var.yres_virtual, v->font->height);
	v->x = 0;
	v->y = 0;
	v->start_y = 0;
	v->freeze = FALSE;

	/* Setup screen data */
	v->cell_head = 0;
	v->cell_tail = 0;
	v->cell_count = 0;
	v->cell_len = v->w * v->h;
	v->cell = vmm_zalloc(v->cell_len * sizeof(struct vtemu_cell));
	if (!v->cell) {
		goto dealloc_cmap;
	}
	for (c = 0; c < v->cell_len; c++) {
		v->cell[c].x = 0xFFFFFFFF;
		v->cell[c].y = 0xFFFFFFFF;
	}
	v->cursor_bkp_size = v->font->height * v->info->var.bits_per_pixel;
	v->cursor_bkp_size = v->cursor_bkp_size / 8;
	v->cursor_bkp = vmm_zalloc(v->cursor_bkp_size);
	if (!v->cursor_bkp) {
		goto free_cells;
	}
	v->esc_cmd_active = FALSE;
	v->esc_cmd_count = 0;
	v->esc_attrib_count = 0;
	v->esc_attrib[0] = 0;

	/* Setup input data */
	v->in_head = 0;
	v->in_tail = 0;
	v->in_count = 0;
	v->in_key_flags = 0;
	INIT_SPIN_LOCK(&v->in_lock);
	INIT_COMPLETION(&v->in_done);

	/* Draw cursor */
	vtemu_cursor_draw(v);

	return v;

free_cells:
	vmm_free(v->cell);
dealloc_cmap:
	vmm_fb_dealloc_cmap(&v->cmap);
release_fb:
	vmm_fb_release(v->info);
discon_ihndl:
	vmm_input_disconnect_handler(&v->hndl);
unreg_ihndl:
	vmm_input_unregister_handler(&v->hndl);
free_vtemu:
	vmm_free(v);
	return NULL;
}

int vtemu_destroy(struct vtemu *v)
{
	int rc, rc1, rc2, rc3;

	if (!v) {
		return VMM_EFAIL;
	}

	vmm_free(v->cursor_bkp);
	vmm_free(v->cell);
	vmm_fb_dealloc_cmap(&v->cmap);
	rc  = vmm_fb_release(v->info);
	rc1 = vmm_chardev_unregister(&v->cdev);
	rc2 = vmm_input_disconnect_handler(&v->hndl);
	rc3 = vmm_input_unregister_handler(&v->hndl);
	vmm_free(v);

	if (rc) {
		return rc;
	} else if (rc1) {
		return rc1;
	} else if (rc2) {
		return rc2;
	} else if (rc3) {
		return rc3;
	}

	return VMM_OK;
}

