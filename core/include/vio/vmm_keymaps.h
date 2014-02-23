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
 * @file vmm_keymaps.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief keysym to keycode conversion using keyboad mappings header
 *
 * The header has been largely adapted from QEMU sources:
 * ui/keymaps.h
 *
 * QEMU keysym to keycode conversion using rdesktop keymaps
 *
 * Copyright (c) 2004 Johannes Schindelin
 *
 * The original source is licensed under GPL.
 */

/*
 * Daemons such as VNC server, GUI render, etc will use vmm_keymaps APIs
 * for converting key press events (keysms) into intermediate scancodes
 * (keycode). These daemons will pass intermediate scancodes (keycode) to
 * Guest emulated keyboard device via vmm_vinput APIs. This in-turn causes
 * Guest OS to receive virtual key press events.
 *
 * The figure below sumarizes the above:
 *
 * -------------             ----------------            --------------
 * |  Daemon   |   Using     |              |   Using    |    Guest   |
 * | Key Press |============>| Intermediate |===========>|  Key Press |
 * |   Event   | vmm_keymaps |   Scancode   | vmm_vinput |    Event   |
 * -------------             ----------------            --------------
 *
 * The format of intermediate scancode is as follows:
 *
 *  --------------------------------------
 *  | Bits[11:7] | Bits[8:8] | Bits[7:0] |
 *  | Modifiers  |  Status   |  KeyCode  |
 *  --------------------------------------
 *
 *  KeyCode   = Key position/number
 *  Status    = Key state Up (=1) or Down (=0)
 *  Modifiers = Key state for SHIFT, CTRL, ALT, and ALTGR keys
 */

#ifndef __VMM_KEYMAPS_H_
#define __VMM_KEYMAPS_H_

#include <vmm_types.h>

struct vmm_name2keysym {
	const char* name;
	int keysym;
};

struct vmm_key_range {
	int start;
	int end;
	struct vmm_key_range *next;
};

#define VMM_MAX_NORMAL_KEYCODE 512
#define VMM_MAX_EXTRA_COUNT 256
struct vmm_keymap_layout {
	u16 keysym2keycode[VMM_MAX_NORMAL_KEYCODE];
	struct {
		int keysym;
	u16 keycode;
	} keysym2keycode_extra[VMM_MAX_EXTRA_COUNT];
	int extra_count;
	struct vmm_key_range *keypad_range;
	struct vmm_key_range *numlock_range;
};

/* scancode without modifiers */
#define SCANCODE_KEYMASK	0xff
/* scancode without grey or up bit */
#define SCANCODE_KEYCODEMASK	0x7f

/* "grey" keys will usually need a 0xe0 prefix */
#define SCANCODE_GREY		0x80
#define SCANCODE_EMUL0		0xE0
/* "up" flag */
#define SCANCODE_UP		0x80

/* Additional modifiers to use if not catched another way. */
#define SCANCODE_SHIFT		0x100
#define SCANCODE_CTRL		0x200
#define SCANCODE_ALT		0x400
#define SCANCODE_ALTGR		0x800

struct vmm_keymap_layout *vmm_keymap_alloc_layout(
					const struct vmm_name2keysym *table,
					const char *lang);
void vmm_keymap_free_layout(struct vmm_keymap_layout *layout);
int vmm_keysym2scancode(struct vmm_keymap_layout *layout, int keysym);
bool vmm_keycode_is_keypad(struct vmm_keymap_layout *layout, int keycode);
bool vmm_keysym_is_numlock(struct vmm_keymap_layout *layout, int keysym);

