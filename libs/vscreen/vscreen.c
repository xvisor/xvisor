/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file vscreen.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation frame buffer based virtual screen capturing
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <vmm_modules.h>
#include <vmm_spinlocks.h>
#include <vmm_completion.h>
#include <arch_atomic.h>
#include <vio/vmm_keymaps.h>
#include <libs/list.h>
#include <libs/mathlib.h>
#include <libs/vscreen.h>

#define MODULE_DESC			"vscreen library"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VSCREEN_IPRIORITY)
#define	MODULE_INIT			NULL
#define	MODULE_EXIT			NULL

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)			vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

typedef enum {
	VSCREEN_COLOR_BLACK,
	VSCREEN_COLOR_RED,
	VSCREEN_COLOR_GREEN,
	VSCREEN_COLOR_YELLOW,
	VSCREEN_COLOR_BLUE,
	VSCREEN_COLOR_MAGENTA,
	VSCREEN_COLOR_CYAN,
	VSCREEN_COLOR_WHITE,
} vscreen_color;
#define VSCREEN_DEFAULT_FC	VSCREEN_COLOR_WHITE
#define VSCREEN_DEFAULT_BC	VSCREEN_COLOR_BLACK

typedef enum {
	VSCREEN_WORK_EXIT,
	VSCREEN_MAX_WORK,
} vscreen_work_type;

struct vscreen_work {
	struct dlist head;
	u32 type;
};

struct vscreen_context {
	/* Parameters */
	bool is_hard;
	u32 refresh_rate;
	u32 esc_key_code[3];
	struct fb_info *info;
	struct vmm_vdisplay *vdis;
	struct vmm_vkeyboard *vkbd;
	struct vmm_vmouse *vmou;
	/* Common State */
	char name[32];
	bool freeze;
	u32 esc_key_state;
	struct input_handler hndl;
	const struct fb_videomode *mode;
	struct fb_var_screeninfo var;
	struct fb_cmap cmap;
	u32 fc, bc;
	unsigned long smem_start;
	u32 smem_len;
	/* Input event tracking */
	bool key_event;
	u32 key_code;
	int key_value;
	bool mouse_event;
	int mouse_btn;
	int mouse_dx;
	int mouse_dy;
	int mouse_dz;
	/* Hard bind state */
	bool hard_vdis;
	const struct fb_videomode *hard_mode;
	struct fb_var_screeninfo hard_var;
	physical_addr_t hard_smem_start;
	u32 hard_smem_len;
	/* Work queue */
	u64 work_timeout;
	vmm_spinlock_t work_list_lock;
	struct dlist work_list;
	struct vmm_completion work_avail;
	/* Notifier clients */
	struct vmm_notifier_block vdis_client;
	struct vmm_notifier_block vinp_client;
};

static void vscreen_enqueue_work(struct vscreen_context *cntx,
				 u32 type)
{
	irq_flags_t flags;
	struct vscreen_work *work;

	if (cntx->freeze) {
		vmm_printf("%s: Cannot queue work when freezed\n",
			   __func__);
		return;
	}

	if (VSCREEN_MAX_WORK <= type) {
		vmm_printf("%s: Invalid work type %d\n",
			   __func__, type);
		return;
	}

	work = vmm_zalloc(sizeof(struct vscreen_work));
	if (!work) {
		vmm_printf("%s: Failed to alloc work\n",
			   __func__);
		return;
	}

	INIT_LIST_HEAD(&work->head);
	work->type = type;

	vmm_spin_lock_irqsave(&cntx->work_list_lock, flags);
	list_add_tail(&work->head, &cntx->work_list);
	vmm_spin_unlock_irqrestore(&cntx->work_list_lock, flags);

	vmm_completion_complete(&cntx->work_avail);
}

static void vscreen_blank_display(struct vscreen_context *cntx)
{
	struct fb_fillrect rect;

	/* Blank frame buffer */
	rect.dx = 0;
	rect.dy = 0;
	rect.width = cntx->info->var.xres_virtual;
	rect.height = cntx->info->var.yres_virtual;
	rect.color = cntx->bc;
	rect.rop = ROP_COPY;

	cntx->info->fbops->fb_fillrect(cntx->info, &rect);
}

