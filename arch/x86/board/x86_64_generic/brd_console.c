/**
 * Copyright (c) 2010-20 Himanshu Chauhan.
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
 * @file brd_console.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief main source file for serial port in qemu-mips board.
 */

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_compiler.h>
#include <vmm_host_aspace.h>
#include <input/vmm_input.h>
#include <libs/vtemu.h>
#include <libs/fifo.h>

extern void cls();
extern void putch(unsigned char c);
extern void settextcolor(u8 forecolor, u8 backcolor);
extern void init_console(void);

#if defined(CONFIG_VTEMU)
static struct fifo *defterm_fifo;
static u32 defterm_key_flags;
static struct vmm_input_handler defterm_hndl;
static bool defterm_key_handler_registered;

static int defterm_key_event(struct vmm_input_handler *ihnd, 
			     struct vmm_input_dev *idev, 
			     unsigned int type, unsigned int code, int value)
{
	int rc, i, len;
	char str[16];
	u32 key_flags;

	if (value) { /* value=1 (key-up) or value=2 (auto-repeat) */
		/* Update input key flags */
		key_flags = vtemu_key2flags(code);
		if ((key_flags & VTEMU_KEYFLAG_LOCKS) &&
		    (defterm_key_flags & key_flags)) {
			defterm_key_flags &= ~key_flags;
		} else {
			defterm_key_flags |= key_flags;
		}

		/* Retrive input key string */
		rc = vtemu_key2str(code, defterm_key_flags, str);
		if (rc) {
			return VMM_OK;
		}

		/* Add input key string to input buffer */
		len = strlen(str);
		for (i = 0; i < len; i++) {
			fifo_enqueue(defterm_fifo, &str[i], TRUE);
		}
	} else { /* value=0 (key-down) */
		/* Update input key flags */
		key_flags = vtemu_key2flags(code);
		if (!(key_flags & VTEMU_KEYFLAG_LOCKS)) {
			defterm_key_flags &= ~key_flags;
		}
	}

	return VMM_OK;
}

int arch_defterm_getc(u8 *ch)
{
	int rc;

	if (!defterm_key_handler_registered) {
		memset(&defterm_hndl, 0, sizeof(defterm_hndl));
		defterm_hndl.name = "defterm";
		defterm_hndl.evbit[0] |= BIT_MASK(EV_KEY);
		defterm_hndl.event = defterm_key_event;
		defterm_hndl.priv = NULL;

		rc = vmm_input_register_handler(&defterm_hndl); 
		if (rc) {
			return rc;
		}

		rc = vmm_input_connect_handler(&defterm_hndl); 
		if (rc) {
			return rc;
		}

		defterm_key_handler_registered = TRUE;
	}

	if (defterm_fifo && !fifo_isempty(defterm_fifo)) {
		if (!fifo_dequeue(defterm_fifo, ch)) {
			return VMM_ENOTAVAIL;
		}

		return VMM_OK;
	}

	return VMM_EFAIL;
}

#else

int arch_defterm_getc(u8 *ch)
{
	return VMM_EFAIL;
}

#endif

int arch_defterm_putc(u8 ch)
{
        putch(ch);

	return VMM_OK;
}

int __init arch_defterm_init(void)
{
        init_console();

#if defined(CONFIG_VTEMU)
	defterm_fifo = fifo_alloc(128, sizeof(u8));
	if (!defterm_fifo) {
		return VMM_ENOMEM;
	}

	defterm_key_flags = 0;
	defterm_key_handler_registered = FALSE;
#endif

	return VMM_OK;
}
