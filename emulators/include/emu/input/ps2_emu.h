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
 * @file ps2_emu.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief PS/2 keyboard/mouse emulation
 *
 * The source has been largely adapted from QEMU include/hw/input/ps2.h
 * 
 * QEMU PS/2 keyboard/mouse emulation
 *
 * Copyright (c) 2003 Fabrice Bellard
 *
 * The original code is licensed under the GPL.
 */
#ifndef __PS2_EMU_H__
#define __PS2_EMU_H__

#include <vmm_spinlocks.h>
#include <vio/vmm_vinput.h>

#define PS2_EMU_IPRIORITY	(VMM_VINPUT_IPRIORITY+1)

/* Keyboard Commands */
#define KBD_CMD_SET_LEDS	0xED	/* Set keyboard leds */
#define KBD_CMD_ECHO     	0xEE
#define KBD_CMD_SCANCODE	0xF0	/* Get/set scancode set */
#define KBD_CMD_GET_ID 	        0xF2	/* get keyboard ID */
#define KBD_CMD_SET_RATE	0xF3	/* Set typematic rate */
#define KBD_CMD_ENABLE		0xF4	/* Enable scanning */
#define KBD_CMD_RESET_DISABLE	0xF5	/* reset and disable scanning */
#define KBD_CMD_RESET_ENABLE   	0xF6    /* reset and enable scanning */
#define KBD_CMD_RESET		0xFF	/* Reset */

/* Keyboard Replies */
#define KBD_REPLY_POR		0xAA	/* Power on reset */
#define KBD_REPLY_ID		0xAB	/* Keyboard ID */
#define KBD_REPLY_ACK		0xFA	/* Command ACK */
#define KBD_REPLY_RESEND	0xFE	/* Command NACK, send the cmd again */

/* Mouse Commands */
#define AUX_SET_SCALE11		0xE6	/* Set 1:1 scaling */
#define AUX_SET_SCALE21		0xE7	/* Set 2:1 scaling */
#define AUX_SET_RES		0xE8	/* Set resolution */
#define AUX_GET_SCALE		0xE9	/* Get scaling factor */
#define AUX_SET_STREAM		0xEA	/* Set stream mode */
#define AUX_POLL		0xEB	/* Poll */
#define AUX_RESET_WRAP		0xEC	/* Reset wrap mode */
#define AUX_SET_WRAP		0xEE	/* Set wrap mode */
#define AUX_SET_REMOTE		0xF0	/* Set remote mode */
#define AUX_GET_TYPE		0xF2	/* Get type */
#define AUX_SET_SAMPLE		0xF3	/* Set sample rate */
#define AUX_ENABLE_DEV		0xF4	/* Enable aux device */
#define AUX_DISABLE_DEV		0xF5	/* Disable aux device */
#define AUX_SET_DEFAULT		0xF6
#define AUX_RESET		0xFF	/* Reset aux device */
#define AUX_ACK			0xFA	/* Command byte ACK. */

#define MOUSE_STATUS_REMOTE     0x40
#define MOUSE_STATUS_ENABLED    0x20
#define MOUSE_STATUS_SCALE21    0x10

#define PS2_EMU_QUEUE_SIZE	256

struct ps2_emu_queue {
	u8 data[PS2_EMU_QUEUE_SIZE];
	int rptr, wptr, count;
};

struct ps2_emu_state {
	vmm_spinlock_t lock;
	struct ps2_emu_queue queue;
	s32 write_cmd;
	void (*update_irq)(void *, int);
	void *update_arg;
};

struct ps2_emu_keyboard {
	vmm_spinlock_t lock;
	struct ps2_emu_state state;
	int scan_enabled;
	/* Xvisor uses QEMU-like translated PC scancodes internally. 
	 * To avoid multiple conversions we do the translation (if any)
	 * in the PS/2 emulation not the keyboard controller. 
	 */
	int translate;
	int scancode_set; /* 1=XT, 2=AT, 3=PS/2 */
	int ledstate;
	struct vmm_vkeyboard *keyboard;
};

struct ps2_emu_mouse {
	vmm_spinlock_t lock;
	struct ps2_emu_state state;
	u8 mouse_status;
	u8 mouse_resolution;
	u8 mouse_sample_rate;
	u8 mouse_wrap;
	u8 mouse_type; /* 0 = PS2, 3 = IMPS/2, 4 = IMEX */
	u8 mouse_detect_state;
	int mouse_dx; /* current values, needed for 'poll' mode */
	int mouse_dy;
	int mouse_dz;
	u8 mouse_buttons;
	struct vmm_vmouse *mouse;
};

/* ===== PS/2 Queue APIs ===== */
void ps2_emu_queue(struct ps2_emu_state *s, int b);
u32 ps2_emu_read_data(struct ps2_emu_state *s);

/* ===== Keyboard Emulation APIs ===== */
struct ps2_emu_keyboard *ps2_emu_alloc_keyboard(const char *name,
					void (*update_irq)(void *, int),
					void *update_arg);
int ps2_emu_free_keyboard(struct ps2_emu_keyboard *k);
int ps2_emu_reset_keyboard(struct ps2_emu_keyboard *k);
int ps2_emu_write_keyboard(struct ps2_emu_keyboard *k, int val);
int ps2_emu_keyboard_set_translation(struct ps2_emu_keyboard *k,
				     int mode);

/* ===== Mouse Emulation APIs ===== */
struct ps2_emu_mouse *ps2_emu_alloc_mouse(const char *name,
					void (*update_irq)(void *, int),
					void *update_arg);
int ps2_emu_free_mouse(struct ps2_emu_mouse *m);
int ps2_emu_reset_mouse(struct ps2_emu_mouse *m);
int ps2_emu_write_mouse(struct ps2_emu_mouse *m, int val);
int ps2_emu_mouse_fake_event(struct ps2_emu_mouse *m);

#endif /* __PS2_EMU_H__ */
