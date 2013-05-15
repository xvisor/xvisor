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
 * @file brd_defterm.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief text-mode VGA console as default terminal.
 */

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_compiler.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <input/vmm_input.h>
#include <libs/vtemu.h>
#include <libs/fifo.h>

/* 
 * These define our textpointer, our background and foreground
 * colors (attributes), and x and y cursor coordinates 
 */
static u16 *textmemptr;
static u32 attrib = 0x0E;
static u32 csr_x = 0, csr_y = 0;

#if defined(CONFIG_VTEMU)
static struct fifo *defterm_fifo;
static u32 defterm_key_flags;
static struct vmm_input_handler defterm_hndl;
static bool defterm_key_handler_registered;
#endif

static u16 *memsetw(u16 *dest, u16 val, size_t count)
{
	u16 *temp = (u16 *)dest;

	for( ; count != 0; count--) {
		*temp++ = val;
	}

	return dest;
}

static void update_cursor(void)
{
	u16 pos = (csr_y * 80) + csr_x;
 
	/* cursor LOW port to vga INDEX register */
	vmm_outb(0x0F, 0x3D4);
	vmm_outb((u8)(pos & 0xFF), 0x3D5);

	/* cursor HIGH port to vga INDEX register */
	vmm_outb(0x0E, 0x3D4);
	vmm_outb((u8)((pos >> 8) & 0xFF), 0x3D5);
}

/* Scrolls the screen */
void scroll(void)
{
	u32 blank, temp;

	/* 
	 * A blank is defined as a space... we need to give it
	 * backcolor too 
	 */
	blank = (0x20 | (attrib << 8));

	/* Row 25 is the end, this means we need to scroll up */
	if (csr_y >= 25) {
		/*
		 * Move the current text chunk that makes up the screen
		 * back in the buffer by a line.
		 */
		temp = csr_y - 25 + 1;
		memcpy(textmemptr, textmemptr + temp * 80, ((25 - temp) * 80 * 2));

		/* 
		 * Finally, we set the chunk of memory that occupies
		 *  the last line of text to our 'blank' character 
		 */
		memsetw(textmemptr + (25 - temp) * 80, blank, 80);
		csr_y = 25 - 1;
	}
}

/* Clears the screen */
static void cls()
{
	u32 i, blank;

	/* 
	 * Again, we need the 'short' that will be used to
	 *  represent a space with color 
	 */
	blank = 0x20 | (attrib << 8);

	/* Sets the entire screen to spaces in our current color */
	for(i = 0; i < 25; i++) {
		memsetw (textmemptr + i * 80, blank, 80);
	}

	/* 
	 * Update out virtual cursor, and then move the
	 *  hardware cursor 
	 */
	csr_x = 0;
	csr_y = 0;
}

/* Puts a single character on the screen */
static void putch(unsigned char c)
{
	u16 *where;
	u32 att = attrib << 8;

	/* Handle a backspace, by moving the cursor back one space */
	if (c == '\b') {
		if (csr_x != 0) {
			csr_x--;
		}
	} else if (c == '\t') {
		/* 
		 * Handles a tab by incrementing the cursor's x, but only
		 * to a point that will make it divisible by 8 
		 */
		csr_x = (csr_x + 8) & ~(8 - 1);
	} else if (c == '\r') {
		/* 
		 * Handles a 'Carriage Return', which simply brings the
		 * cursor back to the margin 
		 */
		csr_x = 0;
	} else if (c == '\n') {
		/* 
		 * We handle our newlines the way DOS and the BIOS do: we
		 *  treat it as if a 'CR' was also there, so we bring the
		 *  cursor to the margin and we increment the 'y' value 
		 */
		csr_x = 0;
		csr_y++;
	} else if (c >= ' ') {
		/* 
		 * Any character greater than and including a space, is a
		 *  printable character. The equation for finding the index
		 *  in a linear chunk of memory can be represented by:
		 *  Index = [(y * width) + x] 
		 */
		where = textmemptr + (csr_y * 80 + csr_x);
		*where = c | att;	/* Character AND attributes: color */
		csr_x++;
	} 

	/* 
	 * If the cursor has reached the edge of the screen's width, we
	 * insert a new line in there 
	 */
	if (csr_x >= 80) {
		csr_x = 0;
		csr_y++;
	}

	/* Scroll the screen if needed, and finally move the cursor */
	scroll();

	/* Update cursor location */
	update_cursor();
}

/* Sets the forecolor and backcolor that we will use */
static void settextcolor(u8 forecolor, u8 backcolor)
{
        /* 
	 * Top 4 bytes are the background, bottom 4 bytes
	 * are the foreground color 
	 */
        attrib = (backcolor << 4) | (forecolor & 0x0F);
}

/* Sets our text-mode VGA pointer, then clears the screen for us */
static void init_console(void)
{
        settextcolor(15 /* White foreground */, 0 /* Black background */);
	textmemptr = (u16 *)vmm_host_iomap(0xB8000, 0x4000);
	cls();
}

#if defined(CONFIG_VTEMU)

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
	defterm_fifo = fifo_alloc(sizeof(u8), 128);
	if (!defterm_fifo) {
		return VMM_ENOMEM;
	}

	defterm_key_flags = 0;
	defterm_key_handler_registered = FALSE;
#endif

	return VMM_OK;
}