static int vscreen_code2vkey(u32 code)
{
	int ret;

	switch (code) {
	case KEY_LEFTSHIFT:
		ret = VMM_VKEY_SHIFT;
		break;
	case KEY_RIGHTSHIFT:
		ret = VMM_VKEY_SHIFT_R;
		break;
	case KEY_LEFTALT:
		ret = VMM_VKEY_ALT;
		break;
	case KEY_RIGHTALT:
		ret = VMM_VKEY_ALT_R;
		break;
	case KEY_LEFTCTRL:
		ret = VMM_VKEY_CTRL;
		break;
	case KEY_RIGHTCTRL:
		ret = VMM_VKEY_CTRL_R;
		break;
	case KEY_MENU:
		ret = VMM_VKEY_MENU;
		break;
	case KEY_ESC:
		ret = VMM_VKEY_ESC;
		break;
	case KEY_1:
		ret = VMM_VKEY_1;
		break;
	case KEY_2:
		ret = VMM_VKEY_2;
		break;
	case KEY_3:
		ret = VMM_VKEY_3;
		break;
	case KEY_4:
		ret = VMM_VKEY_4;
		break;
	case KEY_5:
		ret = VMM_VKEY_5;
		break;
	case KEY_6:
		ret = VMM_VKEY_6;
		break;
	case KEY_7:
		ret = VMM_VKEY_7;
		break;
	case KEY_8:
		ret = VMM_VKEY_8;
		break;
	case KEY_9:
		ret = VMM_VKEY_9;
		break;
	case KEY_0:
		ret = VMM_VKEY_0;
		break;
	case KEY_MINUS:
		ret = VMM_VKEY_MINUS;
		break;
	case KEY_EQUAL:
		ret = VMM_VKEY_EQUAL;
		break;
	case KEY_BACKSPACE:
		ret = VMM_VKEY_BACKSPACE;
		break;
	case KEY_TAB:
		ret = VMM_VKEY_TAB;
		break;
	case KEY_Q:
		ret = VMM_VKEY_Q;
		break;
	case KEY_W:
		ret = VMM_VKEY_W;
		break;
	case KEY_E:
		ret = VMM_VKEY_E;
		break;
	case KEY_R:
		ret = VMM_VKEY_R;
		break;
	case KEY_T:
		ret = VMM_VKEY_T;
		break;
	case KEY_Y:
		ret = VMM_VKEY_Y;
		break;
	case KEY_U:
		ret = VMM_VKEY_U;
		break;
	case KEY_I:
		ret = VMM_VKEY_I;
		break;
	case KEY_O:
		ret = VMM_VKEY_O;
		break;
	case KEY_P:
		ret = VMM_VKEY_P;
		break;
	case KEY_LEFTBRACE:
		ret = VMM_VKEY_BRACKET_LEFT;
		break;
	case KEY_RIGHTBRACE:
		ret = VMM_VKEY_BRACKET_RIGHT;
		break;
	case KEY_ENTER:
		ret = VMM_VKEY_RET;
		break;
	case KEY_A:
		ret = VMM_VKEY_A;
		break;
	case KEY_S:
		ret = VMM_VKEY_S;
		break;
	case KEY_D:
		ret = VMM_VKEY_D;
		break;
	case KEY_F:
		ret = VMM_VKEY_F;
		break;
	case KEY_G:
		ret = VMM_VKEY_G;
		break;
	case KEY_H:
		ret = VMM_VKEY_H;
		break;
	case KEY_J:
		ret = VMM_VKEY_J;
		break;
	case KEY_K:
		ret = VMM_VKEY_K;
		break;
	case KEY_L:
		ret = VMM_VKEY_L;
		break;
	case KEY_SEMICOLON:
		ret = VMM_VKEY_SEMICOLON;
		break;
	case KEY_APOSTROPHE:
		ret = VMM_VKEY_APOSTROPHE;
		break;
	case KEY_GRAVE:
		ret = VMM_VKEY_GRAVE_ACCENT;
		break;
	case KEY_BACKSLASH:
		ret = VMM_VKEY_BACKSLASH;
		break;
	case KEY_Z:
		ret = VMM_VKEY_Z;
		break;
	case KEY_X:
		ret = VMM_VKEY_X;
		break;
	case KEY_C:
		ret = VMM_VKEY_C;
		break;
	case KEY_V:
		ret = VMM_VKEY_V;
		break;
	case KEY_B:
		ret = VMM_VKEY_B;
		break;
	case KEY_N:
		ret = VMM_VKEY_N;
		break;
	case KEY_M:
		ret = VMM_VKEY_M;
		break;
	case KEY_COMMA:
		ret = VMM_VKEY_COMMA;
		break;
	case KEY_DOT:
		ret = VMM_VKEY_DOT;
		break;
	case KEY_SLASH:
		ret = VMM_VKEY_SLASH;
		break;
	case KEY_SPACE:
		ret = VMM_VKEY_SPC;
		break;
	case KEY_CAPSLOCK:
		ret = VMM_VKEY_CAPS_LOCK;
		break;
	case KEY_FN_F1:
		ret = VMM_VKEY_F1;
		break;
	case KEY_FN_F2:
		ret = VMM_VKEY_F2;
		break;
	case KEY_FN_F3:
		ret = VMM_VKEY_F3;
		break;
	case KEY_FN_F4:
		ret = VMM_VKEY_F4;
		break;
	case KEY_FN_F5:
		ret = VMM_VKEY_F5;
		break;
	case KEY_FN_F6:
		ret = VMM_VKEY_F6;
		break;
	case KEY_FN_F7:
		ret = VMM_VKEY_F7;
		break;
	case KEY_FN_F8:
		ret = VMM_VKEY_F8;
		break;
	case KEY_FN_F9:
		ret = VMM_VKEY_F9;
		break;
	case KEY_FN_F10:
		ret = VMM_VKEY_F10;
		break;
	case KEY_NUMLOCK:
		ret = VMM_VKEY_NUM_LOCK;
		break;
	case KEY_SCROLLLOCK:
		ret = VMM_VKEY_SCROLL_LOCK;
		break;
	case KEY_KPSLASH:
		ret = VMM_VKEY_KP_DIVIDE;
		break;
	case KEY_KPASTERISK:
		ret = VMM_VKEY_KP_MULTIPLY;
		break;
	case KEY_KPMINUS:
		ret = VMM_VKEY_KP_SUBTRACT;
		break;
	case KEY_KPPLUS:
		ret = VMM_VKEY_KP_ADD;
		break;
	case KEY_KPENTER:
		ret = VMM_VKEY_KP_ENTER;
		break;
	case KEY_KPDOT:
		ret = VMM_VKEY_KP_DECIMAL;
		break;
	case KEY_SYSRQ:
		ret = VMM_VKEY_SYSRQ;
		break;
	case KEY_KP0:
		ret = VMM_VKEY_KP_0;
		break;
	case KEY_KP1:
		ret = VMM_VKEY_KP_1;
		break;
	case KEY_KP2:
		ret = VMM_VKEY_KP_2;
		break;
	case KEY_KP3:
		ret = VMM_VKEY_KP_3;
		break;
	case KEY_KP4:
		ret = VMM_VKEY_KP_4;
		break;
	case KEY_KP5:
		ret = VMM_VKEY_KP_5;
		break;
	case KEY_KP6:
		ret = VMM_VKEY_KP_6;
		break;
	case KEY_KP7:
		ret = VMM_VKEY_KP_7;
		break;
	case KEY_KP8:
		ret = VMM_VKEY_KP_8;
		break;
	case KEY_KP9:
		ret = VMM_VKEY_KP_9;
		break;
	case KEY_FN_F11:
		ret = VMM_VKEY_F11;
		break;
	case KEY_FN_F12:
		ret = VMM_VKEY_F12;
		break;
	case KEY_PRINT:
		ret = VMM_VKEY_PRINT;
		break;
	case KEY_HOME:
		ret = VMM_VKEY_HOME;
		break;
	case KEY_PAGEUP:
		ret = VMM_VKEY_PGUP;
		break;
	case KEY_PAGEDOWN:
		ret = VMM_VKEY_PGDN;
		break;
	case KEY_END:
		ret = VMM_VKEY_END;
		break;
	case KEY_LEFT:
		ret = VMM_VKEY_LEFT;
		break;
	case KEY_UP:
		ret = VMM_VKEY_UP;
		break;
	case KEY_DOWN:
		ret = VMM_VKEY_DOWN;
		break;
	case KEY_RIGHT:
		ret = VMM_VKEY_RIGHT;
		break;
	case KEY_INSERT:
		ret = VMM_VKEY_INSERT;
		break;
	case KEY_DELETE:
		ret = VMM_VKEY_DELETE;
		break;
	case KEY_STOP:
		ret = VMM_VKEY_STOP;
		break;
	case KEY_AGAIN:
		ret = VMM_VKEY_AGAIN;
		break;
	case KEY_PROPS:
		ret = VMM_VKEY_PROPS;
		break;
	case KEY_UNDO:
		ret = VMM_VKEY_UNDO;
		break;
	case KEY_FRONT:
		ret = VMM_VKEY_FRONT;
		break;
	case KEY_COPY:
		ret = VMM_VKEY_COPY;
		break;
	case KEY_OPEN:
		ret = VMM_VKEY_OPEN;
		break;
	case KEY_PASTE:
		ret = VMM_VKEY_PASTE;
		break;
	case KEY_FIND:
		ret = VMM_VKEY_FIND;
		break;
	case KEY_CUT:
		ret = VMM_VKEY_CUT;
		break;
	case KEY_LINEFEED:
		ret = VMM_VKEY_LF;
		break;
	case KEY_HELP:
		ret = VMM_VKEY_HELP;
		break;
	case KEY_LEFTMETA:
		ret = VMM_VKEY_META_L;
		break;
	case KEY_RIGHTMETA:
		ret = VMM_VKEY_META_R;
		break;
	case KEY_COMPOSE:
		ret = VMM_VKEY_COMPOSE;
		break;
	default:
		ret = VMM_VKEY_MAX;
	};

	return ret;
}

