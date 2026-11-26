/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * kernel API (linux/string.h); cross-checked against FreeBSD's LinuxKPI
 * string.h (BSD-2-Clause). The C string functions (memcpy, memset,
 * strlen, ...) come from the environment: the illumos kernel provides
 * them in genunix, userland from libc. kstrdup() and friends are in
 * src/linux_slab.c.
 */
#ifndef _LINUXKPI_LINUX_STRING_H_
#define	_LINUXKPI_LINUX_STRING_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/gfp.h>
#include <linux/errno.h>

char *kstrdup(const char *s, gfp_t gfp);
char *kstrndup(const char *s, size_t max, gfp_t gfp);
const char *kstrdup_const(const char *s, gfp_t gfp);
void kfree_const(const void *p);
void *kmemdup(const void *src, size_t len, gfp_t gfp);
char *kmemdup_nul(const char *s, size_t len, gfp_t gfp);
void *kmemdup_array(const void *src, size_t count, size_t elsize, gfp_t gfp);

/*
 * Copy a NUL-terminated string into dst of size bytes, always terminating
 * (if size > 0). Returns the length copied, or -E2BIG on truncation.
 */
static inline ssize_t
lkpi_strscpy(char *dst, const char *src, size_t size)
{
	size_t i;

	if (size == 0)
		return (-E2BIG);
	for (i = 0; i < size; i++) {
		dst[i] = src[i];
		if (src[i] == '\0')
			return ((ssize_t)i);
	}
	dst[size - 1] = '\0';
	return (-E2BIG);
}

/* strscpy, then zero the rest of dst. */
static inline ssize_t
lkpi_strscpy_pad(char *dst, const char *src, size_t size)
{
	ssize_t r = lkpi_strscpy(dst, src, size);
	size_t used = r < 0 ? size : (size_t)r + 1;

	if (used < size)
		memset(dst + used, 0, size - used);
	return (r);
}

/*
 * strscpy(dst, src) (dst an array: its size) or strscpy(dst, src, size),
 * as in v6.14.
 */
#define	__lkpi_strscpy_sel(_1, _2, _3, n, ...)	n
#define	__lkpi_strscpy2(d, s)		lkpi_strscpy((d), (s), sizeof (d))
#define	__lkpi_strscpy3(d, s, n)	lkpi_strscpy((d), (s), (n))
#define	strscpy(...)							\
	__lkpi_strscpy_sel(__VA_ARGS__, __lkpi_strscpy3,		\
	    __lkpi_strscpy2, 0)(__VA_ARGS__)
#define	__lkpi_strscpy_pad2(d, s)	lkpi_strscpy_pad((d), (s), sizeof (d))
#define	__lkpi_strscpy_pad3(d, s, n)	lkpi_strscpy_pad((d), (s), (n))
#define	strscpy_pad(...)						\
	__lkpi_strscpy_sel(__VA_ARGS__, __lkpi_strscpy_pad3,		\
	    __lkpi_strscpy_pad2, 0)(__VA_ARGS__)

static inline void
memset32(uint32_t *s, uint32_t v, size_t count)
{
	while (count-- > 0)
		*s++ = v;
}

static inline void
memset64(uint64_t *s, uint64_t v, size_t count)
{
	while (count-- > 0)
		*s++ = v;
}

static inline void
memset_p(void **p, void *v, size_t count)
{
	while (count-- > 0)
		*p++ = v;
}

/* First byte in [start, start + bytes) that is not c, or NULL. */
static inline void *
memchr_inv(const void *start, int c, size_t bytes)
{
	const uint8_t *p = start;

	for (; bytes > 0; bytes--, p++)
		if (*p != (uint8_t)c)
			return ((void *)(uintptr_t)p);
	return (NULL);
}

/* memset that the compiler may not elide (clearing secrets). */
static inline void
memzero_explicit(void *s, size_t count)
{
	memset(s, 0, count);
	__asm__ __volatile__("" : : "r"(s) : "memory");
}

/* Length of prefix if str starts with it, else 0. */
static inline size_t
str_has_prefix(const char *str, const char *prefix)
{
	size_t len = strlen(prefix);

	return (strncmp(str, prefix, len) == 0 ? len : 0);
}

static inline bool
strstarts(const char *str, const char *prefix)
{
	return (strncmp(str, prefix, strlen(prefix)) == 0);
}

/* True if the size bytes at p are all zero. */
static inline bool
mem_is_zero(const void *p, size_t size)
{
	return (memchr_inv(p, 0, size) == NULL);
}

/*
 * String helpers not guaranteed by the illumos kernel (src/linux_string.c);
 * the lkpi_ names avoid clashes with libc in userland.
 */
char *lkpi_strim(char *s);
char *lkpi_strsep(char **sp, const char *delim);
char *lkpi_strchrnul(const char *s, int c);
char *lkpi_strnchr(const char *s, size_t count, int c);
char *lkpi_strrchr(const char *s, int c);
size_t lkpi_strlcat(char *dst, const char *src, size_t size);
int lkpi_match_string(const char * const *array, size_t n, const char *s);
int lkpi_sscanf(const char *buf, const char *fmt, ...)
    __attribute__((format(scanf, 2, 3)));
#define	strim(s)		lkpi_strim(s)
#define	strstrip(s)		lkpi_strim(s)
#define	strsep(sp, d)		lkpi_strsep((sp), (d))
#define	strchrnul(s, c)		lkpi_strchrnul((s), (c))
#define	strnchr(s, n, c)	lkpi_strnchr((s), (n), (c))
#define	strrchr(s, c)		lkpi_strrchr((s), (c))
#define	strlcat(d, s, n)	lkpi_strlcat((d), (s), (n))
#define	sscanf(...)		lkpi_sscanf(__VA_ARGS__)
#define	match_string(a, n, s)	lkpi_match_string((a), (n), (s))
#define	sysfs_match_string(a, s)	lkpi_match_string((a), ARRAY_SIZE(a), (s))

/* Copy a string into a fixed-size non-NUL-terminated field, padding. */
#define	strtomem_pad(dest, src, pad)	do {				\
	size_t __lkpi_n = strnlen((src), sizeof (dest));		\
	memcpy((dest), (src), __lkpi_n);				\
	memset((char *)(dest) + __lkpi_n, (pad), sizeof (dest) - __lkpi_n); \
} while (0)
#define	strtomem(dest, src)						\
	memcpy((dest), (src), strnlen((src), sizeof (dest)))

#include <linux/string_choices.h>

#endif /* _LINUXKPI_LINUX_STRING_H_ */
