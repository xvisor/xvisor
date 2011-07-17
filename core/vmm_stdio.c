/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_stdio.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for standerd input/output
 */

#include <stdarg.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_main.h>
#include <vmm_board.h>
#include <vmm_sections.h>
#include <vmm_stdio.h>

/* NOTE: assuming sizeof(void *) == sizeof(int) */

#define PAD_RIGHT 1
#define PAD_ZERO 2
/* the following should be enough for 32 bit int */
#define PRINT_BUF_LEN 16

vmm_stdio_ctrl_t stdio_ctrl;

bool vmm_iscontrol(char c)
{
	return ((0 <= c) && (c < 32)) ? TRUE : FALSE;
}

bool vmm_isprintable(char c)
{
	if (((31 < c) && (c < 127)) || 
	   (c == '\r') ||
	   (c == '\n') ||
	   (c == '\t')) {
		return TRUE;
	}
	return FALSE;
}

int vmm_printchar(char **str, char c, bool block)
{
	int rc = VMM_OK;
	if (str) {
		**str = c;
		++(*str);
	} else {
		if (stdio_ctrl.cdev) {
			while (!vmm_chardev_dowrite(stdio_ctrl.cdev, 
							(u8 *)&c, 0, sizeof(c))) {
				if (!block) {
					rc = VMM_EFAIL;
					break;
				}
			}
		} else {
			while ((rc = vmm_defterm_putc((u8)c))) {
				if (!block) {
					break;
				}
			}
		}
	}
	return rc;
}

void vmm_putc(char ch)
{
	if (ch == '\n') {
		vmm_printchar(NULL, '\r', TRUE);
	}
	vmm_printchar(NULL, ch, TRUE);
}

static void printc(char **str, char ch)
{
	if (str) {
		vmm_printchar(str, ch, TRUE);
	} else {
		vmm_putc(ch);
	}
}

static int prints(char **out, const char *string, int width, int pad)
{
	int pc = 0;
	char padchar = ' ';

	if (width > 0) {
		int len = 0;
		const char *ptr;
		for (ptr = string; *ptr; ++ptr)
			++len;
		if (len >= width)
			width = 0;
		else
			width -= len;
		if (pad & PAD_ZERO)
			padchar = '0';
	}
	if (!(pad & PAD_RIGHT)) {
		for (; width > 0; --width) {
			printc(out, padchar);
			++pc;
		}
	}
	for (; *string; ++string) {
		printc(out, *string);
		++pc;
	}
	for (; width > 0; --width) {
		printc(out, padchar);
		++pc;
	}

	return pc;
}

static int printi(char **out, int i, int b, int sg, int width, int pad,
		  int letbase)
{
	char print_buf[PRINT_BUF_LEN];
	char *s;
	int t, neg = 0, pc = 0;
	unsigned int u = i;

	if (i == 0) {
		print_buf[0] = '0';
		print_buf[1] = '\0';
		return prints(out, print_buf, width, pad);
	}

	if (sg && b == 10 && i < 0) {
		neg = 1;
		u = -i;
	}

	s = print_buf + PRINT_BUF_LEN - 1;
	*s = '\0';

	while (u) {
		t = u % b;
		if (t >= 10)
			t += letbase - '0' - 10;
		*--s = t + '0';
		u /= b;
	}

	if (neg) {
		if (width && (pad & PAD_ZERO)) {
			printc(out, '-');
			++pc;
			--width;
		} else {
			*--s = '-';
		}
	}

	return pc + prints(out, s, width, pad);
}

static int print(char **out, const char *format, va_list args)
{
	int width, pad;
	int pc = 0;
	char scr[2];

	for (; *format != 0; ++format) {
		if (*format == '%') {
			++format;
			width = pad = 0;
			if (*format == '\0')
				break;
			if (*format == '%')
				goto out;
			if (*format == '-') {
				++format;
				pad = PAD_RIGHT;
			}
			while (*format == '0') {
				++format;
				pad |= PAD_ZERO;
			}
			for (; *format >= '0' && *format <= '9'; ++format) {
				width *= 10;
				width += *format - '0';
			}
			if (*format == 's') {
				char *s = va_arg(args, char *);
				pc += prints(out, s ? s : "(null)", width, pad);
				continue;
			}
			if (*format == 'd') {
				pc +=
				    printi(out, va_arg(args, int), 10, 1, width,
					   pad, 'a');
				continue;
			}
			if (*format == 'x') {
				pc +=
				    printi(out, va_arg(args, int), 16, 0, width,
					   pad, 'a');
				continue;
			}
			if (*format == 'X') {
				pc +=
				    printi(out, va_arg(args, int), 16, 0, width,
					   pad, 'A');
				continue;
			}
			if (*format == 'u') {
				pc +=
				    printi(out, va_arg(args, unsigned int), 10,
					   0, width, pad, 'a');
				continue;
			}
			if (*format == 'c') {
				/* char are converted to int then pushed on the stack */
				scr[0] = va_arg(args, int);
				scr[1] = '\0';
				pc += prints(out, scr, width, pad);
				continue;
			}
		} else {
out:
			printc(out, *format);
			++pc;
		}
	}
	if (out)
		**out = '\0';
	return pc;
}

int vmm_printf(const char *format, ...)
{
	va_list args;
	int retval;
	va_start(args, format);
	retval = print(NULL, format, args);
	va_end(args);
	return retval;
}