static void vscreen_keyboard_event(struct vscreen_context *cntx,
				   u32 code, int value)
{
	int i, vkey, vkeycode;

	/* Check for escape keys */
	for (i = 0; i < array_size(cntx->esc_key_code); i++) {
		if (code != cntx->esc_key_code[i]) {
			continue;
		}
		switch (value) {
		case 0: /* key-down */
			cntx->esc_key_state |= (1 << i);
			break;
		case 1: /* key-up */
			cntx->esc_key_state &= ~(1 << i);
			break;
		case 2: /* auto-repeat */
		default:
			break;
		};
	}
	if (cntx->esc_key_state == 
			((1 << array_size(cntx->esc_key_code)) - 1)) {
		/* All escape keys pressed hence signal exit */
		vscreen_enqueue_work(cntx, VSCREEN_WORK_EXIT);
	}

	/* If no virtual keyboard then do nothing */
	if (!cntx->vkbd) {
		return;
	}

	DPRINTF("%s: code=%d value=%d\n",
		__func__, code, value);

	/* Convert input code to virtual key */
	vkey = vscreen_code2vkey(code);
	if (VMM_VKEY_MAX <= vkey) {
		return;
	}

	/* Convert virtual key to virtual key code */
	vkeycode = vmm_vkey2keycode(vkey);

	DPRINTF("%s: vkey=%d vkeycode=%d\n",
		__func__, vkey, vkeycode);

	/* Inject virtual keyboard event */
	if (value) {
		if (vkeycode & SCANCODE_GREY) {
			vmm_vkeyboard_event(cntx->vkbd, SCANCODE_EMUL0);
		}
		vmm_vkeyboard_event(cntx->vkbd,
					vkeycode & SCANCODE_KEYCODEMASK);
	} else {
		if (vkeycode & SCANCODE_GREY) {
			vmm_vkeyboard_event(cntx->vkbd, SCANCODE_EMUL0);
		}
		vmm_vkeyboard_event(cntx->vkbd, vkeycode | SCANCODE_UP);
	}
}

