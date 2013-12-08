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
 * @file ps2_emu.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief PS/2 Keyboard/Mouse Emulation.
 *
 * The source has been largely adapted from QEMU hw/input/ps2.c
 * 
 * QEMU PS/2 keyboard/mouse emulation
 *
 * Copyright (c) 2003 Fabrice Bellard
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vio/vmm_vinput.h>
#include <emu/input/ps2_emu.h>

#define MODULE_DESC			"PS/2 Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(PS2_EMU_IPRIORITY)
#define	MODULE_INIT			NULL
#define	MODULE_EXIT			NULL

/* ===== PS/2 Queue APIs ===== */

void ps2_emu_queue(struct ps2_emu_state *s, int b)
{
	irq_flags_t flags;
	struct ps2_emu_queue *q;

	if (!s) {
		return;
	}

	vmm_spin_lock_irqsave(&s->lock, flags);

	q = &s->queue;
	if (q->count >= PS2_EMU_QUEUE_SIZE) {
		vmm_spin_unlock_irqrestore(&s->lock, flags);
		return;
	}
	q->data[q->wptr] = b;
	if (++q->wptr == PS2_EMU_QUEUE_SIZE) {
		q->wptr = 0;
	}
	q->count++;

	vmm_spin_unlock_irqrestore(&s->lock, flags);

	if (s->update_irq) {
		s->update_irq(s->update_arg, 1);
	}
}
VMM_EXPORT_SYMBOL(ps2_emu_queue);

u32 ps2_emu_read_data(struct ps2_emu_state *s)
{
	u32 val;
	int index, level = 0;
	bool update = FALSE;
	irq_flags_t flags;
	struct ps2_emu_queue *q;

	if (!s) {
		return 0;
	}

	vmm_spin_lock_irqsave(&s->lock, flags);

	q = &s->queue;
	if (q->count == 0) {
		/* NOTE: if no data left, we return the
		 * last keyboard one (needed for EMM386)
		 */
		/* XXX: need a timer to do things correctly */
		index = q->rptr - 1;
		if (index < 0) {
			index = PS2_EMU_QUEUE_SIZE - 1;
		}
		val = q->data[index];
	} else {
		val = q->data[q->rptr];
		if (++q->rptr == PS2_EMU_QUEUE_SIZE) {
			q->rptr = 0;
		}
		q->count--;
		update = TRUE;
		level = q->count != 0;
	}

	vmm_spin_unlock_irqrestore(&s->lock, flags);

	if (update && s->update_irq) {
		/* reading deasserts IRQ */
		s->update_irq(s->update_arg, 0);
		/* reassert IRQs if data left */
		s->update_irq(s->update_arg, level);
	}

	return val;
}
VMM_EXPORT_SYMBOL(ps2_emu_read_data);

static int ps2_emu_queue_count(struct ps2_emu_state *s)
{
	int ret;
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&s->lock, flags);
	ret = s->queue.count;
	vmm_spin_unlock_irqrestore(&s->lock, flags);

	return ret;
}

static s32 ps2_emu_get_write_cmd(struct ps2_emu_state *s)
{
	s32 ret;
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&s->lock, flags);
	ret = s->write_cmd;
	vmm_spin_unlock_irqrestore(&s->lock, flags);

	return ret;
}

static void ps2_emu_set_write_cmd(struct ps2_emu_state *s, s32 write_cmd)
{
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&s->lock, flags);
	s->write_cmd = write_cmd;
	vmm_spin_unlock_irqrestore(&s->lock, flags);
}

static void ps2_emu_common_reset(struct ps2_emu_state *s)
{
	irq_flags_t flags;
	struct ps2_emu_queue *q;

	vmm_spin_lock_irqsave(&s->lock, flags);

	s->write_cmd = -1;
	q = &s->queue;
	q->rptr = 0;
	q->wptr = 0;
	q->count = 0;

	vmm_spin_unlock_irqrestore(&s->lock, flags);

	if (s->update_irq) {
		s->update_irq(s->update_arg, 0);
	}
}

/* ===== Keyboard Emulation APIs ===== */