int vmm_sprintf(char *out, const char *format, ...)
{
	va_list args;
	int retval;
	va_start(args, format);
	retval = print(&out, format, args);
	va_end(args);
	return retval;
}

int vmm_panic(const char *format, ...)
{
	va_list args;
	int retval;
	va_start(args, format);
	retval = print(NULL, format, args);
	va_end(args);
	vmm_hang();
	return retval;
}

int vmm_scanchar(char **str, char *c, bool block)
{
	u8 ch = 0;
	bool got_input = FALSE;
	int rc = VMM_OK;
	if (str) {
		*c = **str;
		++(*str);
	} else {
		got_input = FALSE;
		if (stdio_ctrl.cdev) {
			while (!got_input) {
				if (!vmm_chardev_doread(stdio_ctrl.cdev,
						&ch, 0, sizeof(ch))) {
					if (!block) {
						rc = VMM_EFAIL;
						break;
					}
				} else {
					got_input = TRUE;
				}
			}
		} else {
			while (!got_input) {
				if ((rc = vmm_defterm_getc(&ch))) {
					if (!block) {
						break;
					}
				} else {
					got_input = TRUE;
				}
			}
		}
		if (!got_input) {
			ch = 0;
		}
		*c = ch;
	}
	return rc;
}

char vmm_getc(void)
{
	char ch = 0;
	vmm_scanchar(NULL, &ch, TRUE);
	if (ch == '\r') {
		ch = '\n';
	}
	if (vmm_isprintable(ch)) {
		vmm_putc(ch);
	}
	return ch;
}

char *vmm_gets(char *s, int maxwidth, char endchar)
{
	char ch, ch1;
	bool add_ch, del_ch;
	u32 ite, pos = 0, count = 0;
	if (!s) {
		return NULL;
	}
	vmm_memset(s, 0, maxwidth);
	while (count < maxwidth) {
		add_ch = FALSE;
		del_ch = FALSE;
		if ((ch = vmm_getc()) == endchar) {
			break;
		}
		if (vmm_isprintable(ch)) {
			add_ch = TRUE;
		} else if (ch == 27) { /* Escape character */
			vmm_scanchar(NULL, &ch, TRUE);
			vmm_scanchar(NULL, &ch1, TRUE);
			if (ch == '[') {
				if (ch1 == 'A') { /* Up Key */
					/* Ignore it. */ 
					/* We will take care of it later. */
				} else if (ch1 == 'B') { /* Down Key */
					/* Ignore it. */
					/* We will take care of it later. */
				} else if (ch1 == 'C') { /* Right Key */
					if (pos < count) {
						vmm_putc(27);
						vmm_putc('[');
						vmm_putc('C');
						pos++;
					}
				} else if (ch1 == 'D') { /* Left Key */
					if (pos > 0) {
						vmm_putc(27);
						vmm_putc('[');
						vmm_putc('D');
						pos--;
					}
				} else if (ch1 == '3') {
					vmm_scanchar(NULL, &ch, TRUE);
					if (ch == '~') { /* Delete Key */
						if (pos < count) {
							vmm_putc(27);
							vmm_putc('[');
							vmm_putc('C');
							pos++;
							del_ch = TRUE;
						}
					}
				}
			}
		} else if (ch == 127){ /* Delete character */
			if (pos > 0) {
				del_ch = TRUE;
			}
		}
		if (add_ch) {
			for (ite = 0; ite < (count - pos); ite++) {
				s[count - ite] = s[(count - 1) - ite];
			}
			for (ite = pos; ite < count; ite++) {
				vmm_putc(s[ite + 1]);
			}
			for (ite = pos; ite < count; ite++) {
				vmm_putc(27);
				vmm_putc('[');
				vmm_putc('D');
			}
			s[pos] = ch;
			count++;
			pos++;
		}
		if (del_ch) {
			if (pos > 0) {
				for (ite = pos; ite < count; ite++) {
					s[ite - 1] = s[ite];
				}
				pos--;
				count--;
			}
			vmm_putc(27);
			vmm_putc('[');
			vmm_putc('D');
			for (ite = pos; ite < count; ite++) {
				vmm_putc(s[ite]);
			}
			vmm_putc(' ');
			for (ite = pos; ite <= count; ite++) {
				vmm_putc(27);
				vmm_putc('[');
				vmm_putc('D');
			}
		}
	}
	s[count] = '\0';
	return s;
}

vmm_chardev_t *vmm_stdio_device(void)
{
	return stdio_ctrl.cdev;
}

int vmm_stdio_change_device(vmm_chardev_t * cdev)
{
	if (!cdev) {
		return VMM_EFAIL;
	}

	vmm_spin_lock(&stdio_ctrl.lock);
	stdio_ctrl.cdev = cdev;
	vmm_spin_unlock(&stdio_ctrl.lock);

	return VMM_OK;
}

int vmm_stdio_init(void)
{
	/* Reset memory of control structure */
	vmm_memset(&stdio_ctrl, 0, sizeof(stdio_ctrl));

	/* Initialize lock */
	INIT_SPIN_LOCK(&stdio_ctrl.lock);

	/* Set current device to NULL */
	stdio_ctrl.cdev = NULL;

	/* Initialize default serial terminal (board specific) */
	return vmm_defterm_init();
}
