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
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for standerd input/output
 */

#include <stdarg.h>
#include <arch_defterm.h>
#include <vmm_error.h>
#include <vmm_math.h>
#include <vmm_string.h>
#include <vmm_main.h>
#include <vmm_stdio.h>

/* NOTE: assuming sizeof(void *) == sizeof(int) */

#define PAD_RIGHT 1
#define PAD_ZERO 2
/* the following should be enough for 32 bit int */
#define PRINT_BUF_LEN 64

struct vmm_stdio_ctrl {
        vmm_spinlock_t lock;
        struct vmm_chardev *indev;
        struct vmm_chardev *outdev;
};

static struct vmm_stdio_ctrl stdio_ctrl;

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

int vmm_printchar(char **str, struct vmm_chardev *cdev, char c, bool block)
{
	int rc ;
	if (str && *str) {
		**str = c;
		++(*str);
		rc = VMM_OK;
	} else if (cdev) {
		rc = ((vmm_chardev_dowrite(cdev, (u8 *)&c, 0, sizeof(c), 
					   block)) ? VMM_OK : VMM_EFAIL);
	} else {
		while (((rc = arch_defterm_putc((u8)c)) == VMM_EFAIL) 
		       && block);
	}

	return rc;
}

void vmm_cputc(struct vmm_chardev *cdev, char ch)
{
	if (ch == '\n') {
		vmm_printchar(NULL, cdev, '\r', TRUE);
	}
	vmm_printchar(NULL, cdev, ch, TRUE);
}

void vmm_putc(char ch)
{
	vmm_cputc(stdio_ctrl.outdev, ch);
}

static void printc(char **str, struct vmm_chardev *cdev, char ch)
{
	if (str) {
		vmm_printchar(str, cdev, ch, TRUE);
	} else {
		vmm_cputc(cdev, ch);
	}
}

static int prints(char **out, struct vmm_chardev *cdev, 
		  const char *string, int width, int pad)
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
			printc(out, cdev, padchar);
			++pc;
		}
	}
	for (; *string; ++string) {
		printc(out, cdev, *string);
		++pc;
	}
	for (; width > 0; --width) {
		printc(out, cdev, padchar);
		++pc;
	}

	return pc;
}

static int printi(char **out, struct vmm_chardev *cdev, 
		  long long i, int b, int sg, int width, int pad, int letbase)
{
	char print_buf[PRINT_BUF_LEN];
	char *s;
	int t, neg = 0, pc = 0;
	unsigned long long u = i;

	if (i == 0) {
		print_buf[0] = '0';
		print_buf[1] = '\0';
		return prints(out, cdev, print_buf, width, pad);
	}

	if (sg && b == 10 && i < 0) {
		neg = 1;
		u = -i;
	}

	s = print_buf + PRINT_BUF_LEN - 1;
	*s = '\0';

	while (u) {
		t = vmm_umod64(u, b);
		if (t >= 10)
			t += letbase - '0' - 10;
		*--s = t + '0';
		u = vmm_udiv64(u, b);
	}

	if (neg) {
		if (width && (pad & PAD_ZERO)) {
			printc(out, cdev, '-');
			++pc;
			--width;
		} else {
			*--s = '-';
		}
	}

	return pc + prints(out, cdev, s, width, pad);
}