static void vscreen_mouse_event(struct vscreen_context *cntx,
				int btn, int dx, int dy, int dz)
{
	/* If no virtual mouse then do nothing */
	if (!cntx->vmou) {
		return;
	}

	DPRINTF("%s: btn=%d dx=%d dy=%d dz=%d\n",
		__func__, btn, dx, dy, dz);

	/* Inject virtual mouse event */
	vmm_vmouse_event(cntx->vmou, dx, dy, dz, btn);
}

static int vscreen_event(struct input_handler *ihnd,
			 struct input_dev *idev,
			 unsigned int type, unsigned int code, int value)
{
	struct vscreen_context *cntx = ihnd->priv;

	/* Do nothing if freezed */
	if (cntx->freeze) {
		goto done;
	}

	DPRINTF("%s: type=%d code=%d value=%d\n",
		__func__, type, code, value);

	/* Process SYN, KEY and REL events */
	switch (type) {
	case EV_SYN:
		/* Process keyboard event */
		if (cntx->key_event) {
			vscreen_keyboard_event(cntx,
					cntx->key_code,
					cntx->key_value);
		}
		/* Process mouse event */
		if (cntx->mouse_event) {
			vscreen_mouse_event(cntx,
					cntx->mouse_btn,
					cntx->mouse_dx,
					cntx->mouse_dy,
					cntx->mouse_dz);
		}
		/* Reset event tracking state */
		cntx->key_event = FALSE;
		cntx->key_code = 0;
		cntx->key_value = 0;
		cntx->mouse_event = FALSE;
		cntx->mouse_dx = 0;
		cntx->mouse_dy = 0;
		cntx->mouse_dz = 0;
		break;
	case EV_KEY:
		switch (code) {
		case BTN_LEFT:
			cntx->mouse_event = TRUE;
			if (value == 0) { /* button-release */
				cntx->mouse_btn &= ~VMM_MOUSE_LBUTTON;
			} else if (value == 1) { /* button-press */
				cntx->mouse_btn |= VMM_MOUSE_LBUTTON;
			}
			break;
		case BTN_RIGHT:
			cntx->mouse_event = TRUE;
			if (value == 0) { /* button-release */
				cntx->mouse_btn &= ~VMM_MOUSE_RBUTTON;
			} else if (value == 1) { /* button-press */
				cntx->mouse_btn |= VMM_MOUSE_RBUTTON;
			}
			break;
		case BTN_MIDDLE:
			cntx->mouse_event = TRUE;
			if (value == 0) { /* button-release */
				cntx->mouse_btn &= ~VMM_MOUSE_MBUTTON;
			} else if (value == 1) { /* button-press */
				cntx->mouse_btn |= VMM_MOUSE_MBUTTON;
			}
			break;
		default:
			if (value != 2) { /* ignore auto-repeat events */
				cntx->key_event = TRUE;
				cntx->key_code = code;
				cntx->key_value = value;
			}
			break;
		};
		break;
	case EV_REL:
		switch (code) {
		case REL_X:
			cntx->mouse_event = TRUE;
			cntx->mouse_dx = value;
			break;
		case REL_Y:
			cntx->mouse_event = TRUE;
			cntx->mouse_dy = value;
			break;
		case REL_Z:
			cntx->mouse_event = TRUE;
			cntx->mouse_dz = value;
			break;
		default:
			break;
		};
		break;
	default:
		break;
	};

done:
	return VMM_OK;
}