/* Table to convert from PC scancodes to raw scancodes.  */
static const unsigned char ps2_raw_keycode[128] = {
  0, 118,  22,  30,  38,  37,  46,  54,  61,  62,  70,  69,  78,  85, 102,  13,
 21,  29,  36,  45,  44,  53,  60,  67,  68,  77,  84,  91,  90,  20,  28,  27,
 35,  43,  52,  51,  59,  66,  75,  76,  82,  14,  18,  93,  26,  34,  33,  42,
 50,  49,  58,  65,  73,  74,  89, 124,  17,  41,  88,   5,   6,   4,  12,   3,
 11,   2,  10,   1,   9, 119, 126, 108, 117, 125, 123, 107, 115, 116, 121, 105,
114, 122, 112, 113, 127,  96,  97, 120,   7,  15,  23,  31,  39,  47,  55,  63,
 71,  79,  86,  94,   8,  16,  24,  32,  40,  48,  56,  64,  72,  80,  87, 111,
 19,  25,  57,  81,  83,  92,  95,  98,  99, 100, 101, 103, 104, 106, 109, 110
};
static const unsigned char ps2_raw_keycode_set3[128] = {
  0,   8,  22,  30,  38,  37,  46,  54,  61,  62,  70,  69,  78,  85, 102,  13,
 21,  29,  36,  45,  44,  53,  60,  67,  68,  77,  84,  91,  90,  17,  28,  27,
 35,  43,  52,  51,  59,  66,  75,  76,  82,  14,  18,  92,  26,  34,  33,  42,
 50,  49,  58,  65,  73,  74,  89, 126,  25,  41,  20,   7,  15,  23,  31,  39,
 47,   2,  63,  71,  79, 118,  95, 108, 117, 125, 132, 107, 115, 116, 124, 105,
114, 122, 112, 113, 127,  96,  97,  86,  94,  15,  23,  31,  39,  47,  55,  63,
 71,  79,  86,  94,   8,  16,  24,  32,  40,  48,  56,  64,  72,  80,  87, 111,
 19,  25,  57,  81,  83,  92,  95,  98,  99, 100, 101, 103, 104, 106, 109, 110
};

/* keycode is expressed as follow:
 * bit 7    - 0 key pressed, 1 = key released
 * bits 6-0 - translated scancode set 2
 *
 * Note: This function must be called with keyboard lock held
 */
static void __ps2_emu_keyboard_event(struct ps2_emu_keyboard *k, int keycode)
{
	/* XXX: add support for scancode set 1 */
	if (!k->translate && keycode < 0xe0 && k->scancode_set > 1) {
		if (keycode & 0x80) {
			ps2_emu_queue(&k->state, 0xf0);
		}
		if (k->scancode_set == 2) {
			keycode = ps2_raw_keycode[keycode & 0x7f];
		} else if (k->scancode_set == 3) {
			keycode = ps2_raw_keycode_set3[keycode & 0x7f];
		}
	}

	ps2_emu_queue(&k->state, keycode);
}

static void ps2_emu_keyboard_event(struct vmm_vkeyboard *vkbd, int keycode)
{
	irq_flags_t flags;
	struct ps2_emu_keyboard *k = vmm_vkeyboard_priv(vkbd);

	vmm_spin_lock_irqsave(&k->lock, flags);
	__ps2_emu_keyboard_event(k, keycode);
	vmm_spin_unlock_irqrestore(&k->lock, flags);
}

struct ps2_emu_keyboard *ps2_emu_alloc_keyboard(const char *name,
					void (*update_irq)(void *, int),
					void *update_arg)
{
	struct ps2_emu_keyboard *k;

	if (!name) {
		return NULL;
	};

	k = vmm_zalloc(sizeof(struct ps2_emu_keyboard));
	if (!k) {
		return NULL;
	}

	INIT_SPIN_LOCK(&k->lock);
	INIT_SPIN_LOCK(&k->state.lock);
	k->state.update_irq = update_irq;
	k->state.update_arg = update_arg;
	k->scancode_set = 2;

	k->keyboard = vmm_vkeyboard_create(name, ps2_emu_keyboard_event, k);
	if (!k->keyboard) {
		vmm_free(k);
		return NULL;
	}

	return k;
}
VMM_EXPORT_SYMBOL(ps2_emu_alloc_keyboard);

