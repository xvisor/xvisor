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
 * @file stringlib.c
 * @author Anup Patel (anup@brainfault.org)
 * @author Jean-Christophe DUBOIS (jcd@tribudubois.net)
 * @brief implementation of string library
 */

#include <vmm_types.h>
#include <vmm_host_io.h>
#include <vmm_limits.h>
#include <vmm_compiler.h>
#include <vmm_modules.h>
#include <libs/ctype.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>

#include <stdarg.h>

size_t strlen(const char *s)
{
	size_t ret = 0;
	/* search end of string */
	for (; *s != '\0'; s++, ret++);
	return ret;
}

size_t strnlen(const char *s, size_t n)
{
	size_t ret = 0;
	if (n > 0) {
		/* search end of string with limit*/
		for (; (*s != '\0') && (ret != n); s++, ret++);
	}
	return ret;
}

char *strcpy(char *dest, const char *src)
{
	char *save = dest;
	/* copy string */
	for (; (*dest = *src) != '\0'; dest++, src++);
	return save;
}

char *strncpy(char *dest, const char *src, size_t n)
{
	char *save = dest;
	if (n > 0) {
		/* copy string with limit */
		for (; n && ((*dest = *src) != '\0'); dest++, src++, n--);
	}
	return save;
}

size_t strlcpy(char *dest, const char *src, size_t n)
{
	size_t ret = 0;
	if (n > 0) {
		/* copy string with limit */
		for (n--; n && ((*dest = *src) != '\0'); dest++, src++,
			ret ++, n--);
		/* add a trailing 0 */
		if (n == 0) *dest = '\0';
	}
	/* increase ret until end of src */
	for (; *src != '\0'; src++, ret++);
	return ret;
}

char *strcat(char *dest, const char *src)
{
	char *save = dest;
	/* move to end of dest */
	for (; *dest != '\0'; dest++) ;
	/* copy src to end of dest */
	for (; (*dest = *src) != '\0'; dest++, src++);
	return save;
}

char *strncat(char *dest, const char *src, size_t n)
{
	char *save = dest;
	if (n > 0) {
		/* move to end of dest */
		for (; n && (*dest != '\0'); dest++, n--) ;
		/* copy src to end of dest */
		for (; n && ((*dest = *src) != '\0'); dest++, src++, n--);
	}
	return save;
}

size_t strlcat(char *dest, const char *src, size_t n)
{
	size_t ret = 0;
	if (n > 0) {
		/* move to end of dest */
		for (n--; n && (*dest != '\0'); dest++, ret++, n--);
		/* copy src to end of dest */
		for (; n && ((*dest = *src) != '\0'); dest++, src++, ret++, n--);
		/* add a trailing 0 */
		if (n == 0) *dest = '\0';
	}
	/* increase ret until end of src */
	for (; *src != '\0'; src++, ret++);
	return ret;
}

int strcmp(const char *a, const char *b)
{
	/* search first diff or end of string */
	for (; *a == *b && *a != '\0'; a++, b++);
	return *a - *b;
}

int strncmp(const char *a, const char *b, size_t n)
{
	if (n > 0) {
		/* search first diff or end of string */
		for (n--; n != 0 && *a == *b && *a != '\0'; a++, b++, n--);
		return *a - *b;
	} else {
		return 0;
	}
}

int strcasecmp(const char *s1, const char *s2)
{
	int c1, c2;

	do {
		c1 = tolower(*s1++);
		c2 = tolower(*s2++);
	} while (c1 == c2 && c1 != 0);
	return c1 - c2;
}

char *strchr(const char *s, int c)
{
	/* search for the c char starting left */
	for (; *s != (char)c && *s != '\0'; s++);
	return *s == '\0' ? NULL : (char *)s;
}

char *strrchr(const char *s, int c)
{
	const char *p = s + strlen(s);

	/* search for the c char starting right */
	for (; *p != (char)c && p != s; p--);
	return (*p != c) ? NULL : (char *)p;
}

