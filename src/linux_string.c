/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementations, from the documented Linux behaviour, of
 * string helpers that the illumos kernel does not (portably) provide:
 * strim, strsep, strchrnul, strnchr, strrchr, strlcat, match_string, a
 * small sscanf, and the uuid/guid helpers.
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/uuid.h>
#include <linux/random.h>
#include <linux/stdarg.h>

char *
lkpi_strim(char *s)
{
	size_t n;
	char *end;

	while (isspace((unsigned char)*s))
		s++;
	n = strlen(s);
	if (n == 0)
		return (s);
	end = s + n - 1;
	while (end >= s && isspace((unsigned char)*end))
		end--;
	end[1] = '\0';
	return (s);
}

char *
lkpi_strsep(char **sp, const char *delim)
{
	char *s = *sp, *p;

	if (s == NULL)
		return (NULL);
	for (p = s; *p != '\0'; p++) {
		if (strchr(delim, *p) != NULL) {
			*p = '\0';
			*sp = p + 1;
			return (s);
		}
	}
	*sp = NULL;
	return (s);
}

char *
lkpi_strchrnul(const char *s, int c)
{
	while (*s != '\0' && *s != (char)c)
		s++;
	return ((char *)(uintptr_t)s);
}

char *
lkpi_strnchr(const char *s, size_t count, int c)
{
	for (; count > 0 && *s != '\0'; count--, s++) {
		if (*s == (char)c)
			return ((char *)(uintptr_t)s);
	}
	return (NULL);
}

char *
lkpi_strrchr(const char *s, int c)
{
	const char *last = NULL;

	do {
		if (*s == (char)c)
			last = s;
	} while (*s++ != '\0');
	return ((char *)(uintptr_t)last);
}

/* Appends src to dst of size bytes; returns the length it tried to make. */
size_t
lkpi_strlcat(char *dst, const char *src, size_t size)
{
	size_t dl = strnlen(dst, size), sl = strlen(src), n;

	if (dl == size)
		return (size + sl);
	n = sl < size - dl - 1 ? sl : size - dl - 1;
	memcpy(dst + dl, src, n);
	dst[dl + n] = '\0';
	return (dl + sl);
}

/* Index of s in array (n entries, a NULL entry ends early), or -EINVAL. */
int
lkpi_match_string(const char * const *array, size_t n, const char *s)
{
	size_t i;

	for (i = 0; i < n && array[i] != NULL; i++) {
		if (strcmp(array[i], s) == 0)
			return ((int)i);
	}
	return (-EINVAL);
}

/*
 * sscanf subset: whitespace, literal characters, and conversions
 * %d %i %u %x %o %s %c %n with the hh, h, l and ll length modifiers and
 * an optional field width. Returns the number of assignments.
 */
int
lkpi_sscanf(const char *buf, const char *fmt, ...)
{
	va_list ap;
	const char *s = buf;
	int nassigned = 0;

	va_start(ap, fmt);
	while (*fmt != '\0') {
		int len = 0, base, neg = 0;
		unsigned int width = 0;
		unsigned long long v = 0;
		const char *start;

		if (isspace((unsigned char)*fmt)) {
			while (isspace((unsigned char)*s))
				s++;
			fmt++;
			continue;
		}
		if (*fmt != '%' || fmt[1] == '%') {
			if (*fmt == '%')
				fmt++;
			if (*s != *fmt)
				break;
			s++;
			fmt++;
			continue;
		}
		fmt++;
		while (*fmt >= '0' && *fmt <= '9')
			width = width * 10 + (unsigned int)(*fmt++ - '0');
		while (*fmt == 'h' || *fmt == 'l' || *fmt == 'z') {
			len += (*fmt == 'h') ? -1 : 1;
			fmt++;
		}
		if (*fmt == 'n') {
			*va_arg(ap, int *) = (int)(s - buf);
			fmt++;
			continue;
		}
		if (width == 0)
			width = ~0U;
		if (*fmt == 'c') {
			if (*s == '\0')
				break;
			*va_arg(ap, char *) = *s++;
			nassigned++;
			fmt++;
			continue;
		}
		while (isspace((unsigned char)*s))
			s++;
		if (*fmt == 's') {
			char *out = va_arg(ap, char *);

			if (*s == '\0')
				break;
			while (*s != '\0' && !isspace((unsigned char)*s) &&
			    width-- > 0)
				*out++ = *s++;
			*out = '\0';
			nassigned++;
			fmt++;
			continue;
		}
		switch (*fmt) {
		case 'd': case 'u': base = 10; break;
		case 'x': case 'X': base = 16; break;
		case 'o': base = 8; break;
		case 'i': base = 0; break;
		default: goto out;
		}
		if ((*s == '-' || *s == '+') && width > 0) {
			neg = (*s == '-');
			s++;
			width--;
		}
		if (base == 0) {
			if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
				base = 16;
			else if (s[0] == '0')
				base = 8;
			else
				base = 10;
		}
		if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X') &&
		    width > 2) {
			s += 2;
			width -= 2;
		}
		start = s;
		while (width > 0 && isxdigit((unsigned char)*s)) {
			int d = isdigit((unsigned char)*s) ? *s - '0' :
			    tolower((unsigned char)*s) - 'a' + 10;

			if (d >= base)
				break;
			v = v * (unsigned int)base + (unsigned int)d;
			s++;
			width--;
		}
		if (s == start)
			break;
		if (neg)
			v = -v;
		if (*fmt == 'd' || *fmt == 'i') {
			if (len <= -2)
				*va_arg(ap, signed char *) = (signed char)v;
			else if (len == -1)
				*va_arg(ap, short *) = (short)v;
			else if (len == 0)
				*va_arg(ap, int *) = (int)v;
			else if (len == 1)
				*va_arg(ap, long *) = (long)v;
			else
				*va_arg(ap, long long *) = (long long)v;
		} else {
			if (len <= -2)
				*va_arg(ap, unsigned char *) = (unsigned char)v;
			else if (len == -1)
				*va_arg(ap, unsigned short *) = (unsigned short)v;
			else if (len == 0)
				*va_arg(ap, unsigned int *) = (unsigned int)v;
			else if (len == 1)
				*va_arg(ap, unsigned long *) = (unsigned long)v;
			else
				*va_arg(ap, unsigned long long *) = v;
		}
		nassigned++;
		fmt++;
	}
out:
	va_end(ap);
	return (nassigned);
}

/* "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" */
bool
uuid_is_valid(const char *uuid)
{
	unsigned int i;

	for (i = 0; i < UUID_STRING_LEN; i++) {
		if (i == 8 || i == 13 || i == 18 || i == 23) {
			if (uuid[i] != '-')
				return (false);
		} else if (!isxdigit((unsigned char)uuid[i])) {
			return (false);
		}
	}
	return (true);
}

/* Random (version 4, RFC 4122 variant) identifiers. */
void
uuid_gen(uuid_t *u)
{
	get_random_bytes(u->b, sizeof (u->b));
	u->b[6] = (u8)((u->b[6] & 0x0f) | 0x40);
	u->b[8] = (u8)((u->b[8] & 0x3f) | 0x80);
}

void
guid_gen(guid_t *g)
{
	get_random_bytes(g->b, sizeof (g->b));
	g->b[7] = (u8)((g->b[7] & 0x0f) | 0x40);	/* little-endian field 3 */
	g->b[8] = (u8)((g->b[8] & 0x3f) | 0x80);
}