static int print(char **out, struct vmm_chardev *cdev, const char *format, va_list args)
{
	int width, pad;
	int pc = 0;
	char scr[2];
	long long tmp;

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
				pc += prints(out, cdev, s ? s : "(null)", width, pad);
				continue;
			}
			if (*format == 'd') {
				pc +=
				    printi(out, cdev, va_arg(args, int), 
					   10, 1, width, pad, '0');
				continue;
			}
			if (*format == 'x') {
				pc +=
				    printi(out, cdev, va_arg(args, unsigned int), 
					   16, 0, width, pad, 'a');
				continue;
			}
			if (*format == 'X') {
				pc +=
				    printi(out, cdev, va_arg(args, unsigned int), 
					   16, 0, width, pad, 'A');
				continue;
			}
			if (*format == 'u') {
				pc +=
				    printi(out, cdev, va_arg(args, unsigned int), 
					   10, 0, width, pad, 'a');
				continue;
			}
			if (*format == 'l' && *(format + 1) == 'l') {
				if (sizeof(long long) == 
						sizeof(unsigned long)) {
					tmp = va_arg(args, unsigned long long);
				} else {
					((unsigned long *)&tmp)[0] = 
						va_arg(args, unsigned long);
					((unsigned long *)&tmp)[1] = 
						va_arg(args, unsigned long);
				}
				if (*(format + 2) == 'u') {
					format += 2;
					pc += printi(out, cdev, tmp,
						10, 0, width, pad, 'a');
				} else if (*(format + 2) == 'x') {
					format += 2;
					pc += printi(out, cdev, tmp,
						16, 0, width, pad, 'a');
				} else if (*(format + 2) == 'X') {
					format += 2;
					pc += printi(out, cdev, tmp,
						16, 0, width, pad, 'A');
				} else {
					format += 1;
					pc += printi(out, cdev, tmp,
						10, 1, width, pad, '0');
				}
				continue;
			}
			if (*format == 'c') {
				/* char are converted to int then pushed on the stack */
				scr[0] = va_arg(args, int);
				scr[1] = '\0';
				pc += prints(out, cdev, scr, width, pad);
				continue;
			}
		} else {
out:
			printc(out, cdev, *format);
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
	retval = print(NULL, stdio_ctrl.outdev, format, args);
	va_end(args);
	return retval;
}

int vmm_sprintf(char *out, const char *format, ...)
{
	va_list args;
	int retval;
	va_start(args, format);
	retval = print(&out, stdio_ctrl.outdev, format, args);
	va_end(args);
	return retval;
}

int vmm_cprintf(struct vmm_chardev *cdev, const char *format, ...)
{
	va_list args;
	int retval;
	va_start(args, format);
	retval = print(NULL, cdev, format, args);
	va_end(args);
	return retval;
}

int vmm_panic(const char *format, ...)
{
	va_list args;
	int retval;
	va_start(args, format);
	retval = print(NULL, stdio_ctrl.outdev, format, args);
	va_end(args);
	vmm_hang();
	return retval;
}

int vmm_scanchar(char **str, struct vmm_chardev *cdev, char *c, bool block)
{
	int rc;

	if (str && *str) {
		*c = **str;
		++(*str);
		rc = VMM_OK;
	} else if (cdev) {
		rc = (vmm_chardev_doread(cdev, (u8 *)c, 0, sizeof(char), 
					 block) ? VMM_OK : VMM_EFAIL);
	} else {
		while (((rc = arch_defterm_getc((u8 *)c)) == VMM_EFAIL) 
		       && block);
	}

	return rc;
}