static int vscreen_soft_refresh(struct vscreen_context *cntx)
{
	/* Do nothing if freezed */
	if (cntx->freeze) {
		return VMM_OK;
	}

	/* Do nothing if vdisplay is NULL */
	if (!cntx->vdis) {
		return VMM_OK;
	}

	/* TODO: work in progress */
	vmm_printf("%s: not available\n", __func__);

	return VMM_EFAIL;
}

static void vscreen_hard_switch_back(struct vscreen_context *cntx)
{
	/* Do nothing for soft bind */
	if (!cntx->is_hard) {
		return;
	}

	/* Do nothing if hard bind is off */
	if (!cntx->hard_vdis) {
		return;
	}

	/* Switch back to original smem settings */
	fb_set_smem(cntx->info, cntx->smem_start, cntx->smem_len);

	/* Switch back to original variable screen info */
	fb_set_var(cntx->info, &cntx->var);

	/* Mark hard bind as off */
	cntx->hard_vdis = FALSE;
}

static int vscreen_hard_refresh(struct vscreen_context *cntx)
{
	int rc;
	u32 rows, cols;
	physical_addr_t pa;
	struct vmm_pixelformat pf;

	/* Do nothing if freezed */
	if (cntx->freeze) {
		return VMM_OK;
	}

	/* Do nothing if vdisplay is NULL */
	if (!cntx->vdis) {
		return VMM_OK;
	}

	/* Try to get current pixeldata of virtual display */
	rc = vmm_vdisplay_get_pixeldata(cntx->vdis,
					&pf, &rows, &cols, &pa);
	if (rc) {
		vscreen_hard_switch_back(cntx);
		return VMM_OK;
	}

	/* If we are already using appropriate settings then do nothing */
	if (cntx->hard_vdis && 
	    (cntx->info->var.xres_virtual == cols) &&
	    (cntx->info->var.yres_virtual == rows) &&
	    (cntx->info->var.bits_per_pixel == pf.bits_per_pixel) &&
	    (cntx->hard_smem_start == pa)) {
		return VMM_OK;
	}

	/* Save hard sem start and length */
	cntx->hard_smem_start = pa;
	cntx->hard_smem_len = (rows * cols) * pf.bytes_per_pixel;

	/* Find best matching mode */
	memcpy(&cntx->hard_var, &cntx->var, sizeof(cntx->hard_var));
	cntx->hard_var.xres = cols;
	cntx->hard_var.yres = rows;
	cntx->hard_mode = fb_find_best_mode(&cntx->hard_var,
					    &cntx->info->modelist);
	if (!cntx->hard_mode ||
	    (cntx->hard_mode->xres != cols) ||
	    (cntx->hard_mode->yres != rows)) {
		rc = VMM_ENOTAVAIL;
		vmm_printf("%s: fb_find_best_mode() failed\n", __func__);
		goto hard_bind_error;
	}

	/* Setup variable screen info matching virtual display */
	memset(&cntx->hard_var, 0, sizeof(cntx->hard_var));
	fb_videomode_to_var(&cntx->hard_var, cntx->hard_mode);
	cntx->hard_var.bits_per_pixel = pf.bits_per_pixel;
	cntx->hard_var.activate	= FB_ACTIVATE_NOW;

	/* Check and update hard variable screen info */
	rc = fb_check_var(cntx->info, &cntx->hard_var);
	if (rc) {
		vmm_printf("%s: fb_check_var() failed error %d\n",
			   __func__, rc);
		goto hard_bind_error;
	}

	/* Set hard variable screen info as current */
	rc = fb_set_var(cntx->info, &cntx->hard_var);
	if (rc) {
		vmm_printf("%s: fb_set_var() failed error %d\n",
			   __func__, rc);
		goto hard_bind_error;
	}

	/* Set hard sem start and length */
	rc = fb_set_smem(cntx->info,
			 cntx->hard_smem_start,
			 cntx->hard_smem_len);
	if (rc) {
		vmm_printf("%s: fb_set_smem() failed error %d\n",
			   __func__, rc);
		goto hard_bind_error;
	}

	/* Mark hard bind as on */
	cntx->hard_vdis = TRUE;

	return VMM_OK;

hard_bind_error:
	vmm_printf("vscreen: %s: rows=%d\n", 
		   cntx->info->name, cntx->info->var.xres_virtual);
	vmm_printf("vscreen: %s: cols=%d\n",
		   cntx->info->name, cntx->info->var.yres_virtual);
	vmm_printf("vscreen: %s: bits_per_pixel=%d\n",
		   cntx->info->name, cntx->info->var.bits_per_pixel);
	vmm_printf("vscreen: %s: sem_start=0x%llx\n",
		   cntx->info->name, (u64)cntx->smem_start);
	vmm_printf("vscreen: %s: sem_len=0x%x\n",
		   cntx->info->name, cntx->smem_len);
	vmm_printf("vscreen: %s: rows=%d\n",
		   cntx->vdis->name, rows);
	vmm_printf("vscreen: %s: cols=%d\n",
		   cntx->vdis->name, cols);
	vmm_printf("vscreen: %s: bits_per_pixel=%d\n",
		   cntx->vdis->name, pf.bits_per_pixel);
	vmm_printf("vscreen: %s: sem_start=0x%llx\n",
		   cntx->vdis->name, (u64)cntx->hard_smem_start);
	vmm_printf("vscreen: %s: sem_len=0x%x\n",
		   cntx->vdis->name, cntx->hard_smem_len);
	return rc;
}