char *strnchr(const char *s, size_t n, int c)
{
	for (; n && *s != (char)c && *s != '\0'; s++, n--);
	return *s == '\0' || n == 0 ? NULL : (char *)s;
}

const char *strstr(const char *string, const char *substring)
{
	const char *a, *b;

	/* First scan quickly through the two strings looking for a
	 * single-character match.  When it's found, then compare the
	 * rest of the substring.
	 */

	b = substring;
	if (*b == 0) {
		return string;
	}
	for ( ; *string != 0; string += 1) {
		if (*string != *b) {
			continue;
		}
		a = string;
		while (1) {
			if (*b == 0) {
				return string;
			}
			if (*a++ != *b++) {
				break;
			}
		}
		b = substring;
	}

	return NULL;
}

void str2lower(char *s)
{
	if (s) {
		for (; *s != '\0'; s++) {
			if ('A' <= *s && *s <= 'Z') {
				*s = (*s - 'A') + 'a';
			}
		}
	}
}

void str2upper(char *s)
{
	if (s) {
		for (; *s != '\0'; s++) {
			if ('a' <= *s && *s <= 'z') {
				*s = (*s - 'a') + 'A';
			}
		}
	}
}

long long strtoll(const char *s, char **endptr, int base)
{
	long long val;
	int mult = 1;

	if (base < 0 || base == 1 || base > 16) {
		if (endptr) {
			*endptr = (char *)s;
		}
		return 0;
	}

	/* skip spaces and tabs */
	s = skip_spaces(s);

	/* handle sign */
	if (*s == '-') {
		mult = -1;
		s++;
	} else if (*s == '+') {
		s++;
	}

	val = (long long)strtoull(s, endptr, base);

	val *= mult;

	return val;
}

long strtol(const char *s, char **endptr, int base)
{
	return strtoll(s, endptr, base);
}

int atoi(const char *s)
{
	return strtoll(s, NULL, 10);
}

unsigned long long strtoull(const char *s, char **endptr, int base)
{
	unsigned long long val = 0;
	unsigned int digit;

	if (base < 0 || base == 1 || base > 16) {
		if (endptr) {
			*endptr = (char *)s;
		}
		return 0;
	}

	/* skip spaces and tabs */
	s = skip_spaces(s);

	/* handle implicit base */
	if (*s == '0') {
		if (*(s+1) == 'x') {
			if (base == 0 || base == 16) {
				base = 16;
				s += 2;
			} else {
				if (endptr) {
					*endptr = (char *)s;
				}
				return 0;
			}
		} else if (base == 0) {
			base = 8;
			s++;
		}
	}

	if (base == 0) {
		base = 10;
	}

	for (; *s != '\0'; s++) {
		if ('A' <= *s && *s <= 'F') {
			digit = 10 + (*s - 'A');
		} else if ('a' <= *s && *s <= 'f') {
			digit = 10 + (*s - 'a');
		} else if ('0' <= *s && *s <= '9') {
			digit = *s - '0';
		} else {
			break;
		}

		if (digit >= base) {
			break;
		}

		val = val * base + digit;
	}

	if (endptr) {
		*endptr = (char *)s;
	}

	return val;
}

unsigned long strtoul(const char *s, char **endptr, int base)
{
	return strtoull(s, endptr, base);
}

const char *_parse_integer_fixup_radix(const char *s, unsigned int *base)
{
	if (*base == 0) {
		if (s[0] == '0') {
			if (_tolower(s[1]) == 'x' && isxdigit(s[2]))
				*base = 16;
			else
				*base = 8;
		} else
			*base = 10;
	}
	if (*base == 16 && s[0] == '0' && _tolower(s[1]) == 'x')
		s += 2;
	return s;
}