char vmm_getc(void)
{
	char ch = 0;
	vmm_scanchar(NULL, stdio_ctrl.indev, &ch, TRUE);
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
	bool add_ch, del_ch, to_left, to_right, to_start, to_end;
	u32 ite, pos = 0, count = 0;
	if (!s) {
		return NULL;
	}
	vmm_memset(s, 0, maxwidth);
	while (count < maxwidth) {
		to_left = FALSE;
		to_right = FALSE;
		to_start = FALSE;
		to_end = FALSE;
		add_ch = FALSE;
		del_ch = FALSE;
		if ((ch = vmm_getc()) == endchar) {
			break;
		}
		/* Note: we have to process all the required 
		 * ANSI escape seqences for special keyboard keys */
		if (vmm_isprintable(ch)) {
			add_ch = TRUE;
		} else if (ch == '\e') { /* Escape character */
			vmm_scanchar(NULL, stdio_ctrl.indev, &ch, TRUE);
			vmm_scanchar(NULL, stdio_ctrl.indev, &ch1, TRUE);
			if (ch == '[') {
				if (ch1 == 'A') { /* Up Key */
					/* Ignore it. */ 
					/* We will take care of it later. */
				} else if (ch1 == 'B') { /* Down Key */
					/* Ignore it. */
					/* We will take care of it later. */
				} else if (ch1 == 'C') { /* Right Key */
					to_right = TRUE;
				} else if (ch1 == 'D') { /* Left Key */
					to_left = TRUE;
				} else if (ch1 == 'H') { /* Home Key */
					to_start = TRUE;
				} else if (ch1 == 'F') { /* End Key */
					to_end = TRUE;
				} else if (ch1 == '3') {
					vmm_scanchar(NULL, stdio_ctrl.indev, &ch, TRUE);
					if (ch == '~') { /* Delete Key */
						if (pos < count) {
							to_right = TRUE;
							del_ch = TRUE;
						}
					}
				}
			} else if (ch == 'O') {
				if (ch1 == 'H') { /* Home Key */
					to_start = TRUE;
				} else if (ch1 == 'F') { /* End Key */
					to_end = TRUE;
				}
			}
		} else if (ch == 127){ /* Delete character */
			if (pos > 0) {
				del_ch = TRUE;
			}
		}
		if (to_left) {
			if (pos > 0) {
				vmm_putc('\e');
				vmm_putc('[');
				vmm_putc('D');
				pos--;
			}
		}
		if (to_right) {
			if (pos < count) {
				vmm_putc('\e');
				vmm_putc('[');
				vmm_putc('C');
				pos++;
			}
		}
		if (to_start) {
			for (ite = 0; ite < pos; ite++) {
				vmm_putc('\e');
				vmm_putc('[');
				vmm_putc('D');
			}
			pos = 0;
		}
		if (to_end) {
			for (ite = pos; ite < count; ite++) {
				vmm_putc('\e');
				vmm_putc('[');
				vmm_putc('C');
			}
			pos = count;
		}
		if (add_ch) {
			for (ite = 0; ite < (count - pos); ite++) {
				s[count - ite] = s[(count - 1) - ite];
			}
			for (ite = pos; ite < count; ite++) {
				vmm_putc(s[ite + 1]);
			}
			for (ite = pos; ite < count; ite++) {
				vmm_putc('\e');
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
				s[count] = '\0';
				pos--;
				count--;
			}
			vmm_putc('\e');
			vmm_putc('[');
			vmm_putc('D');
			for (ite = pos; ite < count; ite++) {
				vmm_putc(s[ite]);
			}
			vmm_putc(' ');
			for (ite = pos; ite <= count; ite++) {
				vmm_putc('\e');
				vmm_putc('[');
				vmm_putc('D');
			}
		}
	}
	s[count] = '\0';
	return s;
}

struct vmm_chardev *vmm_stdio_indevice(void)
{
	return stdio_ctrl.indev;
}

struct vmm_chardev *vmm_stdio_outdevice(void)
{
	return stdio_ctrl.outdev;
}

int vmm_stdio_change_indevice(struct vmm_chardev * cdev)
{
	if (!cdev) {
		return VMM_EFAIL;
	}

	vmm_spin_lock(&stdio_ctrl.lock);
	stdio_ctrl.indev = cdev;
	vmm_spin_unlock(&stdio_ctrl.lock);

	return VMM_OK;
}

int vmm_stdio_change_outdevice(struct vmm_chardev * cdev)
{
	if (!cdev) {
		return VMM_EFAIL;
	}

	vmm_spin_lock(&stdio_ctrl.lock);
	stdio_ctrl.outdev = cdev;
	vmm_spin_unlock(&stdio_ctrl.lock);

	return VMM_OK;
}

int __init vmm_stdio_init(void)
{
	/* Reset memory of control structure */
	vmm_memset(&stdio_ctrl, 0, sizeof(stdio_ctrl));

	/* Initialize lock */
	INIT_SPIN_LOCK(&stdio_ctrl.lock);

	/* Set current devices to NULL */
	stdio_ctrl.indev = NULL;
	stdio_ctrl.outdev = NULL;

	/* Initialize default serial terminal (board specific) */
	return arch_defterm_init();
}