static int vscreen_vdisplay_notification(struct vmm_notifier_block *nb,
					 unsigned long evt, void *data)
{
	struct vmm_vdisplay_event *event = data;
	struct vscreen_context *cntx =
		container_of(nb, struct vscreen_context, vdis_client);

	if (evt == VMM_VDISPLAY_EVENT_DESTROY) {
		if (cntx->vdis == event->data) {
			/* Signal Exit */
			vscreen_enqueue_work(cntx, VSCREEN_WORK_EXIT);
			/* Clear vdisplay pointer */
			cntx->vdis = NULL;
		}
		return NOTIFY_OK;
	}

	/* We are only interested in destroy events so,
	 * don't care about this event.
	 */
	return NOTIFY_DONE;
}

static int vscreen_vinput_notification(struct vmm_notifier_block *nb,
					unsigned long evt, void *data)
{
	struct vmm_vinput_event *event = data;
	struct vscreen_context *cntx =
		container_of(nb, struct vscreen_context, vinp_client);

	if (evt == VMM_VINPUT_EVENT_DESTROY_KEYBOARD) {
		if (cntx->vkbd == event->data) {
			/* Clear vkeyboard pointer */
			cntx->vkbd = NULL;
		}
		return NOTIFY_OK;
	} else if (evt == VMM_VINPUT_EVENT_DESTROY_MOUSE) {
		if (cntx->vmou == event->data) {
			/* Clear vmouse pointer */
			cntx->vmou = NULL;
		}
		return NOTIFY_OK;
	}

	/* We are only interested in destroy events
	 * so, don't care about this event.
	 */
	return NOTIFY_DONE;
}

static void vscreen_save(struct fb_info *info, void *priv)
{
	struct vscreen_context *cntx = priv;

	/* Set freeze state (Must be first step) */
	cntx->freeze = TRUE;

	/* Switch back to original settings for hard bind */
	vscreen_hard_switch_back(cntx);

	/* Disconnect input handler */
	input_disconnect_handler(&cntx->hndl);

	/* Reset event tracking state */
	cntx->key_event = FALSE;
	cntx->key_code = 0;
	cntx->key_value = 0;
	cntx->mouse_event = FALSE;
	cntx->mouse_btn = 0;
	cntx->mouse_dx = 0;
	cntx->mouse_dy = 0;
	cntx->mouse_dz = 0;

	/* Erase display */
	vscreen_blank_display(cntx);
}

static void vscreen_restore(struct fb_info *info, void *priv)
{
	struct vscreen_context *cntx = priv;

	/* Set current variable screen info */
	fb_set_var(cntx->info, &cntx->var);

	/* Set current color map */
	fb_set_cmap(&cntx->cmap, cntx->info);

	/* Erase display */
	vscreen_blank_display(cntx);

	/* Connect input handler */
	input_connect_handler(&cntx->hndl);

	/* Clear freeze state (Must be last step) */
	cntx->freeze = FALSE;
}

static void vscreen_process(struct vscreen_context *cntx)
{
	int rc;
	u64 timeout;
	irq_flags_t flags;
	bool do_exit = FALSE;
	struct dlist *l;
	struct vscreen_work *work;

	while (1) {
		/* Try to wait for work with timeout */
		timeout = cntx->work_timeout;
		rc = vmm_completion_wait_timeout(&cntx->work_avail, &timeout);

		/* If we timedout then refresh virtual screen */
		if (rc == VMM_ETIMEDOUT) {
			if (cntx->is_hard) {
				rc = vscreen_hard_refresh(cntx);
			} else {
				rc = vscreen_soft_refresh(cntx);
			}
		}

		/* Got error hence bail-out */
		if (rc) {
			break;
		}

		/* Try to dequeue work */
		vmm_spin_lock_irqsave(&cntx->work_list_lock, flags);
		if (list_empty(&cntx->work_list)) {
			vmm_spin_unlock_irqrestore(&cntx->work_list_lock,
						   flags);
			continue;
		}
		l = list_pop(&cntx->work_list);
		work = list_entry(l, struct vscreen_work, head);
		vmm_spin_unlock_irqrestore(&cntx->work_list_lock, flags);

		/* Process work */
		switch (work->type) {
		case VSCREEN_WORK_EXIT:
			do_exit = TRUE;
			break;
		default:
			break;
		}

		/* Free work */
		vmm_free(work);

		/* If we got exit then break */
		if (do_exit) {
			break;
		}
	}

	/* Set freeze state */
	cntx->freeze = TRUE;

	/* Flush all pending work */
	vmm_spin_lock_irqsave(&cntx->work_list_lock, flags);
	while (!list_empty(&cntx->work_list)) {
		l = list_pop(&cntx->work_list);
		work = list_entry(l, struct vscreen_work, head);
		vmm_free(work);
	}
	vmm_spin_unlock_irqrestore(&cntx->work_list_lock, flags);
}