int str2ipaddr(unsigned char *ipaddr, const char *str)
{
	unsigned long long tmp;
	int i;

	tmp = 0;

	for (i = 0; i < 4; i++, ipaddr++) {
		tmp = strtoull(str, (char **)&str, 10);

		if (tmp > 255) {
			return 0;
		}

		if (*str == '.') {
			str++;
		} else if (i != 3) {
			return 0;
		}

		*ipaddr = (unsigned char)tmp;
	}

	return 1;
}

char *strpbrk(const char *cs, const char *ct)
{
	char *ret = NULL;

	for (; *ct != '\0' && (ret = strchr(cs, *ct)) == NULL; ct++);

	return ret;
}

char *strsep(char **s, const char *ct)
{
	char *sbegin = *s;
	char *end;

	if (sbegin != NULL) {
		end = strpbrk(sbegin, ct);

		if (end) {
			*end = '\0';
			end++;
		}

		*s = end;
	}
	return sbegin;
}

#if !defined(ARCH_HAS_MEMCPY)
void *memcpy(void *dest, const void *src, size_t count)
{
	u8 *dst8 = (u8 *) dest;
	u8 *src8 = (u8 *) src;

	if (count & 1) {
		dst8[0] = src8[0];
		dst8 += 1;
		src8 += 1;
	}

	count /= 2;
	while (count--) {
		dst8[0] = src8[0];
		dst8[1] = src8[1];

		dst8 += 2;
		src8 += 2;
	}

	return dest;
}
#endif

void *memcpy_toio(void *dest, const void *src, size_t count)
{
	u8 *dst8 = (u8 *) dest;
	u8 *src8 = (u8 *) src;

	if (count & 1) {
		vmm_writeb(src8[0], &dst8[0]);
		dst8 += 1;
		src8 += 1;
	}

	count /= 2;
	while (count--) {
		vmm_writeb(src8[0], &dst8[0]);
		vmm_writeb(src8[1], &dst8[1]);

		dst8 += 2;
		src8 += 2;
	}

	return dest;
}

void *memcpy_fromio(void *dest, const void *src, size_t count)
{
	u8 *dst8 = (u8 *) dest;
	u8 *src8 = (u8 *) src;

	if (count & 1) {
		dst8[0] = vmm_readb(&src8[0]);
		dst8 += 1;
		src8 += 1;
	}

	count /= 2;
	while (count--) {
		dst8[0] = vmm_readb(&src8[0]);
		dst8[1] = vmm_readb(&src8[1]);

		dst8 += 2;
		src8 += 2;
	}

	return dest;
}

void *memmove(void *dest, const void *src, size_t count)
{
	u8 *dst8 = (u8 *) dest;
	const u8 *src8 = (u8 *) src;

	if (src8 > dst8) {
		if (count & 1) {
			dst8[0] = src8[0];
			dst8 += 1;
			src8 += 1;
		}

		count /= 2;
		while (count--) {
			dst8[0] = src8[0];
			dst8[1] = src8[1];

			dst8 += 2;
			src8 += 2;
		}
	} else {
		dst8 += count;
		src8 += count;

		if (count & 1) {
			dst8 -= 1;
			src8 -= 1;
			dst8[0] = src8[0];
		}

		count /= 2;
		while (count--) {
			dst8 -= 2;
			src8 -= 2;

			dst8[0] = src8[0];
			dst8[1] = src8[1];
		}
	}

	return dest;
}

#if !defined(ARCH_HAS_MEMSET)
void *memset(void *dest, int c, size_t count)
{
	u8 *dst8 = (u8 *) dest;
	u8 ch = (u8) c;

	if (count & 1) {
		dst8[0] = ch;
		dst8 += 1;
	}

	count /= 2;
	while (count--) {
		dst8[0] = ch;
		dst8[1] = ch;
		dst8 += 2;
	}

	return dest;
}
#endif