/* Virtual keys */
enum vmm_vkey {
	VMM_VKEY_SHIFT = 0,
	VMM_VKEY_SHIFT_R = 1,
	VMM_VKEY_ALT = 2,
	VMM_VKEY_ALT_R = 3,
	VMM_VKEY_ALTGR = 4,
	VMM_VKEY_ALTGR_R = 5,
	VMM_VKEY_CTRL = 6,
	VMM_VKEY_CTRL_R = 7,
	VMM_VKEY_MENU = 8,
	VMM_VKEY_ESC = 9,
	VMM_VKEY_1 = 10,
	VMM_VKEY_2 = 11,
	VMM_VKEY_3 = 12,
	VMM_VKEY_4 = 13,
	VMM_VKEY_5 = 14,
	VMM_VKEY_6 = 15,
	VMM_VKEY_7 = 16,
	VMM_VKEY_8 = 17,
	VMM_VKEY_9 = 18,
	VMM_VKEY_0 = 19,
	VMM_VKEY_MINUS = 20,
	VMM_VKEY_EQUAL = 21,
	VMM_VKEY_BACKSPACE = 22,
	VMM_VKEY_TAB = 23,
	VMM_VKEY_Q = 24,
	VMM_VKEY_W = 25,
	VMM_VKEY_E = 26,
	VMM_VKEY_R = 27,
	VMM_VKEY_T = 28,
	VMM_VKEY_Y = 29,
	VMM_VKEY_U = 30,
	VMM_VKEY_I = 31,
	VMM_VKEY_O = 32,
	VMM_VKEY_P = 33,
	VMM_VKEY_BRACKET_LEFT = 34,
	VMM_VKEY_BRACKET_RIGHT = 35,
	VMM_VKEY_RET = 36,
	VMM_VKEY_A = 37,
	VMM_VKEY_S = 38,
	VMM_VKEY_D = 39,
	VMM_VKEY_F = 40,
	VMM_VKEY_G = 41,
	VMM_VKEY_H = 42,
	VMM_VKEY_J = 43,
	VMM_VKEY_K = 44,
	VMM_VKEY_L = 45,
	VMM_VKEY_SEMICOLON = 46,
	VMM_VKEY_APOSTROPHE = 47,
	VMM_VKEY_GRAVE_ACCENT = 48,
	VMM_VKEY_BACKSLASH = 49,
	VMM_VKEY_Z = 50,
	VMM_VKEY_X = 51,
	VMM_VKEY_C = 52,
	VMM_VKEY_V = 53,
	VMM_VKEY_B = 54,
	VMM_VKEY_N = 55,
	VMM_VKEY_M = 56,
	VMM_VKEY_COMMA = 57,
	VMM_VKEY_DOT = 58,
	VMM_VKEY_SLASH = 59,
	VMM_VKEY_ASTERISK = 60,
	VMM_VKEY_SPC = 61,
	VMM_VKEY_CAPS_LOCK = 62,
	VMM_VKEY_F1 = 63,
	VMM_VKEY_F2 = 64,
	VMM_VKEY_F3 = 65,
	VMM_VKEY_F4 = 66,
	VMM_VKEY_F5 = 67,
	VMM_VKEY_F6 = 68,
	VMM_VKEY_F7 = 69,
	VMM_VKEY_F8 = 70,
	VMM_VKEY_F9 = 71,
	VMM_VKEY_F10 = 72,
	VMM_VKEY_NUM_LOCK = 73,
	VMM_VKEY_SCROLL_LOCK = 74,
	VMM_VKEY_KP_DIVIDE = 75,
	VMM_VKEY_KP_MULTIPLY = 76,
	VMM_VKEY_KP_SUBTRACT = 77,
	VMM_VKEY_KP_ADD = 78,
	VMM_VKEY_KP_ENTER = 79,
	VMM_VKEY_KP_DECIMAL = 80,
	VMM_VKEY_SYSRQ = 81,
	VMM_VKEY_KP_0 = 82,
	VMM_VKEY_KP_1 = 83,
	VMM_VKEY_KP_2 = 84,
	VMM_VKEY_KP_3 = 85,
	VMM_VKEY_KP_4 = 86,
	VMM_VKEY_KP_5 = 87,
	VMM_VKEY_KP_6 = 88,
	VMM_VKEY_KP_7 = 89,
	VMM_VKEY_KP_8 = 90,
	VMM_VKEY_KP_9 = 91,
	VMM_VKEY_LESS = 92,
	VMM_VKEY_F11 = 93,
	VMM_VKEY_F12 = 94,
	VMM_VKEY_PRINT = 95,
	VMM_VKEY_HOME = 96,
	VMM_VKEY_PGUP = 97,
	VMM_VKEY_PGDN = 98,
	VMM_VKEY_END = 99,
	VMM_VKEY_LEFT = 100,
	VMM_VKEY_UP = 101,
	VMM_VKEY_DOWN = 102,
	VMM_VKEY_RIGHT = 103,
	VMM_VKEY_INSERT = 104,
	VMM_VKEY_DELETE = 105,
	VMM_VKEY_STOP = 106,
	VMM_VKEY_AGAIN = 107,
	VMM_VKEY_PROPS = 108,
	VMM_VKEY_UNDO = 109,
	VMM_VKEY_FRONT = 110,
	VMM_VKEY_COPY = 111,
	VMM_VKEY_OPEN = 112,
	VMM_VKEY_PASTE = 113,
	VMM_VKEY_FIND = 114,
	VMM_VKEY_CUT = 115,
	VMM_VKEY_LF = 116,
	VMM_VKEY_HELP = 117,
	VMM_VKEY_META_L = 118,
	VMM_VKEY_META_R = 119,
	VMM_VKEY_COMPOSE = 120,
	VMM_VKEY_MAX = 121,
};

int vmm_keyname2vkey(const char *key);
int vmm_keycode2vkey(int keycode);
int vmm_vkey2keycode(int vkey);

#endif /* __VMM_KEYMAPS_H_ */


