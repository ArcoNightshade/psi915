/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. ASCII character classes (Linux semantics, locale independent).
 */
#ifndef _LINUXKPI_LINUX_CTYPE_H_
#define	_LINUXKPI_LINUX_CTYPE_H_

#include <linux/types.h>

static inline int lkpi_isdigit(int c) { return (c >= '0' && c <= '9'); }
static inline int lkpi_islower(int c) { return (c >= 'a' && c <= 'z'); }
static inline int lkpi_isupper(int c) { return (c >= 'A' && c <= 'Z'); }
static inline int lkpi_isalpha(int c) { return (lkpi_islower(c) || lkpi_isupper(c)); }
static inline int lkpi_isalnum(int c) { return (lkpi_isalpha(c) || lkpi_isdigit(c)); }
static inline int lkpi_isxdigit(int c) { return (lkpi_isdigit(c) || ((c | 0x20) >= 'a' && (c | 0x20) <= 'f')); }
static inline int lkpi_isspace(int c) { return (c == ' ' || (c >= '\t' && c <= '\r')); }
static inline int lkpi_iscntrl(int c) { return ((c >= 0 && c < 32) || c == 127); }
static inline int lkpi_isprint(int c) { return (c >= 32 && c < 127); }
static inline int lkpi_isgraph(int c) { return (c > 32 && c < 127); }
static inline int lkpi_ispunct(int c) { return (lkpi_isgraph(c) && !lkpi_isalnum(c)); }
static inline int lkpi_tolower(int c) { return (lkpi_isupper(c) ? c + 32 : c); }
static inline int lkpi_toupper(int c) { return (lkpi_islower(c) ? c - 32 : c); }

#undef isdigit
#undef islower
#undef isupper
#undef isalpha
#undef isalnum
#undef isxdigit
#undef isspace
#undef iscntrl
#undef isprint
#undef isgraph
#undef ispunct
#undef tolower
#undef toupper
#define	isdigit(c)	lkpi_isdigit(c)
#define	islower(c)	lkpi_islower(c)
#define	isupper(c)	lkpi_isupper(c)
#define	isalpha(c)	lkpi_isalpha(c)
#define	isalnum(c)	lkpi_isalnum(c)
#define	isxdigit(c)	lkpi_isxdigit(c)
#define	isspace(c)	lkpi_isspace(c)
#define	iscntrl(c)	lkpi_iscntrl(c)
#define	isprint(c)	lkpi_isprint(c)
#define	isgraph(c)	lkpi_isgraph(c)
#define	ispunct(c)	lkpi_ispunct(c)
#define	tolower(c)	lkpi_tolower(c)
#define	toupper(c)	lkpi_toupper(c)
#define	_tolower(c)	((c) | 0x20)

#ifndef isascii
#define	isascii(c)	(((unsigned char)(c)) <= 0x7f)
#endif

#endif /* _LINUXKPI_LINUX_CTYPE_H_ */