static int vscreen_setup(struct vscreen_context *cntx)
{
	int rc;
	static atomic_t setup_count = ARCH_ATOMIC_INITIALIZER(0);

	/* Give some name to context */
	arch_atomic_inc(&setup_count);
	vmm_snprintf(cntx->name, sizeof(cntx->name),
		     "vscreen-%d", (u32)arch_atomic_read(&setup_count));

	/* Clear freeze state */
	cntx->freeze = FALSE;

	/* Clear escape key state */
	cntx->esc_key_state = 0;

	/* Setup input handler */
	cntx->hndl.name = cntx->name;
	cntx->hndl.evbit[0] = BIT_MASK(EV_SYN) |
				BIT_MASK(EV_KEY) |
				BIT_MASK(EV_REL);
	cntx->hndl.event = vscreen_event;
	cntx->hndl.priv = cntx;
	rc = input_register_handler(&cntx->hndl);
	if (rc) {
		goto setup_fail;
	}

	/* Connect input handler */
	rc = input_connect_handler(&cntx->hndl);
	if (rc) {
		goto unreg_ihndl;
	}

	/* Open frame buffer*/
	rc = fb_open(cntx->info, vscreen_save, vscreen_restore, cntx);
	if (rc) {
		goto discon_ihndl;
	}

	/* Find video mode with equal or greater resolution */
	cntx->mode = fb_find_best_mode(&cntx->info->var,
					&cntx->info->modelist);
	if (!cntx->mode) {
		rc = VMM_EFAIL;
		goto release_fb;
	}

	/* Convert video mode to variable screen info */
	fb_videomode_to_var(&cntx->var, cntx->mode);
	cntx->var.bits_per_pixel = cntx->info->var.bits_per_pixel;
	cntx->var.activate = FB_ACTIVATE_NOW;

	/* Check and update variable screen info */
	rc = fb_check_var(cntx->info, &cntx->var);
	if (rc) {
		goto release_fb;
	}

	/* Set current variable screen info */
	rc = fb_set_var(cntx->info, &cntx->var);
	if (rc) {
		goto release_fb;
	}

	/* Alloc color map */
	rc = fb_alloc_cmap(&cntx->cmap, 8, 0);
	if (rc) {
		goto release_fb;
	}
	cntx->cmap.red[VSCREEN_COLOR_BLACK] = 0x0000;
	cntx->cmap.green[VSCREEN_COLOR_BLACK] = 0x0000;
	cntx->cmap.blue[VSCREEN_COLOR_BLACK] = 0x0000;
	cntx->cmap.red[VSCREEN_COLOR_RED] = 0xffff;
	cntx->cmap.green[VSCREEN_COLOR_RED] = 0x0000;
	cntx->cmap.blue[VSCREEN_COLOR_RED] = 0x0000;
	cntx->cmap.red[VSCREEN_COLOR_GREEN] = 0x0000;
	cntx->cmap.green[VSCREEN_COLOR_GREEN] = 0xffff;
	cntx->cmap.blue[VSCREEN_COLOR_GREEN] = 0x0000;
	cntx->cmap.red[VSCREEN_COLOR_YELLOW] = 0xffff;
	cntx->cmap.green[VSCREEN_COLOR_YELLOW] = 0xffff;
	cntx->cmap.blue[VSCREEN_COLOR_YELLOW] = 0x0000;
	cntx->cmap.red[VSCREEN_COLOR_BLUE] = 0x0000;
	cntx->cmap.green[VSCREEN_COLOR_BLUE] = 0x0000;
	cntx->cmap.blue[VSCREEN_COLOR_BLUE] = 0xffff;
	cntx->cmap.red[VSCREEN_COLOR_MAGENTA] = 0xffff;
	cntx->cmap.green[VSCREEN_COLOR_MAGENTA] = 0x0000;
	cntx->cmap.blue[VSCREEN_COLOR_MAGENTA] = 0xffff;
	cntx->cmap.red[VSCREEN_COLOR_CYAN] = 0x0000;
	cntx->cmap.green[VSCREEN_COLOR_CYAN] = 0xffff;
	cntx->cmap.blue[VSCREEN_COLOR_CYAN] = 0xffff;
	cntx->cmap.red[VSCREEN_COLOR_WHITE] = 0xffff;
	cntx->cmap.green[VSCREEN_COLOR_WHITE] = 0xffff;
	cntx->cmap.blue[VSCREEN_COLOR_WHITE] = 0xffff;
	cntx->fc = VSCREEN_DEFAULT_FC;
	cntx->bc = VSCREEN_DEFAULT_BC;

	/* Set color map */
	rc = fb_set_cmap(&cntx->cmap, cntx->info);
	if (rc) {
		goto dealloc_cmap;
	}

	/* Save current smem start address and length */
	rc = fb_get_smem(cntx->info, &cntx->smem_start, &cntx->smem_len);
	if (rc) {
		goto dealloc_cmap;
	}

	/* Reset event tracking state */
	cntx->key_event = FALSE;
	cntx->key_code = 0;
	cntx->key_value = 0;
	cntx->mouse_event = FALSE;
	cntx->mouse_btn = 0;
	cntx->mouse_dx = 0;
	cntx->mouse_dy = 0;
	cntx->mouse_dz = 0;

	/* Make sure hard bind state is off */
	cntx->hard_vdis = FALSE;

	/* Setup work queue */
	cntx->work_timeout = udiv64(1000000000ULL, cntx->refresh_rate);
	INIT_SPIN_LOCK(&cntx->work_list_lock);
	INIT_LIST_HEAD(&cntx->work_list);
	INIT_COMPLETION(&cntx->work_avail);

	/* Register vdisplay notifier client */
	cntx->vdis_client.notifier_call = &vscreen_vdisplay_notification;
	cntx->vdis_client.priority = 0;
	rc = vmm_vdisplay_register_client(&cntx->vdis_client);
	if (rc) {
		goto dealloc_cmap;
	}

	/* Register vinput notifier client */
	cntx->vinp_client.notifier_call = &vscreen_vinput_notification;
	cntx->vinp_client.priority = 0;
	rc = vmm_vinput_register_client(&cntx->vinp_client);
	if (rc) {
		goto unreg_vdisplay_client;
	}

	return VMM_OK;

unreg_vdisplay_client:
	vmm_vdisplay_unregister_client(&cntx->vdis_client);
dealloc_cmap:
	fb_dealloc_cmap(&cntx->cmap);
release_fb:
	fb_release(cntx->info);
discon_ihndl:
	input_disconnect_handler(&cntx->hndl);
unreg_ihndl:
	input_unregister_handler(&cntx->hndl);
setup_fail:
	return rc;
}

