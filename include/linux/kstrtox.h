/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of the Linux string-to-integer helpers from
 * their documented behaviour (src/linux_kstrtox.c):
 *
 *   kstrto*()      whole string must be a number: optional '+' (or '-'
 *                  for signed types), base prefix for base 0 ("0x" hex,
 *                  "0" octal, else decimal) or "0x" for base 16, at least
 *                  one digit, then optionally one '\n'. 0, -EINVAL or
 *                  -ERANGE; *res is untouched on error.
 *   simple_strto*() parse as far as possible, never fail (deprecated).
 *   kstrtobool()   y/Y/1/on -> true, n/N/0/off -> false.
 */
#ifndef _LINUXKPI_LINUX_KSTRTOX_H_
#define	_LINUXKPI_LINUX_KSTRTOX_H_

#include <linux/compiler.h>
#include <linux/types.h>

int __must_check kstrtoull(const char *s, unsigned int base,
    unsigned long long *res);
int __must_check kstrtoll(const char *s, unsigned int base, long long *res);
int __must_check kstrtoul(const char *s, unsigned int base,
    unsigned long *res);
int __must_check kstrtol(const char *s, unsigned int base, long *res);
int __must_check kstrtouint(const char *s, unsigned int base,
    unsigned int *res);
int __must_check kstrtoint(const char *s, unsigned int base, int *res);
int __must_check kstrtou16(const char *s, unsigned int base, u16 *res);
int __must_check kstrtos16(const char *s, unsigned int base, s16 *res);
int __must_check kstrtou8(const char *s, unsigned int base, u8 *res);
int __must_check kstrtos8(const char *s, unsigned int base, s8 *res);
int __must_check kstrtobool(const char *s, bool *res);

#define	kstrtou64(s, b, r)	kstrtoull(s, b, (unsigned long long *)(r))
#define	kstrtos64(s, b, r)	kstrtoll(s, b, (long long *)(r))
#define	kstrtou32(s, b, r)	kstrtouint(s, b, (unsigned int *)(r))
#define	kstrtos32(s, b, r)	kstrtoint(s, b, (int *)(r))

int __must_check kstrtoint_from_user(const char __user *s, size_t count,
    unsigned int base, int *res);
int __must_check kstrtouint_from_user(const char __user *s, size_t count,
    unsigned int base, unsigned int *res);
int __must_check kstrtoull_from_user(const char __user *s, size_t count,
    unsigned int base, unsigned long long *res);
int __must_check kstrtobool_from_user(const char __user *s, size_t count,
    bool *res);

unsigned long long simple_strtoull(const char *cp, char **endp,
    unsigned int base);
long long simple_strtoll(const char *cp, char **endp, unsigned int base);
unsigned long simple_strtoul(const char *cp, char **endp, unsigned int base);
long simple_strtol(const char *cp, char **endp, unsigned int base);

#endif /* _LINUXKPI_LINUX_KSTRTOX_H_ */
