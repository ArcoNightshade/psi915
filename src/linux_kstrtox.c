/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * String-to-integer helpers (see <linux/kstrtox.h>). Clean-room.
 */

#include <linux/kernel.h>
#include <linux/kstrtox.h>
#include <linux/limits.h>
#include <linux/errno.h>
#include <linux/uaccess.h>

static int
lkpi_digit(char c)
{
	if (c >= '0' && c <= '9')
		return (c - '0');
	if (c >= 'a' && c <= 'z')
		return (c - 'a' + 10);
	if (c >= 'A' && c <= 'Z')
		return (c - 'A' + 10);
	return (99);
}

/* Resolve the base and skip its prefix. */
static const char *
lkpi_base(const char *s, unsigned int *base)
{
	if (*base == 0) {
		if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X') &&
		    lkpi_digit(s[2]) < 16) {
			*base = 16;
			return (s + 2);
		}
		*base = s[0] == '0' ? 8 : 10;
		return (s);
	}
	if (*base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
		return (s + 2);
	return (s);
}

/*
 * Parse digits; *end is the first unparsed character. Returns the number
 * of digits, and sets *ovf on overflow of 64 bits.
 */
static unsigned int
lkpi_parse(const char *s, unsigned int base, unsigned long long *res,
    const char **end, bool *ovf)
{
	unsigned long long v = 0;
	unsigned int n = 0;
	int d;

	*ovf = false;
	while ((d = lkpi_digit(*s)) < (int)base) {
		if (v > (ULLONG_MAX - (unsigned long long)d) / base)
			*ovf = true;
		v = v * base + (unsigned long long)d;
		s++;
		n++;
	}
	*res = v;
	*end = s;
	return (n);
}

static int
lkpi_kstrtoull(const char *s, unsigned int base, unsigned long long *res)
{
	unsigned long long v;
	const char *end;
	bool ovf;

	if (base == 1 || base > 36)
		return (-EINVAL);
	s = lkpi_base(s, &base);
	if (lkpi_parse(s, base, &v, &end, &ovf) == 0)
		return (-EINVAL);
	if (*end == '\n')
		end++;
	if (*end != '\0')
		return (-EINVAL);
	if (ovf)
		return (-ERANGE);
	*res = v;
	return (0);
}

int
kstrtoull(const char *s, unsigned int base, unsigned long long *res)
{
	if (s[0] == '+')
		s++;
	return (lkpi_kstrtoull(s, base, res));
}

int
kstrtoll(const char *s, unsigned int base, long long *res)
{
	unsigned long long v;
	int r;

	if (s[0] == '-') {
		r = lkpi_kstrtoull(s + 1, base, &v);
		if (r != 0)
			return (r);
		if (v > (unsigned long long)LLONG_MAX + 1)
			return (-ERANGE);
		*res = (long long)(0 - v);
		return (0);
	}
	r = kstrtoull(s, base, &v);
	if (r != 0)
		return (r);
	if (v > (unsigned long long)LLONG_MAX)
		return (-ERANGE);
	*res = (long long)v;
	return (0);
}

#define	LKPI_KSTRTO_U(name, type, max)					\
int									\
name(const char *s, unsigned int base, type *res)			\
{									\
	unsigned long long v;						\
	int r = kstrtoull(s, base, &v);					\
									\
	if (r != 0)							\
		return (r);						\
	if (v > (unsigned long long)(max))				\
		return (-ERANGE);					\
	*res = (type)v;							\
	return (0);							\
}

#define	LKPI_KSTRTO_S(name, type, min, max)				\
int									\
name(const char *s, unsigned int base, type *res)			\
{									\
	long long v;							\
	int r = kstrtoll(s, base, &v);					\
									\
	if (r != 0)							\
		return (r);						\
	if (v < (long long)(min) || v > (long long)(max))		\
		return (-ERANGE);					\
	*res = (type)v;							\
	return (0);							\
}

LKPI_KSTRTO_U(kstrtoul, unsigned long, ULONG_MAX)
LKPI_KSTRTO_U(kstrtouint, unsigned int, UINT_MAX)
LKPI_KSTRTO_U(kstrtou16, u16, 0xffff)
LKPI_KSTRTO_U(kstrtou8, u8, 0xff)
LKPI_KSTRTO_S(kstrtol, long, LONG_MIN, LONG_MAX)
LKPI_KSTRTO_S(kstrtoint, int, INT_MIN, INT_MAX)
LKPI_KSTRTO_S(kstrtos16, s16, -32768, 32767)
LKPI_KSTRTO_S(kstrtos8, s8, -128, 127)

int
kstrtobool(const char *s, bool *res)
{
	if (s == NULL)
		return (-EINVAL);
	switch (s[0]) {
	case 'y': case 'Y': case '1':
		*res = true;
		return (0);
	case 'n': case 'N': case '0':
		*res = false;
		return (0);
	case 'o': case 'O':
		if (s[1] == 'n' || s[1] == 'N') {
			*res = true;
			return (0);
		}
		if (s[1] == 'f' || s[1] == 'F') {
			*res = false;
			return (0);
		}
		break;
	default:
		break;
	}
	return (-EINVAL);
}

/* Copy at most 63 bytes of user text and NUL-terminate it. */
static int
lkpi_from_user(const char __user *s, size_t count, char *buf, size_t len)
{
	if (count >= len)
		count = len - 1;
	if (copy_from_user(buf, s, count) != 0)
		return (-EFAULT);
	buf[count] = '\0';
	return (0);
}

int
kstrtoull_from_user(const char __user *s, size_t count, unsigned int base,
    unsigned long long *res)
{
	char buf[64];
	int r = lkpi_from_user(s, count, buf, sizeof (buf));

	return (r != 0 ? r : kstrtoull(buf, base, res));
}

int
kstrtoint_from_user(const char __user *s, size_t count, unsigned int base,
    int *res)
{
	char buf[64];
	int r = lkpi_from_user(s, count, buf, sizeof (buf));

	return (r != 0 ? r : kstrtoint(buf, base, res));
}

int
kstrtouint_from_user(const char __user *s, size_t count, unsigned int base,
    unsigned int *res)
{
	char buf[64];
	int r = lkpi_from_user(s, count, buf, sizeof (buf));

	return (r != 0 ? r : kstrtouint(buf, base, res));
}

int
kstrtobool_from_user(const char __user *s, size_t count, bool *res)
{
	char buf[8];
	int r = lkpi_from_user(s, count, buf, sizeof (buf));

	return (r != 0 ? r : kstrtobool(buf, res));
}

unsigned long long
simple_strtoull(const char *cp, char **endp, unsigned int base)
{
	unsigned long long v;
	const char *end;
	bool ovf;

	cp = lkpi_base(cp, &base);
	(void) lkpi_parse(cp, base, &v, &end, &ovf);
	if (endp != NULL)
		*endp = (char *)(uintptr_t)end;
	return (v);
}

long long
simple_strtoll(const char *cp, char **endp, unsigned int base)
{
	if (*cp == '-')
		return (-(long long)simple_strtoull(cp + 1, endp, base));
	return ((long long)simple_strtoull(cp, endp, base));
}

unsigned long
simple_strtoul(const char *cp, char **endp, unsigned int base)
{
	return ((unsigned long)simple_strtoull(cp, endp, base));
}

long
simple_strtol(const char *cp, char **endp, unsigned int base)
{
	return ((long)simple_strtoll(cp, endp, base));
}
