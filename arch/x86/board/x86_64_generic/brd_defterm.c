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
#include <vmm_completion.h>
#include <vmm_params.h>
#include <libs/vtemu.h>
#include <libs/fifo.h>
#include <drv/input.h>

struct defterm_ops {
	int (*putc)(u8 ch);
	int (*getc)(u8 *ch);
	int (*init)(struct vmm_devtree_node *node);
};

/*
 * These define our textpointer, our background and foreground
 * colors (attributes), and x and y cursor coordinates
 */
static u16 *textmemptr;
static u32 attrib = 0x0E;
static u32 csr_x = 0, csr_y = 0;
static char esc_seq[16] = { 0 };
static u32 esc_seq_count = 0;

#if defined(CONFIG_VTEMU)
static struct fifo *defterm_fifo;
static struct vmm_completion defterm_fifo_cmpl;
static u32 defterm_key_flags;
static struct input_handler defterm_hndl;
static bool defterm_key_handler_registered;
#endif

/* If set to 1, Xvisor will use VGA and attached keyboard as console */
static int use_stdio = 0;

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

static void scroll_up_byline(void)
{
	u8 *dest, *src;
	u32 copylen;
	u32 blank, temp;

	/*
	 * A blank is defined as a space... we need to give it
	 * backcolor too
	 */
	blank = (0x20 | (attrib << 8));


	/*
	 * Move the current text chunk that makes up the screen
	 * back in the buffer by a line.
	 */
	temp = csr_y - 25 + 1;

	dest = (u8 *)textmemptr;
	src = (u8 *)(textmemptr + temp * 80);
	copylen = ((25 - temp) * 80 * 2);

	while (copylen) {
		*dest = *src;
		copylen--;
		dest++;
		src++;
	}

	/*
	 * Finally, we set the chunk of memory that occupies
	 *  the last line of text to our 'blank' character
	 */
	memsetw(textmemptr + (25 - temp) * 80, blank, 80);
	csr_y = 25 - 1;
}