int ps2_emu_free_keyboard(struct ps2_emu_keyboard *k)
{
	int rc;

	if (!k) {
		return VMM_EINVALID;
	}

	rc = vmm_vkeyboard_destroy(k->keyboard);
	vmm_free(k);

	return rc;
}
VMM_EXPORT_SYMBOL(ps2_emu_free_keyboard);

int ps2_emu_reset_keyboard(struct ps2_emu_keyboard *k)
{
	irq_flags_t flags;

	if (!k) {
		return VMM_EINVALID;
	}

	vmm_spin_lock_irqsave(&k->lock, flags);

	ps2_emu_common_reset(&k->state);
	k->scan_enabled = 0;
	k->translate = 0;
	k->scancode_set = 0;

	vmm_spin_unlock_irqrestore(&k->lock, flags);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(ps2_emu_reset_keyboard);

/* Note: This function must be called with keyboard lock held */
static void __ps2_emu_set_ledstate(struct ps2_emu_keyboard *k, int ledstate)
{
	k->ledstate = ledstate;
	vmm_vkeyboard_set_ledstate(k->keyboard, ledstate);
}

/* Note: This function must be called with keyboard lock held */
static void __ps2_emu_soft_reset_keyboard(struct ps2_emu_keyboard *k)
{
	k->scan_enabled = 1;
	k->scancode_set = 2;
	__ps2_emu_set_ledstate(k, 0);
}

int ps2_emu_write_keyboard(struct ps2_emu_keyboard *k, int val)
{
	irq_flags_t flags;

	if (!k) {
		return VMM_EINVALID;
	}

	vmm_spin_lock_irqsave(&k->lock, flags);

	switch(ps2_emu_get_write_cmd(&k->state)) {
	default:
	case -1:
		switch(val) {
		case 0x00:
			ps2_emu_queue(&k->state, KBD_REPLY_ACK);
			break;
		case 0x05:
			ps2_emu_queue(&k->state, KBD_REPLY_RESEND);
			break;
		case KBD_CMD_GET_ID:
			ps2_emu_queue(&k->state, KBD_REPLY_ACK);
			/* We emulate a MF2 AT keyboard here */
			ps2_emu_queue(&k->state, KBD_REPLY_ID);
			if (k->translate) {
				ps2_emu_queue(&k->state, 0x41);
			} else {
				ps2_emu_queue(&k->state, 0x83);
			}
			break;
		case KBD_CMD_ECHO:
			ps2_emu_queue(&k->state, KBD_CMD_ECHO);
			break;
		case KBD_CMD_ENABLE:
			k->scan_enabled = 1;
			ps2_emu_queue(&k->state, KBD_REPLY_ACK);
			break;
		case KBD_CMD_SCANCODE:
		case KBD_CMD_SET_LEDS:
		case KBD_CMD_SET_RATE:
			ps2_emu_set_write_cmd(&k->state, val);
			ps2_emu_queue(&k->state, KBD_REPLY_ACK);
			break;
		case KBD_CMD_RESET_DISABLE:
			__ps2_emu_soft_reset_keyboard(k);
			k->scan_enabled = 0;
			ps2_emu_queue(&k->state, KBD_REPLY_ACK);
			break;
		case KBD_CMD_RESET_ENABLE:
			__ps2_emu_soft_reset_keyboard(k);
			k->scan_enabled = 1;
			ps2_emu_queue(&k->state, KBD_REPLY_ACK);
			break;
		case KBD_CMD_RESET:
			__ps2_emu_soft_reset_keyboard(k);
			ps2_emu_queue(&k->state, KBD_REPLY_ACK);
			ps2_emu_queue(&k->state, KBD_REPLY_POR);
			break;
		default:
			ps2_emu_queue(&k->state, KBD_REPLY_ACK);
			break;
		};
		break;
	case KBD_CMD_SCANCODE:
		if (val == 0) {
			if (k->scancode_set == 1) {
				__ps2_emu_keyboard_event(k, 0x43);
			} else if (k->scancode_set == 2) {
				__ps2_emu_keyboard_event(k, 0x41);
			} else if (k->scancode_set == 3) {
				__ps2_emu_keyboard_event(k, 0x3f);
			}
		} else {
			if (val >= 1 && val <= 3) {
				k->scancode_set = val;
			}
			ps2_emu_queue(&k->state, KBD_REPLY_ACK);
		}
		ps2_emu_set_write_cmd(&k->state, -1);
		break;
	case KBD_CMD_SET_LEDS:
		__ps2_emu_set_ledstate(k, val);
		ps2_emu_queue(&k->state, KBD_REPLY_ACK);
		ps2_emu_set_write_cmd(&k->state, -1);
		break;
	case KBD_CMD_SET_RATE:
		ps2_emu_queue(&k->state, KBD_REPLY_ACK);
		ps2_emu_set_write_cmd(&k->state, -1);
		break;
	};

	vmm_spin_unlock_irqrestore(&k->lock, flags);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(ps2_emu_write_keyboard);

int ps2_emu_keyboard_set_translation(struct ps2_emu_keyboard *k,
				     int mode)
{
	irq_flags_t flags;

	if (!k) {
		return VMM_EINVALID;
	}

	vmm_spin_lock_irqsave(&k->lock, flags);
	k->translate = mode;
	vmm_spin_unlock_irqrestore(&k->lock, flags);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(ps2_emu_keyboard_set_translation);

/* ===== Mouse Emulation APIs ===== */

/* Note: This function must be called with mouse lock held */
static void __ps2_emu_mouse_send_packet(struct ps2_emu_mouse *m)
{
	unsigned int b;
	int dx1, dy1, dz1;

	dx1 = m->mouse_dx;
	dy1 = m->mouse_dy;
	dz1 = m->mouse_dz;

	/* XXX: increase range to 8 bits ? */
	if (dx1 > 127) {
		dx1 = 127;
	} else if (dx1 < -127) {
		dx1 = -127;
	}

	if (dy1 > 127) {
		dy1 = 127;
	} else if (dy1 < -127) {
		dy1 = -127;
	}

	b = 0x08 |
	    ((dx1 < 0) << 4) |
	    ((dy1 < 0) << 5) |
	    (m->mouse_buttons & 0x07);

	ps2_emu_queue(&m->state, b);
	ps2_emu_queue(&m->state, dx1 & 0xff);
	ps2_emu_queue(&m->state, dy1 & 0xff);

	/* extra byte for IMPS/2 or IMEX */
	switch(m->mouse_type) {
	default:
		break;
	case 3:
		if (dz1 > 127) {
			dz1 = 127;
		} else if (dz1 < -127) {
			dz1 = -127;
		}
		ps2_emu_queue(&m->state, dz1 & 0xff);
		break;
	case 4:
		if (dz1 > 7) {
			dz1 = 7;
		} else if (dz1 < -7) {
			dz1 = -7;
		}
		b = (dz1 & 0x0f) | ((m->mouse_buttons & 0x18) << 1);
		ps2_emu_queue(&m->state, b);
		break;
	};

	/* update deltas */
	m->mouse_dx -= dx1;
	m->mouse_dy -= dy1;
	m->mouse_dz -= dz1;
}

static void ps2_emu_mouse_event(struct vmm_vmouse *vmou,
				int dx, int dy, int dz, int buttons_state)
{
	irq_flags_t flags;
	struct ps2_emu_mouse *m = vmm_vmouse_priv(vmou);

	vmm_spin_lock_irqsave(&m->lock, flags);

	/* check if deltas are recorded when disabled */
	if (!(m->mouse_status & MOUSE_STATUS_ENABLED)) {
		vmm_spin_unlock_irqrestore(&m->lock, flags);
		return;
	}

	m->mouse_dx += dx;
	m->mouse_dy -= dy;
	m->mouse_dz += dz;
	/* XXX: SDL sometimes generates nul events: we delete them */
	if (m->mouse_dx == 0 &&
	    m->mouse_dy == 0 &&
	    m->mouse_dz == 0 &&
	    m->mouse_buttons == buttons_state) {
		vmm_spin_unlock_irqrestore(&m->lock, flags);
		return;
	}
	m->mouse_buttons = buttons_state;

	if (!(m->mouse_status & MOUSE_STATUS_REMOTE) &&
	     (ps2_emu_queue_count(&m->state) < (PS2_EMU_QUEUE_SIZE - 16))) {
		for(;;) {
			/* If not remote, send event. Multiple events
			 * are sent if too big deltas
			 */
			__ps2_emu_mouse_send_packet(m);
			if (m->mouse_dx == 0 &&
			    m->mouse_dy == 0 &&
			    m->mouse_dz == 0) {
				break;
			}
		}
	}

	vmm_spin_unlock_irqrestore(&m->lock, flags);
}

struct ps2_emu_mouse *ps2_emu_alloc_mouse(const char *name,
					void (*update_irq)(void *, int),
					void *update_arg)
{
	struct ps2_emu_mouse *m;

	if (!name) {
		return NULL;
	};

	m = vmm_zalloc(sizeof(struct ps2_emu_mouse));
	if (!m) {
		return NULL;
	}

	INIT_SPIN_LOCK(&m->lock);

	INIT_SPIN_LOCK(&m->state.lock);
	m->state.update_irq = update_irq;
	m->state.update_arg = update_arg;

	m->mouse = vmm_vmouse_create(name, FALSE, ps2_emu_mouse_event, m);
	if (!m->mouse) {
		vmm_free(m);
		return NULL;
	}

	return m;
}
VMM_EXPORT_SYMBOL(ps2_emu_alloc_mouse);

int ps2_emu_free_mouse(struct ps2_emu_mouse *m)
{
	int rc;

	if (!m) {
		return VMM_EINVALID;
	}

	rc = vmm_vmouse_destroy(m->mouse);
	vmm_free(m);

	return rc;
}
VMM_EXPORT_SYMBOL(ps2_emu_free_mouse);

int ps2_emu_reset_mouse(struct ps2_emu_mouse *m)
{
	irq_flags_t flags;

	if (!m) {
		return VMM_EINVALID;
	}

	vmm_spin_lock_irqsave(&m->lock, flags);

	ps2_emu_common_reset(&m->state);
	m->mouse_status = 0;
	m->mouse_resolution = 0;
	m->mouse_sample_rate = 0;
	m->mouse_wrap = 0;
	m->mouse_type = 0;
	m->mouse_detect_state = 0;
	m->mouse_dx = 0;
	m->mouse_dy = 0;
	m->mouse_dz = 0;
	m->mouse_buttons = 0;

	vmm_spin_unlock_irqrestore(&m->lock, flags);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(ps2_emu_reset_mouse);

int ps2_emu_write_mouse(struct ps2_emu_mouse *m, int val)
{
	irq_flags_t flags;

	if (!m) {
		return VMM_EINVALID;
	}

	vmm_spin_lock_irqsave(&m->lock, flags);

	switch (ps2_emu_get_write_cmd(&m->state)) {
	default:
	case -1:
		/* mouse command */
		if (m->mouse_wrap) {
			if (val == AUX_RESET_WRAP) {
				m->mouse_wrap = 0;
				ps2_emu_queue(&m->state, AUX_ACK);
				vmm_spin_unlock_irqrestore(&m->lock, flags);
				return VMM_OK;
			} else if (val != AUX_RESET) {
				ps2_emu_queue(&m->state, val);
				vmm_spin_unlock_irqrestore(&m->lock, flags);
				return VMM_OK;
			}
		}
		switch(val) {
		case AUX_SET_SCALE11:
			m->mouse_status &= ~MOUSE_STATUS_SCALE21;
			ps2_emu_queue(&m->state, AUX_ACK);
			break;
		case AUX_SET_SCALE21:
			m->mouse_status |= MOUSE_STATUS_SCALE21;
			ps2_emu_queue(&m->state, AUX_ACK);
			break;
		case AUX_SET_STREAM:
			m->mouse_status &= ~MOUSE_STATUS_REMOTE;
			ps2_emu_queue(&m->state, AUX_ACK);
			break;
		case AUX_SET_WRAP:
			m->mouse_wrap = 1;
			ps2_emu_queue(&m->state, AUX_ACK);
			break;
		case AUX_SET_REMOTE:
			m->mouse_status |= MOUSE_STATUS_REMOTE;
			ps2_emu_queue(&m->state, AUX_ACK);
			break;
		case AUX_GET_TYPE:
			ps2_emu_queue(&m->state, AUX_ACK);
			ps2_emu_queue(&m->state, m->mouse_type);
			break;
		case AUX_SET_RES:
		case AUX_SET_SAMPLE:
			ps2_emu_set_write_cmd(&m->state, val);
			ps2_emu_queue(&m->state, AUX_ACK);
			break;
		case AUX_GET_SCALE:
			ps2_emu_queue(&m->state, AUX_ACK);
			ps2_emu_queue(&m->state, m->mouse_status);
			ps2_emu_queue(&m->state, m->mouse_resolution);
			ps2_emu_queue(&m->state, m->mouse_sample_rate);
			break;
		case AUX_POLL:
			ps2_emu_queue(&m->state, AUX_ACK);
			__ps2_emu_mouse_send_packet(m);
			break;
		case AUX_ENABLE_DEV:
			m->mouse_status |= MOUSE_STATUS_ENABLED;
			ps2_emu_queue(&m->state, AUX_ACK);
			break;
		case AUX_DISABLE_DEV:
			m->mouse_status &= ~MOUSE_STATUS_ENABLED;
			ps2_emu_queue(&m->state, AUX_ACK);
			break;
		case AUX_SET_DEFAULT:
			m->mouse_sample_rate = 100;
			m->mouse_resolution = 2;
			m->mouse_status = 0;
			ps2_emu_queue(&m->state, AUX_ACK);
			break;
		case AUX_RESET:
			m->mouse_sample_rate = 100;
			m->mouse_resolution = 2;
			m->mouse_status = 0;
			m->mouse_type = 0;
			ps2_emu_queue(&m->state, AUX_ACK);
			ps2_emu_queue(&m->state, 0xaa);
			ps2_emu_queue(&m->state, m->mouse_type);
			break;
		default:
			break;
		}
		break;
	case AUX_SET_SAMPLE:
		m->mouse_sample_rate = val;
		/* detect IMPS/2 or IMEX */
		switch(m->mouse_detect_state) {
		default:
		case 0:
			if (val == 200) {
				m->mouse_detect_state = 1;
			}
			break;
		case 1:
			if (val == 100) {
				m->mouse_detect_state = 2;
			} else if (val == 200) {
				m->mouse_detect_state = 3;
			} else {
				m->mouse_detect_state = 0;
			}
			break;
		case 2:
			if (val == 80) {
				m->mouse_type = 3; /* IMPS/2 */
			}
			m->mouse_detect_state = 0;
			break;
		case 3:
			if (val == 80) {
				m->mouse_type = 4; /* IMEX */
			}
			m->mouse_detect_state = 0;
			break;
		}
		ps2_emu_queue(&m->state, AUX_ACK);
		ps2_emu_set_write_cmd(&m->state, -1);
		break;
	case AUX_SET_RES:
		m->mouse_resolution = val;
		ps2_emu_queue(&m->state, AUX_ACK);
		ps2_emu_set_write_cmd(&m->state, -1);
		break;
	}

	vmm_spin_unlock_irqrestore(&m->lock, flags);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(ps2_emu_write_mouse);

int ps2_emu_mouse_fake_event(struct ps2_emu_mouse *m)
{
	if (!m) {
		return VMM_EINVALID;
	}

	ps2_emu_mouse_event(m->mouse, 1, 0, 0, 0);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(ps2_emu_mouse_fake_event);

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