static int vscreen_cleanup(struct vscreen_context *cntx)
{
	int rc, rc1, rc2;

	/* Switch back to original settings for hard bind */
	vscreen_hard_switch_back(cntx);

	/* Unregister vinput notifier client */
	vmm_vinput_unregister_client(&cntx->vinp_client);

	/* Unregister vdisplay notifier client */
	vmm_vdisplay_unregister_client(&cntx->vdis_client);

	/* Dealloc color map */
	fb_dealloc_cmap(&cntx->cmap);

	/* Release frame buffer */
	rc = fb_release(cntx->info);

	/* Disconnect input handler */
	rc1 = input_disconnect_handler(&cntx->hndl);

	/* Unregister input handler */
	rc2 = input_unregister_handler(&cntx->hndl);

	if (rc) {
		return rc;
	} else if (rc1) {
		return rc1;
	} else if (rc2) {
		return rc2;
	}

	return VMM_OK;
}

int vscreen_bind(bool is_hard,
		 u32 refresh_rate,
		 u32 esc_key_code0,
		 u32 esc_key_code1,
		 u32 esc_key_code2,
		 struct fb_info *info,
		 struct vmm_vdisplay *vdis,
		 struct vmm_vkeyboard *vkbd,
		 struct vmm_vmouse *vmou)
{
	int rc;
	struct vscreen_context *cntx;

	/* Can be called from Orphan (or Thread) context only */
	BUG_ON(!vmm_scheduler_orphan_context());

	/* Sanity checks */
	if (!info || !vdis) {
		return VMM_EINVALID;
	}
	if ((refresh_rate < VSCREEN_REFRESH_RATE_MIN) ||
	    (VSCREEN_REFRESH_RATE_MAX < refresh_rate) ||
	    (info->fix.visual != FB_VISUAL_TRUECOLOR)) {
		return VMM_EINVALID;
	}

	/* Alloc vscreen context */
	cntx = vmm_zalloc(sizeof(struct vscreen_context));
	if (!cntx) {
		return VMM_ENOMEM;
	}

	/* Save parameters */
	cntx->is_hard = is_hard;
	cntx->refresh_rate = refresh_rate;
	cntx->esc_key_code[0] = esc_key_code0;
	cntx->esc_key_code[1] = esc_key_code1;
	cntx->esc_key_code[2] = esc_key_code2;
	cntx->info = info;
	cntx->vdis = vdis;
	cntx->vkbd = vkbd;
	cntx->vmou = vmou;

	/* Setup and intialize vscreen */
	rc = vscreen_setup(cntx);
	if (rc) {
		vmm_free(cntx);
		return rc;
	}

	/* Process work until some one signals exit */
	vscreen_process(cntx);

	/* Cleanup vscreen */
	rc = vscreen_cleanup(cntx);

	/* Free vscreen context */
	vmm_free(cntx);

	return rc;
}
VMM_EXPORT_SYMBOL(vscreen_bind);

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