/* Scrolls the screen */
void scroll(void)
{
	/* Row 25 is the end, this means we need to scroll up */
	if (csr_y >= 25)
		scroll_up_byline();
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

	if (esc_seq_count) {
		esc_seq[esc_seq_count] = c;
		esc_seq_count++;
		if ((esc_seq_count == 2) && (esc_seq[1] == '[')) {
			/* Do nothing */
		} else if ((esc_seq_count == 3) &&
			   (esc_seq[1] == '[') &&
			   (esc_seq[2] == 'D')) {
			/* Move left */
			if (csr_x != 0) {
				csr_x--;
			}
			esc_seq_count = 0;
		} else if ((esc_seq_count == 3) &&
			   (esc_seq[1] == '[') &&
			   (esc_seq[2] == 'C')) {
			/* Move right */
			if (csr_x != 0) {
				csr_x++;
			}
			esc_seq_count = 0;
		} else {
			/* Ignore unknown escape sequences */
			esc_seq_count = 0;
		}
		goto done;
	}

	/* Handle a backspace, by moving the cursor back one space */
	if (c == '\e') {
		esc_seq_count = 1;
		esc_seq[0] = '\e';
		goto done;
	} else if (c == '\b') {
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

done:
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
static void init_console(struct vmm_devtree_node *node)
{
	settextcolor(15 /* White foreground */, 0 /* Black background */);
	textmemptr = (u16 *)vmm_host_iomap(0xB8000, 0x4000);
	cls();
}

static int unknown_defterm_putc(u8 ch)
{
	return VMM_EFAIL;
}

static int unknown_defterm_getc(u8 *ch)
{
	return VMM_EFAIL;
}

static int __init unknown_defterm_init(struct vmm_devtree_node *node)
{
	return VMM_ENODEV;
}

#if defined(CONFIG_VTEMU)

static int defterm_key_event(struct input_handler *ihnd, 
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
			vmm_completion_complete(&defterm_fifo_cmpl);
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

int arch_std_defterm_getc(u8 *ch)
{
	int rc;

	if (!defterm_key_handler_registered) {
		memset(&defterm_hndl, 0, sizeof(defterm_hndl));
		defterm_hndl.name = "defterm";
		defterm_hndl.evbit[0] |= BIT_MASK(EV_KEY);
		defterm_hndl.event = defterm_key_event;
		defterm_hndl.priv = NULL;

		rc = input_register_handler(&defterm_hndl); 
		if (rc) {
			return rc;
		}

		rc = input_connect_handler(&defterm_hndl); 
		if (rc) {
			return rc;
		}

		defterm_key_handler_registered = TRUE;
	}

	if (defterm_fifo) {
		/* Assume that we are always called from
		 * Orphan (or Thread) context hence we can
		 * sleep waiting for input characters.
		 */
		vmm_completion_wait(&defterm_fifo_cmpl);

		/* Try to dequeue from defterm fifo */
		if (!fifo_dequeue(defterm_fifo, ch)) {
			return VMM_ENOTAVAIL;
		}

		return VMM_OK;
	}

	return VMM_EFAIL;
}

#else

int arch_std_defterm_getc(u8 *ch)
{
	return VMM_EFAIL;
}

#endif

int arch_std_defterm_putc(u8 ch)
{
	putch(ch);

	return VMM_OK;
}

int __init arch_std_defterm_init(struct vmm_devtree_node *node)
{
	init_console(node);

#if defined(CONFIG_VTEMU)
	defterm_fifo = fifo_alloc(sizeof(u8), 128);
	if (!defterm_fifo) {
		return VMM_ENOMEM;
	}
	INIT_COMPLETION(&defterm_fifo_cmpl);

	defterm_key_flags = 0;
	defterm_key_handler_registered = FALSE;
#endif

	return VMM_OK;
}

static struct defterm_ops stdio_ops = {
	.putc = arch_std_defterm_putc,
	.getc = arch_std_defterm_getc,
	.init = arch_std_defterm_init
};

#if defined(CONFIG_SERIAL_8250_UART)

#include <drv/8250-uart.h>

static struct uart_8250_port uart8250_port;

static int uart8250_defterm_putc(u8 ch)
{
	if (!uart_8250_lowlevel_can_putc(&uart8250_port)) {
		return VMM_EFAIL;
	}
	uart_8250_lowlevel_putc(&uart8250_port, ch);
	return VMM_OK;
}

static int uart8250_defterm_getc(u8 *ch)
{
	if (!uart_8250_lowlevel_can_getc(&uart8250_port)) {
		return VMM_EFAIL;
	}
	*ch = uart_8250_lowlevel_getc(&uart8250_port);
	return VMM_OK;
}

static int __init uart8250_defterm_init(struct vmm_devtree_node *node)
{
	int rc;
	physical_addr_t addr;

	if (vmm_devtree_read_physaddr(node,
			VMM_DEVTREE_REG_ATTR_NAME, &addr)) {
		uart8250_port.base = 0x3f8;
	} else {
		uart8250_port.base = (virtual_addr_t)addr;
	}

	rc = vmm_devtree_clock_frequency(node,
				&uart8250_port.input_clock);
	if (rc) {
		return rc;
	}

	if (vmm_devtree_read_u32(node, "baudrate",
				 &uart8250_port.baudrate)) {
		uart8250_port.baudrate = 115200;
	}

	if (vmm_devtree_read_u32(node, "reg-shift",
				 &uart8250_port.reg_shift)) {
		uart8250_port.reg_shift = 2;
	}

	if (vmm_devtree_read_u32(node, "reg-io-width",
				 &uart8250_port.reg_width)) {
		uart8250_port.reg_width = 1;
	}

	uart_8250_lowlevel_init(&uart8250_port);

	return VMM_OK;
}

static struct defterm_ops uart8250_ops = {
	.putc = uart8250_defterm_putc,
	.getc = uart8250_defterm_getc,
	.init = uart8250_defterm_init
};

#else

static struct defterm_ops uart8250_ops = {
	.putc = unknown_defterm_putc,
	.getc = unknown_defterm_getc,
	.init = unknown_defterm_init
};

#endif

#ifdef ARCH_HAS_DEFTERM_EARLY_PRINT
int init_early_vga_console(void)
{
	settextcolor(15 /* White foreground */, 0 /* Black background */);
	textmemptr = (u16 *)(0xB8000UL);
	cls();

	return 0;
}

void arch_defterm_early_putc(u8 ch)
{
	putch(ch);
}

static int __init setup_early_print(char *buf)
{
	init_early_vga_console();

	return 0;
}

vmm_early_param("earlyprint", setup_early_print);
#endif

static int __init set_default_console(char *buf)
{
	use_stdio = 1;

	return 0;
}

vmm_early_param("console", set_default_console);
/*-------------- UART DEFTERM --------------- */
static struct vmm_devtree_nodeid defterm_devid_table[] = {
	{ .compatible = "ns8250", .data = &uart8250_ops },
	{ .compatible = "ns16450", .data = &uart8250_ops },
	{ .compatible = "ns16550a", .data = &uart8250_ops },
	{ .compatible = "ns16550", .data = &uart8250_ops },
	{ .compatible = "ns16750", .data = &uart8250_ops },
	{ .compatible = "ns16850", .data = &uart8250_ops },
	{ /* end of list */ },
};

static const struct defterm_ops *ops = NULL;

int arch_defterm_putc(u8 ch)
{
	return (ops) ? ops->putc(ch) : unknown_defterm_putc(ch);
}

int arch_defterm_getc(u8 *ch)
{
	return (ops) ? ops->getc(ch) : unknown_defterm_getc(ch);
}

int __init arch_defterm_init(void)
{
	int rc;
	const char *attr;
	struct vmm_devtree_node *node;
	const struct vmm_devtree_nodeid *nodeid;

	if (use_stdio) {
		ops = &stdio_ops;
		return ops->init(NULL);
	}

	/* Find choosen console node */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_CHOSEN_NODE_NAME);
	if (!node) {
		return VMM_ENODEV;
	}

	rc = vmm_devtree_read_string(node,
			VMM_DEVTREE_CONSOLE_ATTR_NAME, &attr);
	if (rc) {
		return rc;
	}

	node = vmm_devtree_getnode(attr);
	if (!node) {
		return VMM_ENODEV;
	}

	/* Find appropriate defterm ops */
	nodeid = vmm_devtree_match_node(defterm_devid_table, node);
	if (nodeid) {
		ops = nodeid->data;
	} else {
		return VMM_ENODEV;
	}

	return (ops) ? ops->init(node) : unknown_defterm_init(node);
}