void *memset_io(void *dest, int c, size_t count)
{
	u8 *dst8 = (u8 *) dest;
	u8 ch = (u8) c;

	if (count & 1) {
		vmm_writeb(ch, &dst8[0]);
		dst8 += 1;
	}

	count /= 2;
	while (count--) {
		vmm_writeb(ch, &dst8[0]);
		vmm_writeb(ch, &dst8[1]);
		dst8 += 2;
	}

	return dest;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
	for (; *(char *)s1 == *(char *)s2 && n;  s1++, s2++, n--);
	return n == 0 ? 0 : *(char *)s1 - *(char *)s2;
}

void *memchr(const void *s, int c, size_t n)
{
	for (; *(const char *)s != (char)c && n; s++, n--);
	return n == 0 ? NULL : (void *)s;
}

int skip_atoi(const char **s)
{
	int i = 0;

	while (isdigit(**s))
		i = i*10 + *((*s)++) - '0';

	return i;
}

/**
 * skip_spaces - Removes leading whitespace from @str.
 * @str: The string to be stripped.
 *
 * Returns a pointer to the first non-whitespace character in @str.
 */
char *skip_spaces(const char *str)
{
	for (; isspace(*str); str++);
	return (char *)str;
}

/**
 * Calculate the length (in bytes) of the initial
 * segment of s which consists entirely of bytes
 * in accept.
 */
size_t strspn(const char* s, const char* accept)
{
	size_t n;
	const char* p;

	for(n = 0; *s; s++, n++) {
		for(p = accept; *p && *p != *s; p++)
			;
		if (!*p)
			break;
	}

	return n;
}

/**
 * Calculate the length of the initial segment of s which
 * consists entirely of bytes not in reject.
 */
size_t strcspn(const char *s, const char *reject)
{
	size_t ret = 0;

	while(*s)
		if(strchr(reject,*s))
			return ret;
		else
			s++,ret++;

	return ret;
}

/**
 * Parse a string into a sequence of token
 */
char* strtok_r(char *str, const char *delim, char **context)
{
	char *ret;

	if (str == NULL) {
		str = *context;
	}

	str += strspn(str, delim);

	if (*str == '\0') {
		return NULL;
	}

	ret = str;

	str += strcspn(str, delim);

	if (*str) {
		*str++ = '\0';
	}

	*context = str;

	return ret;
}

/**
 * vsscanf - Unformat a buffer into a list of arguments
 * @buf:	input buffer
 * @fmt:	format of buffer
 * @args:	arguments
 */
int vsscanf(const char *buf, const char *fmt, va_list args)
{
	const char *str = buf;
	char *next;
	char digit;
	int num = 0;
	u8 qualifier;
	unsigned int base;
	union {
		long long s;
		unsigned long long u;
	} val;
	s16 field_width;
	bool is_sign;

	while (*fmt) {
		/* skip any white space in format */
		/* white space in format matchs any amount of
		 * white space, including none, in the input.
		 */
		if (isspace(*fmt)) {
			fmt = skip_spaces(++fmt);
			str = skip_spaces(str);
		}

		/* anything that is not a conversion must match exactly */
		if (*fmt != '%' && *fmt) {
			if (*fmt++ != *str++)
				break;
			continue;
		}

		if (!*fmt)
			break;
		++fmt;

		/* skip this conversion.
		 * advance both strings to next white space
		 */
		if (*fmt == '*') {
			if (!*str)
				break;
			while (!isspace(*fmt) && *fmt != '%' && *fmt)
				fmt++;
			while (!isspace(*str) && *str)
				str++;
			continue;
		}

		/* get field width */
		field_width = -1;
		if (isdigit(*fmt)) {
			field_width = skip_atoi(&fmt);
			if (field_width <= 0)
				break;
		}

		/* get conversion qualifier */
		qualifier = -1;
		if (*fmt == 'h' || _tolower(*fmt) == 'l' ||
		    _tolower(*fmt) == 'z') {
			qualifier = *fmt++;
			if (unlikely(qualifier == *fmt)) {
				if (qualifier == 'h') {
					qualifier = 'H';
					fmt++;
				} else if (qualifier == 'l') {
					qualifier = 'L';
					fmt++;
				}
			}
		}

		if (!*fmt)
			break;

		if (*fmt == 'n') {
			/* return number of characters read so far */
			*va_arg(args, int *) = str - buf;
			++fmt;
			continue;
		}

		if (!*str)
			break;

		base = 10;
		is_sign = false;

		switch (*fmt++) {
		case 'c':
		{
			char *s = (char *)va_arg(args, char*);
			if (field_width == -1)
				field_width = 1;
			do {
				*s++ = *str++;
			} while (--field_width > 0 && *str);
			num++;
		}
		continue;
		case 's':
		{
			char *s = (char *)va_arg(args, char *);
			if (field_width == -1)
				field_width = SHRT_MAX;
			/* first, skip leading white space in buffer */
			str = skip_spaces(str);

			/* now copy until next white space */
			while (*str && !isspace(*str) && field_width--)
				*s++ = *str++;
			*s = '\0';
			num++;
		}
		continue;
		case 'o':
			base = 8;
			break;
		case 'x':
		case 'X':
			base = 16;
			break;
		case 'i':
			base = 0;
		case 'd':
			is_sign = true;
		case 'u':
			break;
		case '%':
			/* looking for '%' in str */
			if (*str++ != '%')
				return num;
			continue;
		default:
			/* invalid format; stop here */
			return num;
		}

		/* have some sort of integer conversion.
		 * first, skip white space in buffer.
		 */
		str = skip_spaces(str);

		digit = *str;
		if (is_sign && digit == '-')
			digit = *(str + 1);

		if (!digit
		    || (base == 16 && !isxdigit(digit))
		    || (base == 10 && !isdigit(digit))
		    || (base == 8 && (!isdigit(digit) || digit > '7'))
		    || (base == 0 && !isdigit(digit)))
			break;

		if (is_sign)
			val.s = qualifier != 'L' ?
				strtol(str, &next, base) :
				strtoll(str, &next, base);
		else
			val.u = qualifier != 'L' ?
				strtoul(str, &next, base) :
				strtoull(str, &next, base);

		if (field_width > 0 && next - str > field_width) {
			if (base == 0)
				_parse_integer_fixup_radix(str, &base);
			while (next - str > field_width) {
				if (is_sign)
					val.s = sdiv64(val.s, base);
				else
					val.u = udiv64(val.u, base);
				--next;
			}
		}

		switch (qualifier) {
		case 'H':	/* that's 'hh' in format */
			if (is_sign)
				*va_arg(args, signed char *) = val.s;
			else
				*va_arg(args, unsigned char *) = val.u;
			break;
		case 'h':
			if (is_sign)
				*va_arg(args, short *) = val.s;
			else
				*va_arg(args, unsigned short *) = val.u;
			break;
		case 'l':
			if (is_sign)
				*va_arg(args, long *) = val.s;
			else
				*va_arg(args, unsigned long *) = val.u;
			break;
		case 'L':
			if (is_sign)
				*va_arg(args, long long *) = val.s;
			else
				*va_arg(args, unsigned long long *) = val.u;
			break;
		case 'Z':
		case 'z':
			*va_arg(args, size_t *) = val.u;
			break;
		default:
			if (is_sign)
				*va_arg(args, int *) = val.s;
			else
				*va_arg(args, unsigned int *) = val.u;
			break;
		}
		num++;

		if (!next)
			break;
		str = next;
	}

	return num;
}
VMM_EXPORT_SYMBOL(vsscanf);

/**
 * sscanf - Unformat a buffer into a list of arguments
 * @buf:	input buffer
 * @fmt:	formatting of buffer
 * @...:	resulting arguments
 */
int sscanf(const char *buf, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vsscanf(buf, fmt, args);
	va_end(args);

	return i;
}
VMM_EXPORT_SYMBOL(sscanf);
